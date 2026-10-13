#include "profile/ProfileMove.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <climits>

namespace heap::profilemove {

namespace {

// The columns every profile understands whether or not it lists them: a task
// in one of these is data, never re-filed (as applyProfileToModels reads it).
const QSet<QString>& builtInStatuses() {
  static const QSet<QString> kBuiltIn = {QStringLiteral("backlog"),
                                         QStringLiteral("todo"),
                                         QStringLiteral("prog"),
                                         QStringLiteral("half"),
                                         QStringLiteral("blocked"),
                                         QStringLiteral("review"),
                                         QStringLiteral("done")};
  return kBuiltIn;
}

bool holds(const QVector<Task>& tasks, const QString& id) {
  return std::any_of(tasks.cbegin(), tasks.cend(), [&id](const Task& t) {
    return t.id == id;
  });
}

bool isUrl(const QString& target) {
  return target.contains(QStringLiteral("://"));
}

// One profile's own text and links, rewritten with `remap` (old id -> new).
void rewriteProfileRefs(Profile& p, const QHash<QString, QString>& remap) {
  if(remap.isEmpty()) {
    return;
  }
  for(Task& t : p.tasks) {
    rewriteTaskRefs(t.desc, remap);
    rewriteTaskRefs(t.local.notes, remap);
    for(TaskLink& l : t.links) {
      l.targetId = remap.value(l.targetId, l.targetId);
    }
  }
  bool activeTouched = false;
  for(Note& n : p.notes) {
    if(rewriteTaskRefs(n.body, remap) && n.id == p.activeNoteId) {
      activeTouched = true;
    }
  }
  // `notesState` is the active note's body under another name; it says what
  // the note says.
  if(activeTouched) {
    for(const Note& n : std::as_const(p.notes)) {
      if(n.id == p.activeNoteId) {
        p.notesState = n.body;
      }
    }
  }
  for(DocPage& d : p.docPages) {
    rewriteTaskRefs(d.body, remap);
  }
}

}  // namespace

bool rewriteTaskRefs(QString& text, const QHash<QString, QString>& remap) {
  if(remap.isEmpty() || (!text.contains(QLatin1Char('#')) && !text.contains(QStringLiteral("[[")))) {
    return false;
  }
  // '#KEY-1' as the markdown renderer reads a task mention, and [[KEY-1]],
  // [[KEY-1|alias]], [[KEY-1#heading]] as a wiki link to it.
  static const QRegularExpression kRef(
      QStringLiteral("(?<![A-Za-z0-9_])#([A-Za-z][A-Za-z0-9_]*-\\d+)(?![A-Za-z0-9_])"
                     "|\\[\\[\\s*([^\\]|#\\n]+?)\\s*(?=[\\]|#])"));
  QString out;
  qsizetype last = 0;
  bool changed = false;
  for(auto it = kRef.globalMatch(text); it.hasNext();) {
    const QRegularExpressionMatch m = it.next();
    const int group = m.capturedStart(1) >= 0 ? 1 : 2;
    const auto hit = remap.constFind(m.captured(group));
    if(hit == remap.constEnd()) {
      continue;
    }
    out += QStringView(text).mid(last, m.capturedStart(group) - last);
    out += hit.value();
    last = m.capturedEnd(group);
    changed = true;
  }
  if(changed) {
    out += QStringView(text).mid(last);
    text = out;
  }
  return changed;
}

Result moveTasks(QVector<Profile>& profiles,
                 int fromIdx,
                 int toIdx,
                 const QStringList& ids,
                 QVector<CalEvent>& events,
                 heap::history::TaskHistory* history,
                 const Options& opt) {
  Result r;
  const int n = static_cast<int>(profiles.size());
  if(fromIdx < 0 || toIdx < 0 || fromIdx >= n || toIdx >= n || fromIdx == toIdx || ids.isEmpty()) {
    return r;
  }
  Profile& from = profiles[fromIdx];
  Profile& to = profiles[toIdx];

  // What leaves, in the source's order. A tracker card stays.
  const QSet<QString> wanted(ids.cbegin(), ids.cend());
  QVector<Task> leaving;
  for(int i = 0; i < from.tasks.size(); ++i) {
    const Task& t = from.tasks.at(i);
    if(!wanted.contains(t.id)) {
      continue;
    }
    if(!t.externalProvider.isEmpty()) {
      r.trackerKept.append(t.id);
      continue;
    }
    r.moved.append(t.id);
    r.rows.insert(t.id, i);
    leaving.append(t);
  }
  if(leaving.isEmpty()) {
    return r;
  }
  const QSet<QString> movedSet(r.moved.cbegin(), r.moved.cend());
  from.tasks.erase(std::remove_if(from.tasks.begin(),
                                  from.tasks.end(),
                                  [&movedSet](const Task& t) {
                                    return movedSet.contains(t.id);
                                  }),
                   from.tasks.end());

  // The ids they have in the target: their own unless it is taken there.
  QSet<QString> taken;
  for(const Task& t : std::as_const(to.tasks)) {
    taken.insert(t.id);
  }
  for(Task& t : leaving) {
    const QString old = t.id;
    QString id = opt.preferredIds.value(old, old);
    if(taken.contains(id) && id != old && !taken.contains(old)) {
      id = old;
    }
    for(int guard = 0; taken.contains(id) && guard < 10000; ++guard) {
      id = opt.mint ? opt.mint(t) : QString();
      if(id.isEmpty()) {
        id = old + QStringLiteral("-") + QString::number(guard + 2);
      }
    }
    taken.insert(id);
    if(id != old) {
      r.renamed.insert(old, id);
    }
    t.id = id;
    r.movedTo.append(id);
  }

  // A column the target does not have: its first column, as a load files an
  // unknown status (A2-2). The undo hands the old one back.
  QSet<QString> columns;
  QString firstColumn;
  for(const QVariant& v : std::as_const(to.statuses)) {
    const QString id = v.toMap().value(QStringLiteral("id")).toString();
    columns.insert(id);
    if(firstColumn.isEmpty()) {
      firstColumn = id;
    }
  }
  for(int i = 0; i < leaving.size(); ++i) {
    Task& t = leaving[i];
    const auto forced = opt.forcedStatus.constFind(t.id);
    if(forced != opt.forcedStatus.constEnd()) {
      t.status = forced.value();
      continue;
    }
    if(!firstColumn.isEmpty() && !columns.contains(t.status) && !builtInStatuses().contains(t.status)) {
      r.statusBefore.insert(r.moved.at(i), t.status);
      r.statusAfter.insert(t.id, firstColumn);
      t.status = firstColumn;
    }
  }

  // What points at a renumbered task follows it, in a profile where the old
  // id cannot mean another task: the source (it no longer holds the old id)
  // yes, the target (whose own task has that id) no.
  QHash<QString, QString> fromRemap;
  QHash<QString, QString> toRemap;
  for(auto it = r.renamed.cbegin(); it != r.renamed.cend(); ++it) {
    if(!holds(from.tasks, it.key()) && !holds(from.tasks, it.value())) {
      fromRemap.insert(it.key(), it.value());
    }
    if(!holds(to.tasks, it.key()) && !holds(to.tasks, it.value())) {
      toRemap.insert(it.key(), it.value());
    }
  }
  rewriteProfileRefs(from, fromRemap);
  rewriteProfileRefs(to, toRemap);

  // Related links (APP-239) carry the profile they point into.
  const auto newId = [&r](const QString& id) {
    return r.renamed.value(id, id);
  };
  for(Task& t : from.tasks) {
    for(LocalLink& l : t.local.related) {
      if(l.profileId.isEmpty() && movedSet.contains(l.target)) {
        l.target = newId(l.target);
        l.profileId = to.id;
      }
    }
  }
  for(Task& t : to.tasks) {
    for(LocalLink& l : t.local.related) {
      if(l.profileId == from.id && movedSet.contains(l.target)) {
        l.target = newId(l.target);
        l.profileId.clear();
      }
    }
  }
  for(int p = 0; p < n; ++p) {
    if(p == fromIdx || p == toIdx) {
      continue;
    }
    for(Task& t : profiles[p].tasks) {
      for(LocalLink& l : t.local.related) {
        if(l.profileId == from.id && movedSet.contains(l.target)) {
          l.target = newId(l.target);
          l.profileId = to.id;
        }
      }
    }
  }
  // The moved tasks' own references: to each other by the new ids, to what
  // stayed behind by that profile.
  for(Task& t : leaving) {
    rewriteTaskRefs(t.desc, r.renamed);
    rewriteTaskRefs(t.local.notes, r.renamed);
    for(TaskLink& l : t.links) {
      l.targetId = newId(l.targetId);
    }
    for(LocalLink& l : t.local.related) {
      if(isUrl(l.target)) {
        continue;
      }
      if(l.profileId.isEmpty()) {
        if(movedSet.contains(l.target)) {
          l.target = newId(l.target);
        } else {
          l.profileId = from.id;
        }
      } else if(l.profileId == to.id) {
        l.profileId.clear();
      }
    }
  }

  // Into the target: back at their rows (the undo), else at the end.
  QVector<int> order(leaving.size());
  for(int i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
    const int ra = opt.rows.value(leaving.at(a).id, INT_MAX);
    const int rb = opt.rows.value(leaving.at(b).id, INT_MAX);
    return ra < rb;
  });
  for(const int i : std::as_const(order)) {
    const Task& t = leaving.at(i);
    const auto row = opt.rows.constFind(t.id);
    if(row == opt.rows.constEnd()) {
      to.tasks.append(t);
    } else {
      to.tasks.insert(qBound(0, row.value(), static_cast<int>(to.tasks.size())), t);
    }
  }

