// Moving tasks between profiles (A5-14): the pure move (profile/ProfileMove.h)
// and the AppController invokable with its undo across both profiles. The
// controller cases boot against the QStandardPaths test-mode profile from an
// empty data directory, like test_storage_safety.

#include "AppController.h"
#include "StateSerializer.h"

#include "profile/ProfileMove.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

namespace pm = heap::profilemove;

Task task(const QString& id, const QString& status = QStringLiteral("todo")) {
  Task t;
  t.id = id;
  t.title = id + QStringLiteral(" title");
  t.status = status;
  t.priority = QStringLiteral("P2");
  return t;
}

QVariantList columns(std::initializer_list<const char*> ids) {
  QVariantList out;
  for(const char* id : ids) {
    out.append(QVariantMap{{"id", QString::fromLatin1(id)}, {"name", QString::fromLatin1(id)}});
  }
  return out;
}

Profile profile(const QString& id, QVector<Task> tasks) {
  Profile p;
  p.id = id;
  p.name = id.toUpper();
  p.tasks = std::move(tasks);
  p.statuses = columns({"backlog", "todo", "prog", "done"});
  return p;
}

int rowOf(const QVector<Task>& tasks, const QString& id) {
  for(int i = 0; i < tasks.size(); ++i) {
    if(tasks.at(i).id == id) {
      return i;
    }
  }
  return -1;
}

pm::Options minting(int* counter) {
  pm::Options o;
  o.mint = [counter](const Task&) {
    return QStringLiteral("NEW-%1").arg(++*counter);
  };
  o.now = QDateTime(QDate(2026, 10, 13), QTime(10, 0));
  return o;
}

}  // namespace

TEST(ProfileMovePure, TheTaskTakesItsOwnThingsWithIt) {
  Task t = task("A-1");
  t.local.checklist.append(LocalCheckItem{});
  t.local.sessions.append(TimerSession{});
  t.local.notes = QStringLiteral("my notes");
  QVector<Profile> ps{profile("a", {task("A-0"), t}), profile("b", {task("B-1")})};
  ps[0].statusLog.append(StatusChange{"A-1", "todo", "prog", QDateTime::currentDateTime()});
  ps[0].statusLog.append(StatusChange{"A-0", "todo", "prog", QDateTime::currentDateTime()});
  ps[0].waitingOn.append(WaitingOn{"A-1", "olga", QDateTime::currentDateTime(), {}});
  heap::history::TaskHistory history;
  history.append("a", "A-1", heap::history::HistoryEvent{QDateTime::currentDateTime(), "created", {}, {}, false});
  QVector<CalEvent> events;
  int n = 0;
  const pm::Result r = pm::moveTasks(ps, 0, 1, {"A-1"}, events, &history, minting(&n));

  ASSERT_EQ(r.moved, QStringList{"A-1"});
  EXPECT_TRUE(r.renamed.isEmpty());
  EXPECT_EQ(rowOf(ps[0].tasks, "A-1"), -1);
  const int at = rowOf(ps[1].tasks, "A-1");
  ASSERT_GE(at, 0);
  EXPECT_EQ(ps[1].tasks.at(at).local, t.local) << "checklist, notes and sessions travel";
  EXPECT_EQ(ps[0].statusLog.size(), 1);
  EXPECT_EQ(ps[1].statusLog.size(), 1);
  EXPECT_EQ(ps[1].waitingOn.size(), 1);
  EXPECT_TRUE(ps[0].waitingOn.isEmpty());
  const auto events1 = history.events("b", "A-1");
  ASSERT_EQ(events1.size(), 2) << "its history, plus one line saying where it came from";
  EXPECT_EQ(events1.last().kind, QStringLiteral("profile"));
  EXPECT_EQ(events1.last().from, QStringLiteral("A"));
  EXPECT_EQ(events1.last().to, QStringLiteral("B"));
  EXPECT_EQ(history.count("a", "A-1"), 0);
}

