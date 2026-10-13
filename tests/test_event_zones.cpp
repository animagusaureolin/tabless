// A meeting kept in a time zone (A6-7), as AppController stores it.
//
// The editor, a drag and the capture line all speak the viewer's clock; an
// event that names a zone keeps its wall time in that zone. These cases are
// about what the save paths write: a zone survives every edit, the hours
// stored are that zone's, and a zone picked for a series is the series'.
//
// The expected hours are worked out from the machine's own zone, so the
// suite means the same on a laptop in Moscow and on a CI runner in UTC.

#include "AppController.h"
#include "Models.h"

#include "cal/EventZone.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

namespace {

const QString kNy = QStringLiteral("America/New_York");
const QString kTokyo = QStringLiteral("Asia/Tokyo");

struct Wall {
  QDate date;
  double hour = 0.0;
};

// `hour` on `date` in `from` as the viewer's wall clock (`from` empty: the
// viewer's own).
Wall onViewerClock(const QDate& date, double hour, const QString& from) {
  Wall w;
  heap::cal::toZoneWall(date, hour, QTimeZone(from.toUtf8()), QTimeZone::systemTimeZone(), &w.date, &w.hour);
  return w;
}

// The viewer's `hour` on `date` as the wall clock of `to`.
Wall inZone(const QDate& date, double hour, const QString& to) {
  Wall w;
  heap::cal::toZoneWall(date, hour, QTimeZone::systemTimeZone(), QTimeZone(to.toUtf8()), &w.date, &w.hour);
  return w;
}

}  // namespace

class EventZoneTest : public ::testing::Test {
 protected:
  void SetUp() override {
    app_ = std::make_unique<AppController>();
    app_->events()->reset({});
  }

  void TearDown() override {
    app_.reset();
  }

  // A meeting at 10:00-11:00 New York on `day`, made the way the editor
  // makes one: viewer-clock hours plus the zone.
  QString seedNewYork(const QDate& day, const QString& rule = QString()) {
    const Wall s = onViewerClock(day, 10.0, kNy);
    const Wall e = onViewerClock(day, 11.0, kNy);
    QVariantMap d = app_->newEventDraft(s.hour, s.date);
    d["title"] = QStringLiteral("US sync");
    d["date"] = s.date;
    d["start"] = s.hour;
    d["end"] = e.hour;
    d["endDate"] = e.date > s.date ? QVariant(e.date) : QVariant();
    d["tz"] = kNy;
    d["rrule"] = rule;
    app_->saveEvent(d);
    return d.value("id").toString();
  }

  // The row as stored (eventSeriesMaster hands back any stored event raw).
  QVariantMap stored(const QString& id) const {
    return app_->eventSeriesMaster(id);
  }

  QVariantMap occurrenceOn(const QDate& viewerDay) const {
    const QVariantList occ = app_->eventOccurrences(viewerDay.addDays(-1), viewerDay.addDays(1));
    for(const QVariant& v : occ) {
      const QVariantMap m = v.toMap();
      if(m.value("date").toDate() == viewerDay) {
        return m;
      }
    }
    return occ.isEmpty() ? QVariantMap() : occ.first().toMap();
  }

  std::unique_ptr<AppController> app_;
};