  // A meeting of the source's linked to a moved task goes with it, so its
  // Task row does not point at a task that is no longer there (F1-new-9).
  // The undo, a move back, brings it back the same way.
  QSet<int> wentAlong;
  for(int i = 0; i < events.size(); ++i) {
    CalEvent& e = events[i];
    if(e.profileId == from.id && !e.taskId.isEmpty() && movedSet.contains(e.taskId)) {
      e.profileId = to.id;
      e.taskId = newId(e.taskId);
      wentAlong.insert(i);
    }
  }
  // Meetings linked to a renumbered task.
  for(int i = 0; i < events.size(); ++i) {
    if(wentAlong.contains(i)) {
      continue;
    }
    CalEvent& e = events[i];
    const auto hit = r.renamed.constFind(e.taskId);
    if(hit == r.renamed.constEnd()) {
      continue;
    }
    // A meeting of the target's that names its own task of that id stays.
    if(e.profileId == to.id && !toRemap.contains(hit.key())) {
      continue;
    }
    e.taskId = hit.value();
  }

  // Its status moves and its waiting-on line go with it.
  QVector<StatusChange> keptLog;
  for(const StatusChange& c : std::as_const(from.statusLog)) {
    if(movedSet.contains(c.taskId)) {
      StatusChange moved = c;
      moved.taskId = newId(c.taskId);
      to.statusLog.append(moved);
    } else {
      keptLog.append(c);
    }
  }
  if(keptLog.size() != from.statusLog.size()) {
    from.statusLog = keptLog;
    std::stable_sort(to.statusLog.begin(), to.statusLog.end(), [](const StatusChange& a, const StatusChange& b) {
      return a.at < b.at;
    });
  }
  QVector<WaitingOn> keptWaiting;
  for(const WaitingOn& w : std::as_const(from.waitingOn)) {
    if(movedSet.contains(w.taskId)) {
      WaitingOn moved = w;
      moved.taskId = newId(w.taskId);
      to.waitingOn.append(moved);
    } else {
      keptWaiting.append(w);
    }
  }
  from.waitingOn = keptWaiting;

  // Its history: the same events under the new key, and one line that says
  // where it came from. The undo takes that line back.
  if(history != nullptr) {
    for(int i = 0; i < r.moved.size(); ++i) {
      history->rekey(from.id, r.moved.at(i), to.id, r.movedTo.at(i));
      if(opt.recordHistory) {
        history->append(to.id,
                        r.movedTo.at(i),
                        heap::history::HistoryEvent{.at = opt.now.isValid() ? opt.now : QDateTime::currentDateTime(),
                                                    .kind = QStringLiteral("profile"),
                                                    .from = from.name,
                                                    .to = to.name,
                                                    .sync = false});
      } else {
        history->dropLast(to.id, r.movedTo.at(i), QStringLiteral("profile"));
      }
    }
  }
  return r;
}

}  // namespace heap::profilemove