TEST(ProfileMovePure, ATrackerCardStays) {
  Task card = task("jira-APP-1");
  card.externalProvider = QStringLiteral("jira");
  card.externalId = QStringLiteral("APP-1");
  QVector<Profile> ps{profile("a", {card, task("A-2")}), profile("b", {})};
  QVector<CalEvent> events;
  int n = 0;
  const pm::Result r = pm::moveTasks(ps, 0, 1, {"jira-APP-1", "A-2"}, events, nullptr, minting(&n));
  EXPECT_EQ(r.trackerKept, QStringList{"jira-APP-1"});
  EXPECT_EQ(r.moved, QStringList{"A-2"});
  EXPECT_GE(rowOf(ps[0].tasks, "jira-APP-1"), 0);
}

TEST(ProfileMovePure, AClashingIdIsRenumberedAndWhatPointedAtItFollows) {
  Task moving = task("X-5");
  Task stays = task("A-9");
  stays.desc = QStringLiteral("see #X-5 and #X-50, not #X-5x");
  stays.links.append(TaskLink{"blocks", "X-5"});
  stays.local.related.append(LocalLink{"r1", "related", "X-5", {}});
  Note note;
  note.id = QStringLiteral("n1");
  note.body = QStringLiteral("- [[X-5]] and [[X-5|the thing]] and [[X-5#Plan]]");
  Profile a = profile("a", {moving, stays});
  a.notes.append(note);
  a.activeNoteId = QStringLiteral("n1");
  a.notesState = note.body;
  Task theirs = task("X-5");
  theirs.desc = QStringLiteral("our own #X-5");
  Profile b = profile("b", {theirs});
  QVector<Profile> ps{a, b};
  CalEvent linked;
  linked.id = QStringLiteral("e1");
  linked.taskId = QStringLiteral("X-5");
  linked.profileId = QStringLiteral("a");
  CalEvent theirsEvent;
  theirsEvent.id = QStringLiteral("e2");
  theirsEvent.taskId = QStringLiteral("X-5");
  theirsEvent.profileId = QStringLiteral("b");
  QVector<CalEvent> events{linked, theirsEvent};
  int n = 0;
  const pm::Result r = pm::moveTasks(ps, 0, 1, {"X-5"}, events, nullptr, minting(&n));

  ASSERT_EQ(r.renamed.value("X-5"), QStringLiteral("NEW-1"));
  EXPECT_GE(rowOf(ps[1].tasks, "NEW-1"), 0);
  EXPECT_GE(rowOf(ps[1].tasks, "X-5"), 0) << "the target's own task keeps its id";
  const Task& after = ps[0].tasks.at(rowOf(ps[0].tasks, "A-9"));
  EXPECT_EQ(after.desc, QStringLiteral("see #NEW-1 and #X-50, not #X-5x"));
  EXPECT_EQ(after.links.at(0).targetId, QStringLiteral("NEW-1"));
  EXPECT_EQ(after.local.related.at(0).target, QStringLiteral("NEW-1"));
  EXPECT_EQ(after.local.related.at(0).profileId, QStringLiteral("b"));
  EXPECT_EQ(ps[0].notes.at(0).body, QStringLiteral("- [[NEW-1]] and [[NEW-1|the thing]] and [[NEW-1#Plan]]"));
  EXPECT_EQ(ps[0].notesState, ps[0].notes.at(0).body);
  EXPECT_EQ(ps[1].tasks.at(rowOf(ps[1].tasks, "X-5")).desc, QStringLiteral("our own #X-5")) << "the target's text means its own task";
  EXPECT_EQ(events.at(0).taskId, QStringLiteral("NEW-1"));
  EXPECT_EQ(events.at(1).taskId, QStringLiteral("X-5")) << "the target's meeting names its own task";
}