// The bug A6-7 named: a single zoned meeting lost its zone on the first edit.
TEST_F(EventZoneTest, ASingleMeetingKeepsItsZoneThroughAnEdit) {
  const QDate day(2026, 10, 20);
  const QString id = seedNewYork(day);
  QVariantMap s = stored(id);
  EXPECT_EQ(s.value("tz").toString(), kNy);
  EXPECT_EQ(s.value("date").toDate(), day);
  EXPECT_DOUBLE_EQ(s.value("start").toDouble(), 10.0) << "kept on New York's clock";
  EXPECT_DOUBLE_EQ(s.value("end").toDouble(), 11.0);

  // The editor reads it on the viewer's clock, with the zone beside it.
  QVariantMap ev = app_->eventById(id);
  const Wall local = onViewerClock(day, 10.0, kNy);
  EXPECT_EQ(ev.value("tz").toString(), kNy);
  EXPECT_DOUBLE_EQ(ev.value("start").toDouble(), local.hour);

  // Renamed and saved back as the editor does: still 10:00 New York.
  ev["title"] = QStringLiteral("US sync, renamed");
  app_->saveEvent(ev);
  s = stored(id);
  EXPECT_EQ(s.value("title").toString(), QStringLiteral("US sync, renamed"));
  EXPECT_EQ(s.value("tz").toString(), kNy);
  EXPECT_DOUBLE_EQ(s.value("start").toDouble(), 10.0);
  EXPECT_EQ(s.value("date").toDate(), day);

  // A time typed in the editor is the viewer's; stored as New York's.
  const Wall later = onViewerClock(day, 12.0, kNy);
  ev = app_->eventById(id);
  ev["date"] = later.date;
  ev["start"] = later.hour;
  ev["end"] = later.hour + 0.5;
  app_->saveEvent(ev);
  EXPECT_DOUBLE_EQ(stored(id).value("start").toDouble(), 12.0);
  EXPECT_DOUBLE_EQ(stored(id).value("end").toDouble(), 12.5);
}

// The occurrence a view hands back says the zone, so a drag keeps it.
TEST_F(EventZoneTest, ADraggedMeetingStaysInItsZone) {
  const QDate day(2026, 10, 20);
  const QString id = seedNewYork(day);
  const Wall local = onViewerClock(day, 10.0, kNy);
  const QVariantMap occ = occurrenceOn(local.date);
  ASSERT_EQ(occ.value("id").toString(), id);
  EXPECT_EQ(occ.value("tz").toString(), kNy);

  app_->moveOccurrence(occ, 1.0, QStringLiteral("this"));
  EXPECT_EQ(stored(id).value("tz").toString(), kNy);
  EXPECT_DOUBLE_EQ(stored(id).value("start").toDouble(), 11.0);

  // The old drag path (updateEvent) too.
  const Wall noon = onViewerClock(day, 12.0, kNy);
  app_->updateEvent(id, noon.hour, noon.hour + 1.0, noon.date);
  EXPECT_EQ(stored(id).value("tz").toString(), kNy);
  EXPECT_DOUBLE_EQ(stored(id).value("start").toDouble(), 12.0);
}

// Picking "local time" again lets the meeting float at the hour on screen.
TEST_F(EventZoneTest, LocalTimeAgainFloatsAtTheHourOnScreen) {
  const QDate day(2026, 10, 20);
  const QString id = seedNewYork(day);
  QVariantMap ev = app_->eventById(id);
  const double shown = ev.value("start").toDouble();
  ev["tz"] = QString();
  app_->saveEvent(ev);
  EXPECT_TRUE(stored(id).value("tz").toString().isEmpty());
  EXPECT_DOUBLE_EQ(stored(id).value("start").toDouble(), shown);
}

// A zone picked on an occurrence of a floating series is the series': the
// same moment, kept on that zone's clock from then on.
TEST_F(EventZoneTest, AZonePickedForASeriesIsTheSeries) {
  const QDate mon(2026, 9, 21);
  QVariantMap d = app_->newEventDraft(10.0, mon);
  d["title"] = QStringLiteral("weekly");
  d["start"] = 10.0;
  d["end"] = 11.0;
  d["rrule"] = QStringLiteral("FREQ=WEEKLY");
  app_->saveEvent(d);
  const QString id = d.value("id").toString();

  QVariantMap occ = occurrenceOn(mon.addDays(7));
  ASSERT_EQ(occ.value("masterId").toString(), id);
  occ["tz"] = kTokyo;
  app_->saveOccurrence(occ, QStringLiteral("all"));

  const QVariantMap m = stored(id);
  const Wall tokyo = inZone(mon, 10.0, kTokyo);
  EXPECT_EQ(m.value("tz").toString(), kTokyo);
  EXPECT_DOUBLE_EQ(m.value("start").toDouble(), tokyo.hour);
  EXPECT_EQ(m.value("date").toDate(), tokyo.date);
  EXPECT_EQ(app_->events()->rowCount(), 1) << "still one series";
  // On screen nothing moved this week.
  const QVariantMap again = occurrenceOn(mon.addDays(7));
  EXPECT_DOUBLE_EQ(again.value("start").toDouble(), 10.0);
  EXPECT_EQ(again.value("tz").toString(), kTokyo);
}

