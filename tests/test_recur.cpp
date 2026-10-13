// Pure recurrence engine (HEAP-77): nextOccurrence() over the chrono parser's
// "every:*" tokens. No Qt app needed — QDate is self-contained.

#include "recur/RecurrenceEngine.h"

#include <QDate>

#include <gtest/gtest.h>

using heap::recur::isRecurring;
using heap::recur::nextOccurrence;

TEST(Recur, EveryDayAndWeek) {
  const QDate d(2026, 7, 4);
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:day"), d), d.addDays(1));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:week"), d), d.addDays(7));
}

TEST(Recur, EveryWeekdaySkipsWeekend) {
  QDate fri(2026, 7, 4);
  while(fri.dayOfWeek() != 5) {  // first Friday on/after the base
    fri = fri.addDays(1);
  }
  const QDate next = nextOccurrence(QStringLiteral("every:weekday"), fri);
  EXPECT_EQ(next.dayOfWeek(), 1);   // Monday
  EXPECT_EQ(next, fri.addDays(3));  // Fri + 3 = Mon

  QDate wed(2026, 7, 4);
  while(wed.dayOfWeek() != 3) {
    wed = wed.addDays(1);
  }
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:weekday"), wed), wed.addDays(1));  // Wed → Thu
}

TEST(Recur, EveryDowLandsStrictlyAfter) {
  QDate mon(2026, 7, 4);
  while(mon.dayOfWeek() != 1) {
    mon = mon.addDays(1);
  }
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:mon"), mon), mon.addDays(7));  // same weekday → next week
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:wed"), mon), mon.addDays(2));  // Mon → Wed
}

TEST(Recur, UnknownEmptyAndInvalidYieldInvalid) {
  const QDate d(2026, 7, 4);
  EXPECT_FALSE(nextOccurrence(QString(), d).isValid());
  EXPECT_FALSE(nextOccurrence(QStringLiteral("every:month:0"), d).isValid());
  EXPECT_FALSE(nextOccurrence(QStringLiteral("every:month:32"), d).isValid());
  EXPECT_FALSE(nextOccurrence(QStringLiteral("nonsense"), d).isValid());
  EXPECT_FALSE(nextOccurrence(QStringLiteral("every:day"), QDate()).isValid());
}

TEST(Recur, IsRecurringMatchesEngine) {
  EXPECT_TRUE(isRecurring(QStringLiteral("every:weekday")));
  EXPECT_TRUE(isRecurring(QStringLiteral("every:fri")));
  EXPECT_FALSE(isRecurring(QString()));
  EXPECT_TRUE(isRecurring(QStringLiteral("every:month")));
  EXPECT_TRUE(isRecurring(QStringLiteral("every:month:15")));
  EXPECT_FALSE(isRecurring(QStringLiteral("every:fortnight")));
}

// "every month on the 15th": the 15th after `from`, clamped in a short month
// and back on the 15th after it.
TEST(Recur, MonthlyOnADayOfTheMonth) {
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:month:15"), QDate(2026, 7, 4)), QDate(2026, 7, 15));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:month:15"), QDate(2026, 7, 15)), QDate(2026, 8, 15));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:month:31"), QDate(2026, 1, 31)), QDate(2026, 2, 28));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:month:31"), QDate(2026, 2, 28)), QDate(2026, 3, 31)) << "no drift to the 28th";
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:month"), QDate(2026, 7, 4)), QDate(2026, 8, 4));
}

// Finishing late must not spawn an occurrence that is already overdue.
TEST(Recur, NextOccurrenceAfterSkipsToTheFuture) {
  using heap::recur::nextOccurrenceAfter;
  const QDate today(2026, 9, 30);  // a Wednesday
  // Weekly from three weeks ago lands on the first one after today.
  EXPECT_EQ(nextOccurrenceAfter(QStringLiteral("every:week"), QDate(2026, 9, 9), today), QDate(2026, 10, 7));
  // Not late: one step, as before.
  EXPECT_EQ(nextOccurrenceAfter(QStringLiteral("every:day"), QDate(2026, 10, 3), today), QDate(2026, 10, 4));
  // Due today, done today: tomorrow, not today again.
  EXPECT_EQ(nextOccurrenceAfter(QStringLiteral("every:day"), today, today), QDate(2026, 10, 1));
  EXPECT_FALSE(nextOccurrenceAfter(QStringLiteral("nonsense"), today, today).isValid());
}

// B7: interval repeats. "every other day" is every:2:day, counted from the
// task's own date, so the cadence never drifts to the day it was ticked.
TEST(Recur, IntervalsCountFromTheDate) {
  const QDate d(2026, 10, 13);  // Tuesday
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:2:day"), d), QDate(2026, 10, 15));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:3:week"), d), QDate(2026, 11, 3));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:2:month"), d), QDate(2026, 12, 13));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:1:day"), d), d.addDays(1)) << "an interval of one is the plain rule";
  // Every two months on the 31st: clamped in a short month, back after it.
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:2:month:31"), QDate(2026, 12, 31)), QDate(2027, 2, 28));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:2:month:31"), QDate(2027, 2, 28)), QDate(2027, 4, 30));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:2:month:31"), QDate(2027, 4, 30)), QDate(2027, 6, 30));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:2:month:31"), QDate(2027, 6, 30)), QDate(2027, 8, 31));
}

TEST(Recur, AListOfWeekdays) {
  const QDate mon(2026, 10, 12);
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:mon,wed"), mon), QDate(2026, 10, 14));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:mon,wed"), QDate(2026, 10, 14)), QDate(2026, 10, 19));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:mon,wed,fri"), QDate(2026, 10, 16)), QDate(2026, 10, 19));
  // Written in any order, read the same.
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:wed,mon"), mon), QDate(2026, 10, 14));
  EXPECT_EQ(nextOccurrence(QStringLiteral("every:sat,sun"), QDate(2026, 10, 17)), QDate(2026, 10, 18));
}

