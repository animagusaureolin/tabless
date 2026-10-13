#include "platform/Brand.h"
#include "platform/LegacyData.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>

namespace heap::platform::legacy {

namespace {

const QString kState = QStringLiteral("state.json");

bool isLockFile(const QString& name) {
  return name.endsWith(QLatin1String(".lock")) || name.endsWith(QLatin1String(".lock.rmlock"));
}

// True while an old install holds the old folder's lock: copying then could
// catch a state.json half-way through a save, and the old app would go on
// writing to a folder nobody reads any more.
bool legacyRunning(const QString& legacyDir) {
  for(const heap::brand::Legacy& l : heap::brand::kLegacy) {
    QLockFile lock(QDir(legacyDir).filePath(QLatin1String(l.lockFile)));
    lock.setStaleLockTime(0);
    if(lock.tryLock(0)) {
      lock.unlock();
    } else if(lock.error() == QLockFile::LockFailedError) {
      return true;
    }
  }
  return false;
}

bool isMarker(const QString& rel) {
  if(rel == QLatin1String(heap::brand::kMovedMarker)) {
    return true;
  }
  for(const heap::brand::Legacy& l : heap::brand::kLegacy) {
    if(l.movedMarker != nullptr && rel == QLatin1String(l.movedMarker)) {
      return true;
    }
  }
  return false;
}

// Moved once already — to Tabless, or (a heap folder) to lowkey before. A
// new folder losing its state.json later (wiped by hand) is not a reason to
// bring the old one back.
bool alreadyMoved(const QString& legacyDir) {
  const QDir dir(legacyDir);
  if(QFileInfo::exists(dir.filePath(QLatin1String(heap::brand::kMovedMarker)))) {
    return true;
  }
  for(const heap::brand::Legacy& l : heap::brand::kLegacy) {
    if(l.movedMarker != nullptr && QFileInfo::exists(dir.filePath(QLatin1String(l.movedMarker)))) {
      return true;
    }
  }
  return false;
}

// "lowkey" for …/lowkey/lowkey: the folder's own name.
QString nameOf(const QString& legacyDir) {
  return QFileInfo(QDir::cleanPath(legacyDir)).fileName();
}

bool copyTree(const QString& from, const QString& to, int* files, QString* error) {
  QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
  const QDir src(from);
  while(it.hasNext()) {
    const QString path = it.next();
    const QString rel = src.relativeFilePath(path);
    if(isLockFile(QFileInfo(path).fileName()) || isMarker(rel)) {
      continue;
    }
    const QString target = QDir(to).filePath(rel);
    QDir().mkpath(QFileInfo(target).absolutePath());
    if(!QFile::copy(path, target)) {
      *error = QStringLiteral("could not copy %1").arg(QDir::toNativeSeparators(path));
      return false;
    }
    ++*files;
  }
  return true;
}

// Copy beside the new folder, then move each top-level entry in: a copy
// that stops half-way leaves the new folder without a state.json, so the
// next launch simply tries again. False (and r->kind = Failed) on an error.
bool copyInto(const QString& newDir, const QString& fromDir, MoveResult* r) {
  const QString staging = QDir::cleanPath(newDir) + QStringLiteral(".moving");
  QDir(staging).removeRecursively();
  if(!copyTree(fromDir, staging, &r->files, &r->error)) {
    QDir(staging).removeRecursively();
    r->kind = MoveKind::Failed;
    return false;
  }
  QDir().mkpath(newDir);
  const QDir in(staging);
  // state.json last: until it is there, the move has not happened.
  QStringList entries = in.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
  entries.removeAll(kState);
  entries.append(kState);
  for(const QString& name : entries) {
    const QString target = QDir(newDir).filePath(name);
    if(QFileInfo::exists(target)) {
      if(QFileInfo(target).isDir() && QFileInfo(in.filePath(name)).isDir()) {
        // A folder this launch already made (logs): merge file by file.
        QDirIterator files(in.filePath(name), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while(files.hasNext()) {
          const QString f = files.next();
          const QString to = QDir(target).filePath(QDir(in.filePath(name)).relativeFilePath(f));
          QDir().mkpath(QFileInfo(to).absolutePath());
          if(!QFileInfo::exists(to)) {
            QFile::rename(f, to);
          }
        }
      }
      continue;  // what this launch wrote stays
    }
    if(!QFile::rename(in.filePath(name), target) && !QDir().rename(in.filePath(name), target)) {
      r->kind = MoveKind::Failed;
      r->error = QStringLiteral("could not move %1 into %2").arg(name, QDir::toNativeSeparators(newDir));
      QDir(staging).removeRecursively();
      return false;
    }
  }
  QDir(staging).removeRecursively();
  return true;
}

}  // namespace

QStringList legacyDirsFor(const QString& newDir) {
  // By the string, not QDir::cdUp(): on a first launch the new folder does
  // not exist yet, and cdUp() refuses to leave a folder that is not there.
  QStringList parts = QDir::cleanPath(QDir(newDir).absolutePath()).split(QLatin1Char('/'));
  if(parts.size() < 3 || parts.at(parts.size() - 1).compare(QLatin1String(heap::brand::kName), Qt::CaseInsensitive) != 0 ||
     parts.at(parts.size() - 2).compare(QLatin1String(heap::brand::kName), Qt::CaseInsensitive) != 0) {
    return {};
  }
  QStringList dirs;
  for(const heap::brand::Legacy& l : heap::brand::kLegacy) {
    parts[parts.size() - 1] = QLatin1String(l.name);
    parts[parts.size() - 2] = QLatin1String(l.name);
    dirs.append(parts.join(QLatin1Char('/')));
  }
  return dirs;
}

MoveResult moveLegacyData(const QString& newDir, const QString& legacyDir) {
  MoveResult r;
  r.from = legacyDir;
  r.to = newDir;
  r.legacyName = nameOf(legacyDir);
  if(legacyDir.isEmpty() || QFileInfo::exists(QDir(newDir).filePath(kState)) || !QFileInfo::exists(QDir(legacyDir).filePath(kState))) {
    return r;
  }
  if(alreadyMoved(legacyDir)) {
    return r;
  }
  if(legacyRunning(legacyDir)) {
    r.kind = MoveKind::Busy;
    return r;
  }

  if(!copyInto(newDir, legacyDir, &r)) {
    return r;
  }

  QSaveFile marker(QDir(legacyDir).filePath(QLatin1String(heap::brand::kMovedMarker)));
  if(marker.open(QIODevice::WriteOnly)) {
    marker.write(QStringLiteral("%1 is now Tabless. On %2 everything in this folder was copied to\n%3\n"
                                "and Tabless works from there. This folder is kept as it was; you can delete it once\n"
                                "you no longer need to go back to %1.\n")
                     .arg(r.legacyName, QDateTime::currentDateTime().toString(Qt::ISODate), QDir::toNativeSeparators(newDir))
                     .toUtf8());
    marker.commit();
  }
  r.kind = MoveKind::Moved;
  return r;
}

MoveResult moveLegacyData(const QString& newDir) {
  for(const QString& dir : legacyDirsFor(newDir)) {
    MoveResult r = moveLegacyData(newDir, dir);
    if(r.kind != MoveKind::NothingToDo) {
      return r;
    }
  }
  MoveResult none;
  none.to = newDir;
  return none;
}

QString stableDirFor(const QString& channelDir) {
  QStringList parts = QDir::cleanPath(QDir(channelDir).absolutePath()).split(QLatin1Char('/'));
  const QString prefix = QLatin1String(heap::brand::kName) + QLatin1Char('-');
  if(parts.size() < 3 || !parts.at(parts.size() - 1).startsWith(prefix, Qt::CaseInsensitive) ||
     parts.at(parts.size() - 2).compare(QLatin1String(heap::brand::kName), Qt::CaseInsensitive) != 0) {
    return {};
  }
  parts[parts.size() - 1] = QLatin1String(heap::brand::kName);
  return parts.join(QLatin1Char('/'));
}

MoveResult copyStableData(const QString& channelDir) {
  MoveResult r;
  r.from = stableDirFor(channelDir);
  r.to = channelDir;
  r.legacyName = QLatin1String(heap::brand::kName);
  if(r.from.isEmpty() || QFileInfo::exists(QDir(channelDir).filePath(kState)) || !QFileInfo::exists(QDir(r.from).filePath(kState))) {
    return r;
  }
  if(!copyInto(channelDir, r.from, &r)) {
    return r;
  }
  // Left by the user's Tabless while it runs; here it would read as a crash.
  QFile::remove(QDir(channelDir).filePath(QStringLiteral("session.open")));
  r.kind = MoveKind::Moved;
  return r;
}

}  // namespace heap::platform::legacy