TEST(ProfileMovePure, AColumnTheTargetLacksBecomesItsFirstAndTheUndoGivesItBack) {
  QVector<Profile> ps{profile("a", {task("A-1", "waiting")}), profile("b", {})};
  ps[0].statuses = columns({"waiting", "todo"});
  ps[1].statuses = columns({"inbox", "doing"});
  QVector<CalEvent> events;
  int n = 0;
  const pm::Result r = pm::moveTasks(ps, 0, 1, {"A-1"}, events, nullptr, minting(&n));
  EXPECT_EQ(ps[1].tasks.at(0).status, QStringLiteral("inbox"));
  EXPECT_EQ(r.statusBefore.value("A-1"), QStringLiteral("waiting"));

  pm::Options back = minting(&n);
  back.forcedStatus.insert("A-1", "waiting");
  back.rows.insert("A-1", 0);
  pm::moveTasks(ps, 1, 0, {"A-1"}, events, nullptr, back);
  EXPECT_EQ(ps[0].tasks.at(0).status, QStringLiteral("waiting"));
}

TEST(ProfileMovePure, BackAndForthIsLossless) {
  Task t = task("X-5");
  t.desc = QStringLiteral("mine");
  Task other = task("A-2");
  other.desc = QStringLiteral("#X-5");
  QVector<Profile> ps{profile("a", {task("A-0"), t, other}), profile("b", {task("X-5")})};
  const QVector<Profile> start = ps;
  heap::history::TaskHistory history;
  QVector<CalEvent> events;
  int n = 0;
  const pm::Result r = pm::moveTasks(ps, 0, 1, {"X-5"}, events, &history, minting(&n));
  pm::Options back = minting(&n);
  back.preferredIds.insert(r.renamed.value("X-5"), "X-5");
  back.rows.insert("X-5", r.rows.value("X-5"));
  back.recordHistory = false;
  pm::moveTasks(ps, 1, 0, {r.renamed.value("X-5")}, events, &history, back);
  EXPECT_EQ(ps[0].tasks, start[0].tasks);
  EXPECT_EQ(ps[1].tasks, start[1].tasks);
  EXPECT_TRUE(history.events("a", "X-5").isEmpty()) << "the undo takes the history line back";
}

// ── the controller ──

namespace {

QString appDataDir() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QJsonObject taskJson(const QString& id, const QJsonObject& extra = {}) {
  QJsonObject o{{"id", id}, {"title", id + QStringLiteral(" title")}, {"status", "todo"}, {"priority", "P2"}};
  for(auto it = extra.begin(); it != extra.end(); ++it) {
    o.insert(it.key(), it.value());
  }
  return o;
}

QJsonObject profileJson(const QString& id, const QJsonArray& tasks) {
  QJsonArray statuses;
  for(const char* s : {"backlog", "todo", "prog", "blocked", "review", "done"}) {
    statuses.append(QJsonObject{{"id", s}, {"name", s}, {"color", "#888888"}});
  }
  return QJsonObject{{"id", id}, {"name", id.toUpper()}, {"color", "#5cc2dd"}, {"tasks", tasks}, {"statuses", statuses}};
}

void writeState(const QJsonArray& profiles, const QString& active) {
  QDir(appDataDir()).removeRecursively();
  QDir().mkpath(appDataDir());
  QJsonObject root;
  root["schemaVersion"] = heap::state::kSchemaVersion;
  root["activeProfileId"] = active;
  root["profiles"] = profiles;
  root["events"] = QJsonArray();
  root["settings"] = QJsonObject{{"welcomeSeen", true}};
  QFile f(appDataDir() + QStringLiteral("/state.json"));
  ASSERT_TRUE(f.open(QIODevice::WriteOnly));
  f.write(QJsonDocument(root).toJson());
}

bool has(AppController& app, const QString& id) {
  return app.tasks()->indexOfId(id) >= 0;
}

}  // namespace