// "Just this one" of a zoned series stays in the zone too.
TEST_F(EventZoneTest, OneOccurrenceMovedKeepsTheSeriesZone) {
  const QDate tue(2026, 10, 6);
  const QString id = seedNewYork(tue, QStringLiteral("FREQ=WEEKLY"));
  const Wall local = onViewerClock(tue.addDays(7), 10.0, kNy);
  QVariantMap occ = occurrenceOn(local.date);
  ASSERT_EQ(occ.value("masterId").toString(), id);
  EXPECT_EQ(occ.value("tz").toString(), kNy);
  occ["start"] = occ.value("start").toDouble() + 1.0;
  occ["end"] = occ.value("end").toDouble() + 1.0;
  occ["originalDate"] = occ.value("occurrenceDate");
  app_->saveOccurrence(occ, QStringLiteral("this"));

  QVariantMap override;
  for(const CalEvent& e : app_->events()->items()) {
    if(e.masterId == id) {
      override = app_->eventSeriesMaster(e.id);
    }
  }
  ASSERT_FALSE(override.isEmpty());
  EXPECT_EQ(override.value("tz").toString(), kNy);
  EXPECT_DOUBLE_EQ(override.value("start").toDouble(), 11.0);
  EXPECT_DOUBLE_EQ(stored(id).value("start").toDouble(), 10.0) << "the series did not move";
}

// What capture reads hands the zone on; a task keeps none.
TEST_F(EventZoneTest, CaptureSaysTheZoneSaid) {
  const QDateTime now(QDate(2026, 10, 12), QTime(10, 0));
  EXPECT_EQ(app_->captureParse(QStringLiteral("Standup tomorrow 10:00 New York"), now, {}).value("whenTz").toString(), kNy);
  EXPECT_EQ(app_->parseDateTime(QStringLiteral("tomorrow 3pm PT"), now).value("tz").toString(), QStringLiteral("America/Los_Angeles"));
  EXPECT_TRUE(app_->captureParse(QStringLiteral("Standup tomorrow 10:00"), now, {}).value("whenTz").toString().isEmpty());
  const QVariantMap task = app_->quickTaskDraft(QStringLiteral("Standup tomorrow 3pm EST"), now, {});
  EXPECT_EQ(task.value("title").toString(), QStringLiteral("Standup"));
  EXPECT_FALSE(task.contains("tz")) << "a task is on the local clock";
}

TEST_F(EventZoneTest, TheZonePickerSearchesAndOffersRecentZones) {
  EXPECT_TRUE(app_->timeZoneSearch(QString()).isEmpty()) << "no meetings, no recent zones";
  seedNewYork(QDate(2026, 10, 20));
  const QVariantList recent = app_->timeZoneSearch(QString());
  ASSERT_EQ(recent.size(), 1);
  EXPECT_EQ(recent.first().toMap().value("id").toString(), kNy);
  EXPECT_EQ(recent.first().toMap().value("city").toString(), QStringLiteral("New York"));
  EXPECT_EQ(app_->timeZoneSearch(QStringLiteral("tokyo")).first().toMap().value("id").toString(), kTokyo);
  EXPECT_EQ(app_->zoneName(kNy), QStringLiteral("New York"));
  EXPECT_TRUE(app_->isLocalZone(QString()));
  EXPECT_TRUE(app_->isLocalZone(app_->localZoneId()));
  const QDate day(2026, 10, 20);
  const Wall local = onViewerClock(day, 10.0, kNy);
  EXPECT_DOUBLE_EQ(app_->zoneHour(local.date, local.hour, kNy), 10.0);
}

