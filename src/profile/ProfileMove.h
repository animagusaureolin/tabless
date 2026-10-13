#pragma once

#include "Models.h"

#include "history/TaskHistory.h"

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

// Moving tasks from one profile to another (A5-14). A pure function of the
// profiles, the event pool and the history, so the move and its undo are the
// same code run in opposite directions, and both can be tested without an
// AppController.
//
// A task takes everything of its own with it: its history, checklist, local
// layer and timer sessions (all inside the Task or keyed by it), its status
// moves and its waiting-on line. A local id the target profile already holds
// is renumbered, and what pointed at the old id follows it: '#ID' and [[ID]]
// in notes, pages and descriptions, dependency and related links, and the
// meetings linked to it. A tracker card never moves: it belongs to the
// profile whose integration pulled it.
namespace heap::profilemove {

struct Options {
  // id in `from` -> the id it should have in `to`; taken while `to` holds no
  // other task under it (the undo's way back to the old ids).
  QHash<QString, QString> preferredIds;
  // id in `to` -> the row it goes in at (the undo puts a task back where it
  // was); without one, it is appended.
  QHash<QString, int> rows;
  // id in `to` -> the status it gets (the undo's way back to a column the
  // move had to change).
  QHash<QString, QString> forcedStatus;
  // A fresh id for a task whose id `to` already holds. Required.
  std::function<QString(const Task&)> mint;
  // When the move happens, for its line in the task's history.
  QDateTime now;
  // False for the undo: the history line is taken back instead.
  bool recordHistory = true;
};

struct Result {
  QStringList moved;                     // ids as they were in `from`, in its order
  QStringList trackerKept;               // tracker cards asked for, left where they are
  QStringList movedTo;                   // the same tasks' ids in `to`, in the same order
  QHash<QString, QString> renamed;       // id in `from` -> id in `to`, where it changed
  QHash<QString, int> rows;              // id in `from` -> its row there
  QHash<QString, QString> statusBefore;  // id in `from` -> its status, where the move changed it
  QHash<QString, QString> statusAfter;   // id in `to` -> the status it got there
};

// Moves the tasks `ids` of profiles[fromIdx] into profiles[toIdx]. Ids that
// are not in `from` are ignored; tracker cards are reported in trackerKept.
// `events` is the global pool, `history` may be null.
Result moveTasks(QVector<Profile>& profiles,
                 int fromIdx,
                 int toIdx,
                 const QStringList& ids,
                 QVector<CalEvent>& events,
                 heap::history::TaskHistory* history,
                 const Options& opt);

// '#OLD' and [[OLD]] (with an alias or a heading after it) become the new id.
// Returns whether anything changed.
bool rewriteTaskRefs(QString& text, const QHash<QString, QString>& remap);

}  // namespace heap::profilemove