TEST(ProfileMoveApp, MovesAndOneUndoInEitherProfileBringsItBack) {
  writeState({profileJson("a", {taskJson("TASK-1"), taskJson("TASK-2")}), profileJson("b", {taskJson("TASK-3")})}, "a");
  AppController app;
  app.setLanguage(QStringLiteral("en"));
  const QVariantMap r = app.moveTasksToProfile({"TASK-1"}, "b");
  EXPECT_EQ(r.value("moved").toInt(), 1);
  EXPECT_FALSE(has(app, "TASK-1"));
  ASSERT_TRUE(app.hasPendingUndo());

  // Undo from the target.
  app.setActiveProfileId("b");
  EXPECT_TRUE(has(app, "TASK-1"));
  ASSERT_TRUE(app.hasPendingUndo()) << "the move is undoable in the profile it went to";
  app.undo();
  EXPECT_FALSE(has(app, "TASK-1"));
  app.setActiveProfileId("a");
  EXPECT_TRUE(has(app, "TASK-1"));
  EXPECT_EQ(app.tasks()->indexOfId("TASK-1"), 0) << "back at its row";
  EXPECT_FALSE(app.hasPendingUndo()) << "the source's copy of the step went with it";

  // Again, undone where it started; then redone.
  app.moveTasksToProfile({"TASK-1", "TASK-2"}, "b");
  EXPECT_EQ(app.tasks()->rowCount(), 0);
  app.setActiveProfileId("b");
  app.setActiveProfileId("a");
  ASSERT_TRUE(app.hasPendingUndo()) << "switching profiles does not lose it";
  app.undo();
  EXPECT_TRUE(has(app, "TASK-1"));
  EXPECT_TRUE(has(app, "TASK-2"));
  app.redo();
  EXPECT_FALSE(has(app, "TASK-1"));
  app.setActiveProfileId("b");
  EXPECT_TRUE(has(app, "TASK-1"));
  EXPECT_TRUE(has(app, "TASK-2"));
  app.undo();
  EXPECT_FALSE(has(app, "TASK-2"));
}

TEST(ProfileMoveApp, UndoOfAMoveWhoseTaskIsGoneSaysSoAndKeepsTheOlderSteps) {
  writeState({profileJson("a", {taskJson("TASK-1"), taskJson("TASK-2")}), profileJson("b", {})}, "a");
  AppController app;
  app.setLanguage(QStringLiteral("en"));
  QStringList toasts;
  QObject::connect(&app, &AppController::toast, [&toasts](const QString& m) {
    toasts.append(m);
  });
  app.toggleDone({"TASK-2"});  // an older step in the source
  ASSERT_TRUE(app.hasPendingUndo());
  app.moveTasksToProfile({"TASK-1"}, "b");
  app.setActiveProfileId("b");
  app.deleteTask(QStringLiteral("TASK-1"));
  ASSERT_FALSE(has(app, "TASK-1"));
  app.setActiveProfileId("a");

  toasts.clear();
  app.undo();
  EXPECT_FALSE(has(app, "TASK-1"));
  ASSERT_FALSE(toasts.isEmpty()) << "a failed undo says so";
  EXPECT_TRUE(toasts.last().contains(QStringLiteral("changed since"))) << toasts.last().toStdString();
  EXPECT_TRUE(app.hasPendingUndo()) << "the older step is still there";

  // Redo has nothing to replay: the spent step is not in the redo region.
  app.redo();
  EXPECT_TRUE(app.hasPendingUndo()) << "redo after a failed undo does not wipe the history";
  app.undo();
  const int row = app.tasks()->indexOfId("TASK-2");
  ASSERT_GE(row, 0);
  EXPECT_NE(app.tasks()->items().at(row).status, QStringLiteral("done")) << "the older step undoes";
}

TEST(ProfileMoveApp, ATrackerCardRefusesWithAToast) {
  writeState({profileJson("a", {taskJson("jira-APP-7", {{"externalProvider", "jira"}, {"externalId", "APP-7"}})}), profileJson("b", {})},
             "a");
  AppController app;
  app.setLanguage(QStringLiteral("en"));
  QStringList toasts;
  QObject::connect(&app, &AppController::toast, [&toasts](const QString& m) {
    toasts.append(m);
  });
  const QVariantMap r = app.moveTasksToProfile({"jira-APP-7"}, "b");
  EXPECT_EQ(r.value("error").toString(), QStringLiteral("tracker"));
  EXPECT_TRUE(has(app, "jira-APP-7"));
  ASSERT_FALSE(toasts.isEmpty());
  EXPECT_TRUE(toasts.last().contains(QStringLiteral("integration"))) << toasts.last().toStdString();
  EXPECT_FALSE(app.hasPendingUndo());
}