// A zone picked for a series whose master was opened by id (a capture just
// made, a reminder, People): the editor saves the master, not an occurrence.
// Over there its first meeting falls on another date, and the rule's days
// went with the old one: a Monday 20:00 series became Sundays and lost its
// first meeting (A6-7, verify 2026-10-13).
TEST_F(EventZoneTest, AZonePickedForASeriesByItsMasterKeepsEveryMeeting) {
  // Kiritimati is UTC+14: 20:00 anywhere from UTC-12 to UTC+3 is the next day
  // there, so the master's date moves in every zone a CI runner or a laptop
  // here can be in.
  const QString far = QStringLiteral("Pacific/Kiritimati");
  QDate monday(2027, 9, 13);
  ASSERT_EQ(monday.dayOfWeek(), 1);
  const Wall there = inZone(monday, 20.0, far);
  if(there.date == monday) {
    GTEST_SKIP() << "the machine's zone puts 20:00 on the same date in Kiritimati";
  }
  QVariantMap d = app_->newEventDraft(20.0, monday);
  d["title"] = QStringLiteral("Weekly");
  d["date"] = monday;
  d["start"] = 20.0;
  d["end"] = 21.0;
  d["rrule"] = QStringLiteral("FREQ=WEEKLY;BYDAY=MO");
  app_->saveEvent(d);
  const QString id = d.value("id").toString();

  QVariantMap ev = app_->eventById(id);
  ev["tz"] = far;
  app_->saveEvent(ev);

  const QVariantMap m = stored(id);
  EXPECT_EQ(m.value("tz").toString(), far);
  EXPECT_EQ(m.value("date").toDate(), there.date);
  EXPECT_DOUBLE_EQ(m.value("start").toDouble(), there.hour);
  const QVariantList occ = app_->eventOccurrences(monday.addDays(-1), monday.addDays(15));
  QList<QDate> days;
  for(const QVariant& v : occ) {
    const QVariantMap o = v.toMap();
    if(o.value("title").toString() == QStringLiteral("Weekly")) {
      days << o.value("date").toDate();
      EXPECT_NEAR(o.value("start").toDouble(), 20.0, 1e-6) << o.value("date").toDate().toString().toStdString();
    }
  }
  EXPECT_EQ(days, (QList<QDate>{monday, monday.addDays(7), monday.addDays(14)})) << "still every Monday, the first one kept";

  // Undo puts the series back as it was.
  app_->undo();
  EXPECT_TRUE(stored(id).value("tz").toString().isEmpty());
  EXPECT_EQ(stored(id).value("date").toDate(), monday);
}

// "15:00 UTC+3" said in a zone that is UTC+3 at that moment is a local
// meeting: it keeps no zone, and the zone row does not say the hour twice.
TEST_F(EventZoneTest, TheViewersOwnOffsetIsLocalTime) {
  const QDate day(2026, 10, 20);
  const QDateTime at = heap::cal::wallInstant(day, 15.0, QTimeZone::systemTimeZone());
  const QString own = QString::fromUtf8(QTimeZone(QTimeZone::systemTimeZone().offsetFromUtc(at)).id());
  QVariantMap d = app_->newEventDraft(15.0, day);
  d["title"] = QStringLiteral("Own offset");
  d["date"] = day;
  d["start"] = 15.0;
  d["end"] = 16.0;
  d["tz"] = own;
  app_->saveEvent(d);
  const QVariantMap s = stored(d.value("id").toString());
  EXPECT_TRUE(s.value("tz").toString().isEmpty()) << s.value("tz").toString().toStdString();
  EXPECT_DOUBLE_EQ(s.value("start").toDouble(), 15.0);
}

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QStandardPaths::setTestModeEnabled(true);
  QTemporaryDir scratch;
  scratch.setAutoRemove(true);
  qputenv("XDG_CONFIG_HOME", scratch.path().toUtf8());
  qputenv("XDG_DATA_HOME", scratch.path().toUtf8());

  const QApplication qapp(argc, argv);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
