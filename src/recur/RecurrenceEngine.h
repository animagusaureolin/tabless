#pragma once

#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>

// Pure recurrence engine (HEAP-77). Given a recurrence token produced by the
// chrono parser (heap::chrono, e.g. "every:weekday") and a base date, returns
// the next occurrence's date. Header-only + inline so it needs no library or
// test wiring — include and call. Returns an invalid QDate for unknown/empty
// tokens (the caller treats that as "no recurrence").
//
// The tokens, old and new (B7, interval repeats):
//   every:day  every:week  every:weekday  every:<dow>      "mon".."sun"
//   every:month            the day of the date it rolls from
//   every:month:<d>        that day of every month, clamped in a short one
//   every:<n>:day|week|month[:<d>]   every n days / weeks / months, counted
//                          from the task's own date ("every other day" is
//                          every:2:day, "every 3 weeks" every:3:week)
//   every:<dow>,<dow>,...  on those weekdays ("every mon, wed")
// A token of one kind is always written the same way (canonicalRule), so
// two copies of a series compare equal as strings.
namespace heap::recur {

namespace detail {

inline const QStringList& dayNames() {
  static const QStringList kNames = {QStringLiteral("mon"),
                                     QStringLiteral("tue"),
                                     QStringLiteral("wed"),
                                     QStringLiteral("thu"),
                                     QStringLiteral("fri"),
                                     QStringLiteral("sat"),
                                     QStringLiteral("sun")};
  return kNames;
}

// ISO day (Mon=1 … Sun=7) of "mon".."sun"; 0 for anything else.
inline int isoDay(const QString& name) {
  return static_cast<int>(dayNames().indexOf(name)) + 1;
}

}  // namespace detail

// A token read into its parts. `unit` is empty for an unknown token.
struct Rule {
  QString unit;       // "day", "week", "month", "weekday", "days"
  int every = 1;      // the interval, 1 for the plain rules
  int monthDay = 0;   // "month" only: the day it keeps, 0 = the base date's
  QVector<int> days;  // "days" only: ISO weekdays, sorted, at least one