TEST(ProfileMoveApp, TheSameProfileOrNoneIsRefused) {
  writeState({profileJson("a", {taskJson("TASK-1")}), profileJson("b", {})}, "a");
  AppController app;
  EXPECT_EQ(app.moveTasksToProfile({"TASK-1"}, "a").value("error").toString(), QStringLiteral("sameProfile"));
  EXPECT_EQ(app.moveTasksToProfile({"TASK-1"}, "zz").value("error").toString(), QStringLiteral("noProfile"));
  EXPECT_TRUE(has(app, "TASK-1"));
}

TEST(ProfileMoveApp, AMovedTaskIsSavedInTheTargetProfile) {
  writeState({profileJson("a", {taskJson("TASK-1")}), profileJson("b", {})}, "a");
  {
    AppController app;
    app.moveTasksToProfile({"TASK-1"}, "b");
    app.flushSave();
  }
  AppController reopened;
  EXPECT_FALSE(has(reopened, "TASK-1"));
  reopened.setActiveProfileId("b");
  EXPECT_TRUE(has(reopened, "TASK-1"));
}

// ── A5-21: the profile sigil in capture ──

TEST(ProfileSigil, TheWholeNameAnyCaseTheLongestWins) {
  const QStringList names{"Side", "Side project", "Inbox"};
  auto s = heap::capture::findProfileSigil("Write blog post +side PROJECT tomorrow", names);
  ASSERT_EQ(s.index, 1);
  EXPECT_EQ(s.text, QStringLiteral("+side PROJECT"));
  EXPECT_EQ(heap::capture::withoutSigil("Write blog post +side PROJECT tomorrow", s), QStringLiteral("Write blog post tomorrow"));
  EXPECT_EQ(heap::capture::findProfileSigil("+Inbox call mom", names).index, 2);
  EXPECT_EQ(heap::capture::withoutSigil("+Inbox call mom", heap::capture::findProfileSigil("+Inbox call mom", names)),
            QStringLiteral("call mom"));
}

TEST(ProfileSigil, AnUnknownWordAPartWordOrANoteIsText) {
  const QStringList names{"Side project", "Inbox"};
  EXPECT_EQ(heap::capture::findProfileSigil("learn C++ +Inboxes", names).index, -1);
  EXPECT_EQ(heap::capture::findProfileSigil("score a+Inbox", names).index, -1);
  EXPECT_EQ(heap::capture::findProfileSigil("call +bob", names).index, -1);
  EXPECT_EQ(heap::capture::findProfileSigil("task // for +Inbox", names).index, -1);
  EXPECT_EQ(heap::capture::findProfileSigil("task\n+Inbox", names).index, -1);
}

