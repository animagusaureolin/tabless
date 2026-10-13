// lowkey → Tabless (APP-283), heap → lowkey (APP-280): the first launch
// copies an older data folder into the new one — once, all of it, the newest
// old name first, and never while the old app still runs on it.

#include "platform/Brand.h"
#include "platform/LegacyData.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

using heap::platform::legacy::legacyDirsFor;
using heap::platform::legacy::MoveKind;
using heap::platform::legacy::moveLegacyData;

void writeFile(const QString& path, const QByteArray& bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  ASSERT_TRUE(f.open(QIODevice::WriteOnly));
  f.write(bytes);
}

QByteArray readFile(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

class LegacyData : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(root_.isValid());
    newDir_ = root_.filePath(QStringLiteral("Tabless/Tabless"));
    oldDir_ = root_.filePath(QStringLiteral("lowkey/lowkey"));
    heapDir_ = root_.filePath(QStringLiteral("heap/heap"));
  }

  // An old folder: state, a backup, a time-machine snapshot, an attachment,
  // the file-fallback secrets and a lock file that must not travel.
  static void makeOldFolder(const QString& dir, const QByteArray& state = R"({"schemaVersion":12})") {
    writeFile(dir + "/state.json", state);
    writeFile(dir + "/backups/state-20261001-120000.json", "backup");
    writeFile(dir + "/history/2026-10-01T12-00.json.gz", "snapshot");
    writeFile(dir + "/attachments/0123456789abcdef0123456789abcdef.png", "png");
    writeFile(dir + "/secrets.json", "{}");
    writeFile(dir + "/logs/lowkey.log", "old log");
  }

  QTemporaryDir root_;
  QString newDir_;
  QString oldDir_;
  QString heapDir_;
};

}  // namespace

TEST_F(LegacyData, TheOldFoldersSitBesideTheNewOneNewestFirst) {
  const QStringList dirs = legacyDirsFor(newDir_);
  ASSERT_EQ(dirs.size(), 2);
  EXPECT_EQ(QDir::cleanPath(dirs.at(0)), QDir::cleanPath(oldDir_));
  EXPECT_EQ(QDir::cleanPath(dirs.at(1)), QDir::cleanPath(heapDir_));
  EXPECT_TRUE(legacyDirsFor(root_.filePath(QStringLiteral("somewhere/else"))).isEmpty())
      << "a folder that is not Tabless/Tabless has no old one beside it";
}

TEST_F(LegacyData, EverythingIsCopiedAndTheOldFolderStays) {
  makeOldFolder(oldDir_);
  const auto r = moveLegacyData(newDir_);
  ASSERT_EQ(r.kind, MoveKind::Moved) << r.error.toStdString();
  EXPECT_EQ(r.legacyName, QStringLiteral("lowkey"));
  EXPECT_EQ(readFile(newDir_ + "/state.json"), QByteArray(R"({"schemaVersion":12})"));
  EXPECT_EQ(readFile(newDir_ + "/backups/state-20261001-120000.json"), QByteArray("backup"));
  EXPECT_EQ(readFile(newDir_ + "/history/2026-10-01T12-00.json.gz"), QByteArray("snapshot"));
  EXPECT_EQ(readFile(newDir_ + "/attachments/0123456789abcdef0123456789abcdef.png"), QByteArray("png"));
  EXPECT_EQ(readFile(newDir_ + "/secrets.json"), QByteArray("{}"));
  // The old folder is untouched apart from the marker: every file still
  // there with the same bytes, so lowkey 0.8 can still be pointed at it.
  EXPECT_EQ(readFile(oldDir_ + "/state.json"), QByteArray(R"({"schemaVersion":12})"));
  EXPECT_EQ(readFile(oldDir_ + "/backups/state-20261001-120000.json"), QByteArray("backup"));
  EXPECT_EQ(readFile(oldDir_ + "/logs/lowkey.log"), QByteArray("old log"));
  EXPECT_TRUE(QFileInfo::exists(oldDir_ + "/" + QLatin1String(heap::brand::kMovedMarker)));
  EXPECT_FALSE(QFileInfo::exists(newDir_ + ".moving")) << "the staging folder is gone";
}