  bool isValid() const {
    return !unit.isEmpty();
  }
};

// The longest interval a token may carry: a repeat "every 400 days" is a
// typo more often than a plan, and it keeps the walks below short.
inline constexpr int kMaxInterval = 366;

inline Rule parseRule(const QString& recurrence) {
  Rule out;
  const QString r = recurrence.trimmed().toLower();
  if(!r.startsWith(QStringLiteral("every:"))) {
    return out;
  }
  const QStringList parts = r.mid(6).split(QLatin1Char(':'));
  if(parts.isEmpty() || parts.size() > 3) {
    return out;
  }
  int at = 0;
  bool isNumber = false;
  const int n = parts.at(0).toInt(&isNumber);
  if(isNumber) {
    if(n < 1 || n > kMaxInterval || parts.size() < 2) {
      return out;
    }
    out.every = n;
    at = 1;
  }
  const QString unit = parts.at(at);
  const QStringList rest = parts.mid(at + 1);
  if(unit == QStringLiteral("month")) {
    if(rest.size() > 1) {
      return out;
    }
    if(rest.size() == 1) {
      bool ok = false;
      const int day = rest.at(0).toInt(&ok);
      if(!ok || day < 1 || day > 31) {
        return out;
      }
      out.monthDay = day;
    }
    out.unit = unit;
    return out;
  }
  if(!rest.isEmpty()) {
    return out;
  }
  if(unit == QStringLiteral("day") || unit == QStringLiteral("week")) {
    out.unit = unit;
    return out;
  }
  // The weekday rules have no interval of their own.
  if(out.every != 1) {
    return out;
  }
  if(unit == QStringLiteral("weekday")) {
    out.unit = unit;
    return out;
  }
  // "mon" or "mon,wed,fri".
  QVector<int> days;
  for(const QString& name : unit.split(QLatin1Char(','))) {
    const int iso = detail::isoDay(name.trimmed());
    if(iso == 0) {
      return out;
    }
    if(!days.contains(iso)) {
      days.append(iso);
    }
  }
  std::sort(days.begin(), days.end());
  out.days = days;
  out.unit = QStringLiteral("days");
  return out;
}

// The token as it is always written: an interval of 1 drops out
// ("every:1:week" is "every:week"), the weekdays are in order, the five
// working days are "every:weekday". An unknown token comes back empty.
inline QString formatRule(const Rule& rule) {
  if(!rule.isValid()) {
    return {};
  }
  const QString every = rule.every > 1 ? QString::number(rule.every) + QLatin1Char(':') : QString();
  if(rule.unit == QStringLiteral("month")) {
    return QStringLiteral("every:") + every + QStringLiteral("month") +
           (rule.monthDay > 0 ? QLatin1Char(':') + QString::number(rule.monthDay) : QString());
  }
  if(rule.unit == QStringLiteral("days")) {
    if(rule.days == QVector<int>{1, 2, 3, 4, 5}) {
      return QStringLiteral("every:weekday");
    }
    QStringList names;
    for(const int d : rule.days) {
      names << detail::dayNames().at(d - 1);
    }
    return QStringLiteral("every:") + names.join(QLatin1Char(','));
  }
  return QStringLiteral("every:") + every + rule.unit;
}

inline QString canonicalRule(const QString& recurrence) {
  return formatRule(parseRule(recurrence));
}

inline QDate nextOccurrence(const QString& recurrence, const QDate& from) {
  if(recurrence.isEmpty() || !from.isValid()) {
    return {};
  }
  const Rule rule = parseRule(recurrence);
  if(rule.unit == QStringLiteral("day")) {
    return from.addDays(rule.every);
  }
  if(rule.unit == QStringLiteral("week")) {
    return from.addDays(7LL * rule.every);
  }
  if(rule.unit == QStringLiteral("month")) {
    // Plain "every:month" keeps the day of `from` the way addMonths does.
    if(rule.monthDay == 0) {
      return from.addMonths(rule.every);
    }
    // "every:month:15" is the 15th of every month, clamped to the last day of
    // a shorter month and back on the 15th the month after: the first such
    // day strictly after `from`, this month's if it is still ahead. Every n
    // months it is that day n months on.
    const int day = rule.monthDay;
    if(rule.every > 1) {
      const QDate first = QDate(from.year(), from.month(), 1).addMonths(rule.every);
      return {first.year(), first.month(), qMin(day, first.daysInMonth())};
    }
    for(int k = 0; k < 2; ++k) {
      const QDate first = QDate(from.year(), from.month(), 1).addMonths(k);
      const QDate cand(first.year(), first.month(), qMin(day, first.daysInMonth()));
      if(cand > from) {
        return cand;
      }
    }
    return {};
  }
  if(rule.unit == QStringLiteral("weekday")) {
    QDate d = from.addDays(1);
    while(d.dayOfWeek() > 5) {  // Qt::Saturday=6, Qt::Sunday=7 → skip
      d = d.addDays(1);
    }
    return d;
  }
  if(rule.unit == QStringLiteral("days")) {
    // The next date strictly after `from` on one of the days: "every:mon"
    // on a Monday is the Monday after.
    for(int k = 1; k <= 7; ++k) {
      const QDate d = from.addDays(k);
      if(rule.days.contains(d.dayOfWeek())) {
        return d;
      }
    }
  }
  return {};
}

// The rule a series carries on: a plain monthly rule keeps the day of the
// date it started from, so the 31st does not become the 28th after February
// for good (IDIOT-CAL-5). "every:month" from the 31st is "every:month:31";
// "every:2:month" "every:2:month:31". Any other rule is itself.
inline QString seriesRule(const QString& recurrence, const QDate& anchor) {
  Rule rule = parseRule(recurrence);
  if(rule.unit != QStringLiteral("month") || rule.monthDay != 0 || !anchor.isValid()) {
    return recurrence;
  }
  rule.monthDay = anchor.day();
  return formatRule(rule);
}

// The first occurrence after `from` that is also after `today`. Completing a
// weekly task three weeks late must not spawn a copy that is born overdue; it
// skips to the next one still ahead. Always at least one step past `from`.
inline QDate nextOccurrenceAfter(const QString& recurrence, const QDate& from, const QDate& today) {
  QDate d = nextOccurrence(recurrence, from);
  // Daily over ten years is the longest walk that can be asked for; the cap
  // only guards against a token that stops advancing.
  for(int guard = 0; d.isValid() && today.isValid() && d <= today && guard < 4000; ++guard) {
    const QDate n = nextOccurrence(recurrence, d);
    if(!n.isValid() || n <= d) {
      break;
    }
    d = n;
  }
  return d;
}

// The occurrences a calendar shows ahead of a repeating task (A2-25), in
// [from, to]: those after the task's own date (`anchor`) and after `today`
// — the ones completing it would bring, never the one it is, and never a
// day already behind. Display only: nothing is created. At most `cap`.
inline QVector<QDate> occurrencesBetween(
    const QString& recurrence, const QDate& anchor, const QDate& today, const QDate& from, const QDate& to, int cap = 62) {
  QVector<QDate> out;
  if(!anchor.isValid() || !from.isValid() || !to.isValid() || to < from) {
    return out;
  }
  const QString rule = seriesRule(recurrence, anchor);
  const QDate floor = today.isValid() && today > anchor ? today : anchor;
  QDate d = nextOccurrence(rule, anchor);
  // Walks from the task's own date so the cadence of "every 3 weeks" stays
  // its own; 4000 steps is a daily task ten years overdue.
  for(int guard = 0; d.isValid() && d <= to && out.size() < cap && guard < 4000; ++guard) {
    if(d > floor && d >= from) {
      out.append(d);
    }
    const QDate n = nextOccurrence(rule, d);
    if(!n.isValid() || n <= d) {
      break;
    }
    d = n;
  }
  return out;
}

// True when `recurrence` is a token this engine understands.
inline bool isRecurring(const QString& recurrence) {
  return nextOccurrence(recurrence, QDate(2000, 1, 1)).isValid();
}

}  // namespace heap::recur
