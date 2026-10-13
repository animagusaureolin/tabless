// Turning stored events into the ones a calendar shows.
//
// A repeating meeting is stored once: writing every standup out would mean a
// row per working day forever and a merge conflict for each. The dates come
// from the rule, the deletions from `exdates`, and "just this one, moved" from
// an override event that names the occurrence it replaces.

#include "cal/Occurrences.h"

#include <gtest/gtest.h>

using heap::cal::expandEvents;
using heap::cal::kMaxExpandDays;
using heap::cal::Occurrence;

namespace {

const QDate kMon(2026, 9, 21);  // a Monday

CalEvent master(const QString& id, const QString& rule, const QDate& start = kMon) {
  CalEvent e;
  e.id = id;
  e.title = QStringLiteral("weekly sync");
  e.type = QStringLiteral("sync");
  e.date = start;
  e.start = 10.0;
  e.end = 11.0;
  e.rrule = rule;
  return e;
}

QVector<QDate> datesOf(const QVector<Occurrence>& xs) {
  QVector<QDate> out;
  out.reserve(xs.size());
  for(const Occurrence& o : xs) {
    out.append(o.event.date);
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

// An event with no rule is itself, once.
TEST(Occurrences, AnOrdinaryEventPassesThrough) {
  CalEvent e = master(QStringLiteral("a"), QString());

  const auto out = expandEvents({e}, kMon, kMon.addDays(30));

  ASSERT_EQ(out.size(), 1);
  EXPECT_EQ(out.at(0).event.date, kMon);
  EXPECT_FALSE(out.at(0).generated);
}

TEST(Occurrences, AnOrdinaryEventOutsideTheRangeIsNotEmitted) {
  CalEvent e = master(QStringLiteral("a"), QString());

  EXPECT_TRUE(expandEvents({e}, kMon.addDays(10), kMon.addDays(20)).isEmpty());
}

// A multi-day event that began before the range still reaches into it.
TEST(Occurrences, AnEventStartingBeforeTheRangeStillReachesIntoIt) {
  CalEvent e = master(QStringLiteral("a"), QString());
  e.endDate = kMon.addDays(5);

  const auto out = expandEvents({e}, kMon.addDays(3), kMon.addDays(4));

  EXPECT_EQ(out.size(), 1);
}

TEST(Occurrences, AWeeklyRuleRepeatsAcrossTheRange) {
  const auto out = expandEvents({master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"))}, kMon, kMon.addDays(21));

  EXPECT_EQ(datesOf(out), (QVector<QDate>{kMon, kMon.addDays(7), kMon.addDays(14), kMon.addDays(21)}));
}

// Every generated instance is a real event the views can draw without knowing
// anything about recurrence.
TEST(Occurrences, AGeneratedInstanceKeepsTheMastersDetails) {
  const auto out = expandEvents({master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"))}, kMon, kMon.addDays(7));

  ASSERT_GE(out.size(), 2);
  for(const Occurrence& o : out) {
    EXPECT_EQ(o.event.title, QStringLiteral("weekly sync"));
    EXPECT_DOUBLE_EQ(o.event.start, 10.0);
    EXPECT_DOUBLE_EQ(o.event.end, 11.0);
  }
}

// An instance is not a master: it carries no rule of its own, or expanding the
// expansion would repeat it forever.
TEST(Occurrences, AGeneratedInstanceCarriesNoRuleOfItsOwn) {
  const auto out = expandEvents({master(QStringLiteral("a"), QStringLiteral("FREQ=DAILY"))}, kMon, kMon.addDays(2));

  ASSERT_FALSE(out.isEmpty());
  for(const Occurrence& o : out) {
    EXPECT_TRUE(o.event.rrule.isEmpty());
  }
}

// It does point back at the series, so a click can find it again.
TEST(Occurrences, AGeneratedInstancePointsAtItsMaster) {
  const auto out = expandEvents({master(QStringLiteral("a"), QStringLiteral("FREQ=DAILY"))}, kMon, kMon.addDays(1));

  ASSERT_FALSE(out.isEmpty());
  EXPECT_EQ(out.at(0).event.masterId, QStringLiteral("a"));
  EXPECT_EQ(out.at(0).event.originalDate, out.at(0).event.date);
  EXPECT_TRUE(out.at(0).generated);
}

// A two-day event repeats as a two-day event.
TEST(Occurrences, AMultiDayMasterRepeatsWithItsLength) {
  CalEvent e = master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"));
  e.endDate = kMon.addDays(2);

  const auto out = expandEvents({e}, kMon, kMon.addDays(14));

  ASSERT_GE(out.size(), 2);
  for(const Occurrence& o : out) {
    EXPECT_EQ(o.event.date.daysTo(o.event.endDate), 2);
  }
}

// Deleting one standup must not delete the series.
TEST(Occurrences, AnExdateRemovesExactlyOneOccurrence) {
  CalEvent e = master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"));
  e.exdates = {kMon.addDays(7)};

  const auto out = expandEvents({e}, kMon, kMon.addDays(21));

  EXPECT_EQ(datesOf(out), (QVector<QDate>{kMon, kMon.addDays(14), kMon.addDays(21)}));
}

TEST(Occurrences, AnExdateThatMatchesNoOccurrenceChangesNothing) {
  CalEvent e = master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"));
  e.exdates = {kMon.addDays(3)};  // a Thursday; the series is on Mondays

  EXPECT_EQ(expandEvents({e}, kMon, kMon.addDays(21)).size(), 4);
}

// Moving one week's meeting leaves the rest of the series where it was.
TEST(Occurrences, AnOverrideReplacesItsOccurrence) {
  CalEvent m = master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"));
  CalEvent ov = master(QStringLiteral("a-2"), QString(), kMon.addDays(10));
  ov.title = QStringLiteral("moved");
  ov.masterId = QStringLiteral("a");
  ov.originalDate = kMon.addDays(7);

  const auto out = expandEvents({m, ov}, kMon, kMon.addDays(21));

  EXPECT_EQ(datesOf(out), (QVector<QDate>{kMon, kMon.addDays(10), kMon.addDays(14), kMon.addDays(21)}));
  bool sawMoved = false;
  for(const Occurrence& o : out) {
    if(o.event.title == QStringLiteral("moved")) {
      sawMoved = true;
      EXPECT_EQ(o.occurrenceDate, kMon.addDays(7)) << "it still knows which occurrence it replaces";
      EXPECT_FALSE(o.generated);
    }
  }
  EXPECT_TRUE(sawMoved);
}

// An override whose master is gone would be a ghost the user cannot explain.
TEST(Occurrences, AnOrphanedOverrideIsNotEmitted) {
  CalEvent ov = master(QStringLiteral("a-2"), QString());
  ov.masterId = QStringLiteral("nobody");
  ov.originalDate = kMon;

  EXPECT_TRUE(expandEvents({ov}, kMon, kMon.addDays(21)).isEmpty());
}

// Deleting an occurrence that had been moved removes it once, not twice.
TEST(Occurrences, AnExdateBeatsAnOverrideForTheSameDate) {
  CalEvent m = master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"));
  m.exdates = {kMon.addDays(7)};
  CalEvent ov = master(QStringLiteral("a-2"), QString(), kMon.addDays(10));
  ov.masterId = QStringLiteral("a");
  ov.originalDate = kMon.addDays(7);

  const auto out = expandEvents({m, ov}, kMon, kMon.addDays(21));

  EXPECT_EQ(datesOf(out), (QVector<QDate>{kMon, kMon.addDays(14), kMon.addDays(21)}));
}

TEST(Occurrences, ACountLimitsTheSeries) {
  const auto out = expandEvents({master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY;COUNT=2"))}, kMon, kMon.addDays(60));

  EXPECT_EQ(out.size(), 2);
}

TEST(Occurrences, AnUntilLimitsTheSeries) {
  const QString rule = QStringLiteral("FREQ=WEEKLY;UNTIL=") + kMon.addDays(7).toString(QStringLiteral("yyyyMMdd"));

  const auto out = expandEvents({master(QStringLiteral("a"), rule)}, kMon, kMon.addDays(60));

  EXPECT_EQ(datesOf(out), (QVector<QDate>{kMon, kMon.addDays(7)}));
}

// A rule heap does not understand is a single event, not nothing: dropping it
// would hide something the user put in the calendar.
TEST(Occurrences, AnUnparseableRuleFallsBackToOneEvent) {
  const auto out = expandEvents({master(QStringLiteral("a"), QStringLiteral("FREQ=FORTNIGHTLY;NONSENSE"))}, kMon, kMon.addDays(60));

  ASSERT_EQ(out.size(), 1);
  EXPECT_EQ(out.at(0).event.date, kMon);
}

TEST(Occurrences, AnInvalidRangeExpandsToNothing) {
  const auto e = master(QStringLiteral("a"), QStringLiteral("FREQ=DAILY"));

  EXPECT_TRUE(expandEvents({e}, QDate(), kMon).isEmpty());
  EXPECT_TRUE(expandEvents({e}, kMon, QDate()).isEmpty());
  EXPECT_TRUE(expandEvents({e}, kMon.addDays(5), kMon).isEmpty());
}

// A view asking for a decade of a daily rule is a bug in the view; it must not
// become thousands of delegates.
TEST(Occurrences, AnAbsurdRangeIsCapped) {
  const auto out = expandEvents({master(QStringLiteral("a"), QStringLiteral("FREQ=DAILY"))}, kMon, kMon.addYears(10));

  EXPECT_LE(out.size(), kMaxExpandDays + 1);
}

TEST(Occurrences, ARecurringEventWithNoDateIsSkipped) {
  CalEvent e = master(QStringLiteral("a"), QStringLiteral("FREQ=DAILY"));
  e.date = QDate();

  EXPECT_TRUE(expandEvents({e}, kMon, kMon.addDays(10)).isEmpty());
}

// Two series must not be confused with each other, however similar.
TEST(Occurrences, OverridesOnlyApplyToTheirOwnMaster) {
  CalEvent a = master(QStringLiteral("a"), QStringLiteral("FREQ=WEEKLY"));
  CalEvent b = master(QStringLiteral("b"), QStringLiteral("FREQ=WEEKLY"));
  CalEvent ov = master(QStringLiteral("a-2"), QString(), kMon.addDays(10));
  ov.masterId = QStringLiteral("a");
  ov.originalDate = kMon.addDays(7);

  const auto out = expandEvents({a, b, ov}, kMon, kMon.addDays(7));

  // b keeps both of its own occurrences; a lost one to the override, which
  // moved outside this range.
  int fromB = 0;
  for(const Occurrence& o : out) {
    if(o.event.masterId == QStringLiteral("b")) {
      fromB++;
    }
  }
  EXPECT_EQ(fromB, 2);
}

// ── An occurrence moved further than a day (TIME-1) ──
//
// A day or a week view asks for its own few days; the date the occurrence
// replaced may be well outside them. It must still be drawn where it now is.

namespace {

// Weekly on Fridays from 2032-01-02; the 01-09 one moved to Monday 01-12.
QVector<CalEvent> fridayMovedToMonday() {
  CalEvent m = master(QStringLiteral("fri"), QStringLiteral("FREQ=WEEKLY"), QDate(2032, 1, 2));
  CalEvent ov = master(QStringLiteral("fri-ov"), QString(), QDate(2032, 1, 12));
  ov.title = QStringLiteral("moved");
  ov.masterId = QStringLiteral("fri");
  ov.originalDate = QDate(2032, 1, 9);
  return {m, ov};
}

int countMoved(const QVector<Occurrence>& xs) {
  int n = 0;
  for(const Occurrence& o : xs) {
    if(o.event.id == QStringLiteral("fri-ov")) {
      n++;
      EXPECT_EQ(o.event.date, QDate(2032, 1, 12));
      EXPECT_EQ(o.occurrenceDate, QDate(2032, 1, 9));
    }
  }
  return n;
}

}  // namespace

TEST(Occurrences, AnOverrideMovedDaysAwayIsOnEveryRangeOfItsNewDate) {
  const QVector<CalEvent> stored = fridayMovedToMonday();

  EXPECT_EQ(countMoved(expandEvents(stored, QDate(2032, 1, 12), QDate(2032, 1, 12))), 1) << "the day itself";
  EXPECT_EQ(countMoved(expandEvents(stored, QDate(2032, 1, 11), QDate(2032, 1, 13))), 1) << "a day panel's d-1..d+1";
  EXPECT_EQ(countMoved(expandEvents(stored, QDate(2032, 1, 11), QDate(2032, 1, 17))), 1) << "its week";
  EXPECT_EQ(countMoved(expandEvents(stored, QDate(2031, 12, 28), QDate(2032, 2, 7))), 1) << "a month: once, not twice";
  EXPECT_EQ(countMoved(expandEvents(stored, QDate(2032, 1, 5), QDate(2032, 1, 10))), 0) << "not on the week it left";
  EXPECT_TRUE(expandEvents(stored, QDate(2032, 1, 9), QDate(2032, 1, 9)).isEmpty()) << "the Friday it left stays empty";
}

// Backwards across a week boundary: Tuesday 02-17 moved to Thursday 02-12.
TEST(Occurrences, AnOverrideMovedBackAcrossAWeekIsInTheEarlierWeek) {
  CalEvent m = master(QStringLiteral("tue"), QStringLiteral("FREQ=WEEKLY"), QDate(2032, 2, 3));
  CalEvent ov = master(QStringLiteral("tue-ov"), QString(), QDate(2032, 2, 12));
  ov.masterId = QStringLiteral("tue");
  ov.originalDate = QDate(2032, 2, 17);

  EXPECT_EQ(datesOf(expandEvents({m, ov}, QDate(2032, 2, 8), QDate(2032, 2, 14))),
            (QVector<QDate>{QDate(2032, 2, 10), QDate(2032, 2, 12)}));
  EXPECT_TRUE(expandEvents({m, ov}, QDate(2032, 2, 15), QDate(2032, 2, 21)).isEmpty()) << "its one Tuesday was moved away";
}

// Seen through its new date, an override still needs its master to produce
// the date it replaces — or it is a ghost.
TEST(Occurrences, AMovedOverrideOfADateTheSeriesNoLongerHasIsNotEmitted) {
  const QDate newDay(2032, 1, 12);
  const auto movedShown = [&](const QVector<CalEvent>& stored) {
    return countMoved(expandEvents(stored, newDay, newDay));
  };

  QVector<CalEvent> deleted = fridayMovedToMonday();
  deleted[0].exdates = {QDate(2032, 1, 9)};
  EXPECT_EQ(movedShown(deleted), 0) << "the occurrence was deleted";

  QVector<CalEvent> counted = fridayMovedToMonday();
  counted[0].rrule = QStringLiteral("FREQ=WEEKLY;COUNT=1");
  EXPECT_EQ(movedShown(counted), 0) << "the series ended before it (COUNT)";

  QVector<CalEvent> until = fridayMovedToMonday();
  until[0].rrule = QStringLiteral("FREQ=WEEKLY;UNTIL=20320108T000000Z");
  EXPECT_EQ(movedShown(until), 0) << "the series ended before it (UNTIL)";

  QVector<CalEvent> offDay = fridayMovedToMonday();
  offDay[1].originalDate = QDate(2032, 1, 8);  // a Thursday
  EXPECT_EQ(movedShown(offDay), 0) << "not a date of the series";

  QVector<CalEvent> noRule = fridayMovedToMonday();
  noRule[0].rrule.clear();
  EXPECT_EQ(movedShown(noRule), 0) << "no series left";

  QVector<CalEvent> orphan = fridayMovedToMonday();
  orphan.removeFirst();
  EXPECT_EQ(movedShown(orphan), 0) << "no master";
}

// A6-4: an occurrence of a series kept in another zone says where it came
// from - the zone and the start on that zone's clock - so a view can show
// "19:00 New York" beside the local hour. A floating event says nothing.
TEST(Occurrences, AZonedSeriesCarriesItsSourceHour) {
  CalEvent e = master(QStringLiteral("ny"), QStringLiteral("FREQ=DAILY"));
  e.tz = QStringLiteral("America/New_York");
  e.start = 19.0;
  e.end = 19.25;
  const QTimeZone moscow("Europe/Moscow");
  const auto out = expandEvents({e}, kMon.addDays(2), kMon.addDays(2), nullptr, moscow);
  ASSERT_EQ(out.size(), 1);
  EXPECT_EQ(out.first().srcTz, QStringLiteral("America/New_York"));
  EXPECT_DOUBLE_EQ(out.first().srcStart, 19.0);
  EXPECT_TRUE(out.first().event.tz.isEmpty()) << "shown on the viewer's clock";
  EXPECT_DOUBLE_EQ(out.first().event.start, 2.0) << "19:00 EDT is 02:00 in Moscow";

  const auto same = expandEvents({e}, kMon.addDays(2), kMon.addDays(2), nullptr, QTimeZone("America/New_York"));
  ASSERT_FALSE(same.isEmpty());
  EXPECT_TRUE(same.first().srcTz.isEmpty()) << "nothing to say in the zone itself";

  const auto floating = expandEvents({master(QStringLiteral("f"), QStringLiteral("FREQ=DAILY"))}, kMon, kMon);
  ASSERT_EQ(floating.size(), 1);
  EXPECT_TRUE(floating.first().srcTz.isEmpty());
}

// A6-7: a weekly New York meeting stays 10:00 New York through both DST
// changes. The US and the EU change clocks on different Sundays, so for a
// viewer in Berlin it is 16:00, then 15:00 for the weeks only one side has
// changed, then 16:00 again; for one in Moscow (no DST) it moves an hour once.
TEST(Occurrences, AWeeklyNewYorkMeetingFollowsNewYorkAcrossBothDstChanges) {
  CalEvent e = master(QStringLiteral("ny"), QStringLiteral("FREQ=WEEKLY"), QDate(2026, 10, 6));  // a Tuesday
  e.tz = QStringLiteral("America/New_York");
  e.start = 10.0;
  e.end = 10.5;
  const QTimeZone berlin("Europe/Berlin");
  const QTimeZone moscow("Europe/Moscow");
  ASSERT_TRUE(berlin.isValid() && moscow.isValid());

  const auto startOn = [&](const QDate& day, const QTimeZone& viewer) {
    const auto out = expandEvents({e}, day, day, nullptr, viewer);
    return out.size() == 1 ? out.first().event.start : -1.0;
  };
  // Autumn: EU back to CET on 25 Oct, US back to EST on 1 Nov 2026.
  EXPECT_DOUBLE_EQ(startOn(QDate(2026, 10, 20), berlin), 16.0) << "EDT and CEST";
  EXPECT_DOUBLE_EQ(startOn(QDate(2026, 10, 27), berlin), 15.0) << "EDT and CET: only Europe changed";
  EXPECT_DOUBLE_EQ(startOn(QDate(2026, 11, 3), berlin), 16.0) << "EST and CET";
  // Spring: US to EDT on 14 Mar, EU to CEST on 28 Mar 2027.
  EXPECT_DOUBLE_EQ(startOn(QDate(2027, 3, 9), berlin), 16.0) << "EST and CET";
  EXPECT_DOUBLE_EQ(startOn(QDate(2027, 3, 16), berlin), 15.0) << "EDT and CET: only the US changed";
  EXPECT_DOUBLE_EQ(startOn(QDate(2027, 3, 30), berlin), 16.0) << "EDT and CEST";
  // Moscow keeps one offset all year: the meeting moves only with New York.
  EXPECT_DOUBLE_EQ(startOn(QDate(2026, 10, 27), moscow), 17.0);
  EXPECT_DOUBLE_EQ(startOn(QDate(2026, 11, 3), moscow), 18.0);
  EXPECT_DOUBLE_EQ(startOn(QDate(2027, 3, 16), moscow), 17.0);

  // Every occurrence says the same hour on its own clock and the zone it
  // is kept in, whatever the viewer's hour is that week.
  const auto weeks = expandEvents({e}, QDate(2026, 10, 19), QDate(2026, 11, 8), nullptr, berlin);
  ASSERT_EQ(weeks.size(), 3);
  for(const Occurrence& o : weeks) {
    EXPECT_DOUBLE_EQ(o.srcStart, 10.0);
    EXPECT_EQ(o.zone, QStringLiteral("America/New_York"));
  }
}

// A6-7: what the editor writes back is the inverse of what it was shown.
TEST(Occurrences, FromViewerIsTheInverseOfLocalized) {
  CalEvent e = master(QStringLiteral("tk"), QString(), QDate(2026, 10, 27));
  e.tz = QStringLiteral("Asia/Tokyo");
  e.start = 9.0;
  e.end = 10.0;
  const QTimeZone berlin("Europe/Berlin");
  const CalEvent shown = heap::cal::localized(e, berlin);
  EXPECT_EQ(shown.date, QDate(2026, 10, 27));
  EXPECT_DOUBLE_EQ(shown.start, 1.0) << "09:00 JST is 01:00 CET";
  CalEvent edited = shown;
  edited.tz = e.tz;
  const CalEvent back = heap::cal::fromViewer(edited, berlin);
  EXPECT_EQ(back.date, e.date);
  EXPECT_DOUBLE_EQ(back.start, 9.0);
  EXPECT_DOUBLE_EQ(back.end, 10.0);
  EXPECT_EQ(back.tz, e.tz);

  // A late evening in Berlin is the next morning in Tokyo.
  edited.date = QDate(2026, 10, 26);
  edited.start = 23.0;
  edited.end = 23.5;
  const CalEvent late = heap::cal::fromViewer(edited, berlin);
  EXPECT_EQ(late.date, QDate(2026, 10, 27));
  EXPECT_DOUBLE_EQ(late.start, 7.0);

  // A floating event is left as it is.
  const CalEvent floating = master(QStringLiteral("f"), QString());
  const CalEvent same = heap::cal::fromViewer(floating, berlin);
  EXPECT_EQ(same.date, floating.date);
  EXPECT_DOUBLE_EQ(same.start, floating.start);
  EXPECT_TRUE(same.tz.isEmpty());
}