TEST_F(LegacyData, AHeapFolderMovesStraightToTabless) {
  makeOldFolder(heapDir_, R"({"schemaVersion":11})");
  const auto r = moveLegacyData(newDir_);
  ASSERT_EQ(r.kind, MoveKind::Moved) << r.error.toStdString();
  EXPECT_EQ(r.legacyName, QStringLiteral("heap"));
  EXPECT_EQ(readFile(newDir_ + "/state.json"), QByteArray(R"({"schemaVersion":11})"));
  EXPECT_TRUE(QFileInfo::exists(heapDir_ + "/" + QLatin1String(heap::brand::kMovedMarker)));
}

TEST_F(LegacyData, LowkeysFolderWinsOverTheHeapOneItCameFrom) {
  // 0.8 copied heap's folder into lowkey's and left its marker there.
  makeOldFolder(heapDir_, R"({"schemaVersion":11})");
  writeFile(heapDir_ + "/MOVED-TO-LOWKEY.txt", "moved");
  makeOldFolder(oldDir_, R"({"schemaVersion":12,"newer":true})");
  ASSERT_EQ(moveLegacyData(newDir_).kind, MoveKind::Moved);
  EXPECT_EQ(readFile(newDir_ + "/state.json"), QByteArray(R"({"schemaVersion":12,"newer":true})"));
  EXPECT_FALSE(QFileInfo::exists(newDir_ + "/MOVED-TO-LOWKEY.txt")) << "an old marker does not travel";
}

TEST_F(LegacyData, AHeapFolderAlreadyMovedToLowkeyIsNotBroughtBack) {
  // The lowkey folder was deleted by hand after 0.8 moved heap's into it.
  makeOldFolder(heapDir_, R"({"schemaVersion":11})");
  writeFile(heapDir_ + "/MOVED-TO-LOWKEY.txt", "moved");
  EXPECT_EQ(moveLegacyData(newDir_).kind, MoveKind::NothingToDo);
  EXPECT_FALSE(QFileInfo::exists(newDir_ + "/state.json"));
}

TEST_F(LegacyData, ASecondLaunchDoesNotCopyAgain) {
  makeOldFolder(oldDir_);
  ASSERT_EQ(moveLegacyData(newDir_).kind, MoveKind::Moved);
  writeFile(newDir_ + "/state.json", R"({"schemaVersion":13})");  // Tabless saved since
  writeFile(oldDir_ + "/state.json", R"({"schemaVersion":12,"changed":true})");
  EXPECT_EQ(moveLegacyData(newDir_).kind, MoveKind::NothingToDo);
  EXPECT_EQ(readFile(newDir_ + "/state.json"), QByteArray(R"({"schemaVersion":13})"));
}

TEST_F(LegacyData, AWipedNewFolderDoesNotBringTheOldDataBack) {
  makeOldFolder(oldDir_);
  ASSERT_EQ(moveLegacyData(newDir_).kind, MoveKind::Moved);
  QDir(newDir_).removeRecursively();
  EXPECT_EQ(moveLegacyData(newDir_).kind, MoveKind::NothingToDo);
  EXPECT_FALSE(QFileInfo::exists(newDir_ + "/state.json"));
}

TEST_F(LegacyData, ANewInstallWithoutAnOldOneDoesNothing) {
  EXPECT_EQ(moveLegacyData(newDir_).kind, MoveKind::NothingToDo);
  EXPECT_FALSE(QFileInfo::exists(newDir_ + "/state.json"));
}