TEST(Recur, MalformedIntervalsAreNotRules) {
  for(const char* bad : {"every:0:day",
                         "every:2:weekday",
                         "every:2:mon",
                         "every:2",
                         "every:2:year",
                         "every:mon,xyz",
                         "every:367:day",
                         "every:2:month:40",
                         "every:mon:2",
                         "every:2:day:3",
                         "every:"}) {
    EXPECT_FALSE(isRecurring(QString::fromLatin1(bad))) << bad;
  }
  EXPECT_TRUE(isRecurring(QStringLiteral("every:2:week")));
  EXPECT_TRUE(isRecurring(QStringLiteral("every:mon,thu")));
}

// One rule, one spelling: two copies of a series compare equal as strings.
TEST(Recur, CanonicalSpelling) {
  using heap::recur::canonicalRule;
  EXPECT_EQ(canonicalRule(QStringLiteral("every:wed,mon")), QStringLiteral("every:mon,wed"));
  EXPECT_EQ(canonicalRule(QStringLiteral("every:mon,mon")), QStringLiteral("every:mon"));
  EXPECT_EQ(canonicalRule(QStringLiteral("every:fri,thu,wed,tue,mon")), QStringLiteral("every:weekday"));
  EXPECT_EQ(canonicalRule(QStringLiteral("every:1:week")), QStringLiteral("every:week"));
  EXPECT_EQ(canonicalRule(QStringLiteral("EVERY:2:Day")), QStringLiteral("every:2:day"));
  EXPECT_EQ(canonicalRule(QStringLiteral("every:month:15")), QStringLiteral("every:month:15"));
  EXPECT_EQ(canonicalRule(QStringLiteral("nonsense")), QString());
  // The old tokens are their own spelling: a 0.8 profile reads unchanged.
  for(const char* old : {"every:day", "every:week", "every:weekday", "every:month", "every:month:31", "every:sun"}) {
    EXPECT_EQ(canonicalRule(QString::fromLatin1(old)), QString::fromLatin1(old)) << old;
  }
}

TEST(Recur, SeriesRuleKeepsTheDayOfAMonthlyRule) {
  using heap::recur::seriesRule;
  EXPECT_EQ(seriesRule(QStringLiteral("every:month"), QDate(2026, 1, 31)), QStringLiteral("every:month:31"));
  EXPECT_EQ(seriesRule(QStringLiteral("every:2:month"), QDate(2026, 1, 31)), QStringLiteral("every:2:month:31"));
  EXPECT_EQ(seriesRule(QStringLiteral("every:2:week"), QDate(2026, 1, 31)), QStringLiteral("every:2:week"));
  EXPECT_EQ(seriesRule(QStringLiteral("every:month"), QDate()), QStringLiteral("every:month"));
}

// A2-25: what a calendar shows ahead of a repeating task — after its own
// date and after today, inside the range, nothing else.
TEST(Recur, OccurrencesBetweenForTheCalendar) {
  using heap::recur::occurrencesBetween;
  const QDate today(2026, 10, 13);  // Tuesday
  // Daily from today: the rest of the week, not today (that is the task).
  auto v = occurrencesBetween(QStringLiteral("every:day"), today, today, QDate(2026, 10, 12), QDate(2026, 10, 18));
  ASSERT_EQ(v.size(), 5);
  EXPECT_EQ(v.first(), QDate(2026, 10, 14));
  EXPECT_EQ(v.last(), QDate(2026, 10, 18));
  // Overdue since last Monday: nothing behind today, the cadence kept.
  v = occurrencesBetween(QStringLiteral("every:2:day"), QDate(2026, 10, 5), today, QDate(2026, 10, 1), QDate(2026, 10, 20));
  EXPECT_EQ(v, (QVector<QDate>{QDate(2026, 10, 15), QDate(2026, 10, 17), QDate(2026, 10, 19)}));
  // A task planned for later shows nothing before its own date.
  v = occurrencesBetween(QStringLiteral("every:mon,wed"), QDate(2026, 10, 21), today, QDate(2026, 10, 12), QDate(2026, 11, 1));
  EXPECT_EQ(v, (QVector<QDate>{QDate(2026, 10, 26), QDate(2026, 10, 28)}));
  // Every 3 weeks in a month grid.
  v = occurrencesBetween(QStringLiteral("every:3:week"), today, today, QDate(2026, 9, 28), QDate(2026, 11, 8));
  EXPECT_EQ(v, (QVector<QDate>{QDate(2026, 11, 3)}));
  // A plain monthly rule from the 31st keeps the 31st.
  v = occurrencesBetween(QStringLiteral("every:month"), QDate(2026, 10, 31), today, QDate(2026, 10, 1), QDate(2027, 1, 31));
  EXPECT_EQ(v, (QVector<QDate>{QDate(2026, 11, 30), QDate(2026, 12, 31), QDate(2027, 1, 31)}));
  // The cap, and no rule at all.
  EXPECT_EQ(occurrencesBetween(QStringLiteral("every:day"), today, today, today, today.addDays(400), 10).size(), 10);
  EXPECT_TRUE(occurrencesBetween(QStringLiteral("nonsense"), today, today, today, today.addDays(30)).isEmpty());
  EXPECT_TRUE(occurrencesBetween(QStringLiteral("every:day"), QDate(), today, today, today.addDays(30)).isEmpty());
}