TEST(ProfileMoveApp, CaptureReadsTheSigilAndHashStaysALabel) {
  QJsonObject side = profileJson("side", {});
  side["name"] = QStringLiteral("Side project");
  writeState({profileJson("inbox", {}), side}, "inbox");
  AppController app;
  const QVariantMap p = app.captureParse(QStringLiteral("Write blog post +Side project tomorrow"), QDateTime::currentDateTime(), {});
  EXPECT_EQ(p.value("profileId").toString(), QStringLiteral("side"));
  EXPECT_EQ(p.value("profileName").toString(), QStringLiteral("Side project"));
  EXPECT_EQ(p.value("title").toString(), QStringLiteral("Write blog post"));
  bool marked = false;
  for(const QVariant& m : p.value("marks").toList()) {
    marked = marked || m.toMap().value("kind").toString() == QStringLiteral("profile");
  }
  EXPECT_TRUE(marked);
  const QVariantMap d = app.quickTaskDraft(QStringLiteral("Write blog post +Side project"), QDateTime::currentDateTime(), {});
  EXPECT_EQ(d.value("_targetProfileId").toString(), QStringLiteral("side"));
  EXPECT_EQ(d.value("title").toString(), QStringLiteral("Write blog post"));
  const QVariantMap hash = app.quickTaskDraft(QStringLiteral("Write #blog post"), QDateTime::currentDateTime(), {});
  EXPECT_TRUE(hash.value("_targetProfileId").toString().isEmpty());
  EXPECT_EQ(hash.value("labels").toStringList(), QStringList{"blog"});
  const QVariantMap unknown = app.quickTaskDraft(QStringLiteral("Write +blog post"), QDateTime::currentDateTime(), {});
  EXPECT_EQ(unknown.value("title").toString(), QStringLiteral("Write +blog post"));
}

// ── A1-17: Today with every profile ──

TEST(ProfileMoveApp, TodayWithEveryProfileLabelsTheOtherProfilesRows) {
  const QDate today = QDate::currentDate();
  const QString at = QDateTime(today, QTime(15, 0)).toString(Qt::ISODate);
  const QString day = QDateTime(today, QTime(0, 0)).toString(Qt::ISODate);
  QJsonObject other = profileJson(
      "home",
      {taskJson("HOME-1", {{"scheduledAt", at}, {"scheduledHasTime", true}}),
       taskJson("HOME-2", {{"dueAt", day}, {"dueHasTime", false}}),
       taskJson("HOME-3", {{"status", "prog"}}),
       taskJson("HOME-4", {{"dueAt", QDateTime(today.addDays(-3), QTime(0, 0)).toString(Qt::ISODate)}, {"dueHasTime", false}})});
  other["color"] = QStringLiteral("#c08040");
  writeState({profileJson("work", {taskJson("WORK-1", {{"scheduledAt", at}, {"scheduledHasTime", true}})}), other}, "work");
  AppController app;
  const QVariantMap mine = app.todayData(today, false);
  EXPECT_EQ(mine.value("blocks").toList().size(), 1);
  const QVariantMap all = app.todayData(today, true);
  bool foreignBlock = false;
  for(const QVariant& v : all.value("blocks").toList()) {
    const QVariantMap b = v.toMap();
    if(b.value("id").toString() == QStringLiteral("HOME-1")) {
      foreignBlock = true;
      EXPECT_EQ(b.value("profileId").toString(), QStringLiteral("home"));
      EXPECT_EQ(b.value("profileName").toString(), QStringLiteral("HOME"));
      EXPECT_EQ(b.value("profileColor").toString(), QStringLiteral("#c08040"));
    } else {
      EXPECT_TRUE(b.value("profileId").toString().isEmpty());
    }
  }
  EXPECT_TRUE(foreignBlock);
  bool foreignDue = false;
  for(const QVariant& v : all.value("deadlines").toList()) {
    foreignDue = foreignDue || v.toMap().value("profileId").toString() == QStringLiteral("home");
  }
  EXPECT_TRUE(foreignDue);
  EXPECT_EQ(all.value("undated").toInt(), mine.value("undated").toInt()) << "the undated link opens this profile's list";
  EXPECT_EQ(all.value("overdue").toList().size(), mine.value("overdue").toList().size())
      << "the overdue link opens this profile's list, so it counts this profile's";
  bool foreignProgress = false;
  for(const QVariant& v : all.value("inProgress").toList()) {
    foreignProgress = foreignProgress || v.toMap().value("profileName").toString() == QStringLiteral("HOME");
  }
  EXPECT_TRUE(foreignProgress) << "another profile's task in progress carries its profile";
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir scratch;
  scratch.setAutoRemove(true);
  qputenv("XDG_CONFIG_HOME", scratch.path().toUtf8());
  qputenv("XDG_DATA_HOME", scratch.path().toUtf8());
  QApplication qapp(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