TEST_F(LegacyData, NothingMovesWhileTheOldAppStillRunsOnItsFolder) {
  makeOldFolder(oldDir_);
  QLockFile held(oldDir_ + "/lowkey.lock");
  ASSERT_TRUE(held.tryLock(0));
  const auto busy = moveLegacyData(newDir_);
  EXPECT_EQ(busy.kind, MoveKind::Busy);
  EXPECT_EQ(busy.legacyName, QStringLiteral("lowkey"));
  EXPECT_FALSE(QFileInfo::exists(newDir_ + "/state.json"));
  held.unlock();
  EXPECT_EQ(moveLegacyData(newDir_).kind, MoveKind::Moved);
}

TEST_F(LegacyData, FilesThisLaunchAlreadyWroteAreKept) {
  makeOldFolder(oldDir_);
  writeFile(newDir_ + "/logs/tabless.log", "new log");  // the logger runs before the move
  ASSERT_EQ(moveLegacyData(newDir_).kind, MoveKind::Moved);
  EXPECT_EQ(readFile(newDir_ + "/logs/tabless.log"), QByteArray("new log"));
  EXPECT_EQ(readFile(newDir_ + "/logs/lowkey.log"), QByteArray("old log"));
}

// A channel build (APP-309) starts on a copy of the user's own Tabless folder
// and never writes to it.
TEST_F(LegacyData, AChannelBuildSitsBesideTheUsersOwnFolder) {
  using heap::platform::legacy::stableDirFor;
  EXPECT_EQ(QDir::cleanPath(stableDirFor(root_.filePath(QStringLiteral("Tabless/Tabless-feature")))), QDir::cleanPath(newDir_));
  EXPECT_TRUE(stableDirFor(newDir_).isEmpty()) << "the user's own folder is not a channel's";
  EXPECT_TRUE(stableDirFor(root_.filePath(QStringLiteral("somewhere/Tabless-feature"))).isEmpty());
  EXPECT_TRUE(legacyDirsFor(root_.filePath(QStringLiteral("Tabless/Tabless-feature"))).isEmpty())
      << "a channel build never moves a lowkey or heap folder";
}

TEST_F(LegacyData, AChannelBuildCopiesTheUsersFolderOnceAndLeavesItAlone) {
  using heap::platform::legacy::copyStableData;
  const QString channelDir = root_.filePath(QStringLiteral("Tabless/Tabless-feature"));
  makeOldFolder(newDir_, R"({"schemaVersion":14})");
  writeFile(newDir_ + "/session.open", "2026-10-13T16:00:00");  // the user's Tabless is running
  writeFile(newDir_ + "/tabless.lock", "lock");

  const auto r = copyStableData(channelDir);
  ASSERT_EQ(r.kind, MoveKind::Moved) << r.error.toStdString();
  EXPECT_EQ(readFile(channelDir + "/state.json"), QByteArray(R"({"schemaVersion":14})"));
  EXPECT_EQ(readFile(channelDir + "/attachments/0123456789abcdef0123456789abcdef.png"), QByteArray("png"));
  EXPECT_FALSE(QFileInfo::exists(channelDir + "/session.open")) << "the running Tabless's marker would read as a crash";
  EXPECT_FALSE(QFileInfo::exists(channelDir + "/tabless.lock"));
  // Nothing written to the user's folder: no marker, the session still open.
  EXPECT_FALSE(QFileInfo::exists(newDir_ + "/" + QLatin1String(heap::brand::kMovedMarker)));
  EXPECT_EQ(readFile(newDir_ + "/session.open"), QByteArray("2026-10-13T16:00:00"));

  writeFile(channelDir + "/state.json", R"({"schemaVersion":15})");  // the channel build migrated it
  EXPECT_EQ(copyStableData(channelDir).kind, MoveKind::NothingToDo);
  EXPECT_EQ(readFile(channelDir + "/state.json"), QByteArray(R"({"schemaVersion":15})"));
  EXPECT_EQ(readFile(newDir_ + "/state.json"), QByteArray(R"({"schemaVersion":14})"));
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
