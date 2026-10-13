#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <QVector>

#include <algorithm>
#include <optional>

// Time zones by the names people say (A6-7).
//
// A meeting can be kept in a zone (CalEvent::tz, see EventZone.h). Two places
// turn words into one: the date parser ("15:00 по Москве", "3pm New York")
// and the event editor's zone picker ("new york", "EST", "UTC+5"). Both read
// the tables here, so a name one of them knows the other knows too.
//
// Only Qt Core: the parser's tests link nothing else.
namespace heap::cal {

// Cities and countries by the words a sentence puts them in, lower-case:
// English, and Russian with the case forms ("по Москве", "в Нью-Йорке").
inline const QHash<QString, QByteArray>& zoneCityNames() {
  static const QHash<QString, QByteArray> names = {
      {QStringLiteral("new york"), "America/New_York"},
      {QStringLiteral("nyc"), "America/New_York"},
      {QStringLiteral("boston"), "America/New_York"},
      {QStringLiteral("toronto"), "America/Toronto"},
      {QStringLiteral("chicago"), "America/Chicago"},
      {QStringLiteral("austin"), "America/Chicago"},
      {QStringLiteral("denver"), "America/Denver"},
      {QStringLiteral("los angeles"), "America/Los_Angeles"},
      {QStringLiteral("san francisco"), "America/Los_Angeles"},
      {QStringLiteral("seattle"), "America/Los_Angeles"},
      {QStringLiteral("london"), "Europe/London"},
      {QStringLiteral("dublin"), "Europe/Dublin"},
      {QStringLiteral("lisbon"), "Europe/Lisbon"},
      {QStringLiteral("berlin"), "Europe/Berlin"},
      {QStringLiteral("paris"), "Europe/Paris"},
      {QStringLiteral("amsterdam"), "Europe/Amsterdam"},
      {QStringLiteral("warsaw"), "Europe/Warsaw"},
      {QStringLiteral("prague"), "Europe/Prague"},
      {QStringLiteral("helsinki"), "Europe/Helsinki"},
      {QStringLiteral("riga"), "Europe/Riga"},
      {QStringLiteral("tallinn"), "Europe/Tallinn"},
      {QStringLiteral("belgrade"), "Europe/Belgrade"},
      {QStringLiteral("istanbul"), "Europe/Istanbul"},
      {QStringLiteral("moscow"), "Europe/Moscow"},
      {QStringLiteral("tbilisi"), "Asia/Tbilisi"},
      {QStringLiteral("yerevan"), "Asia/Yerevan"},
      {QStringLiteral("dubai"), "Asia/Dubai"},
      {QStringLiteral("almaty"), "Asia/Almaty"},
      {QStringLiteral("tashkent"), "Asia/Tashkent"},
      {QStringLiteral("bangalore"), "Asia/Kolkata"},
      {QStringLiteral("delhi"), "Asia/Kolkata"},
      {QStringLiteral("mumbai"), "Asia/Kolkata"},
      {QStringLiteral("india"), "Asia/Kolkata"},
      {QStringLiteral("singapore"), "Asia/Singapore"},
      {QStringLiteral("hong kong"), "Asia/Hong_Kong"},
      {QStringLiteral("shanghai"), "Asia/Shanghai"},
      {QStringLiteral("beijing"), "Asia/Shanghai"},
      {QStringLiteral("seoul"), "Asia/Seoul"},
      {QStringLiteral("tokyo"), "Asia/Tokyo"},
      {QStringLiteral("sydney"), "Australia/Sydney"},
      {QString::fromUtf8("москва"), "Europe/Moscow"},
      {QString::fromUtf8("москве"), "Europe/Moscow"},
      {QString::fromUtf8("московскому"), "Europe/Moscow"},
      {QString::fromUtf8("питер"), "Europe/Moscow"},
      {QString::fromUtf8("питеру"), "Europe/Moscow"},
      {QString::fromUtf8("санкт-петербург"), "Europe/Moscow"},
      {QString::fromUtf8("екатеринбург"), "Asia/Yekaterinburg"},
      {QString::fromUtf8("екатеринбургу"), "Asia/Yekaterinburg"},
      {QString::fromUtf8("новосибирск"), "Asia/Novosibirsk"},
      {QString::fromUtf8("новосибирску"), "Asia/Novosibirsk"},
      {QString::fromUtf8("владивосток"), "Asia/Vladivostok"},
      {QString::fromUtf8("владивостоку"), "Asia/Vladivostok"},
      {QString::fromUtf8("калининград"), "Europe/Kaliningrad"},
      {QString::fromUtf8("калининграду"), "Europe/Kaliningrad"},
      {QString::fromUtf8("самара"), "Europe/Samara"},
      {QString::fromUtf8("самаре"), "Europe/Samara"},
      {QString::fromUtf8("минск"), "Europe/Minsk"},
      {QString::fromUtf8("минску"), "Europe/Minsk"},
      {QString::fromUtf8("киев"), "Europe/Kiev"},
      {QString::fromUtf8("киеву"), "Europe/Kiev"},
      {QString::fromUtf8("алматы"), "Asia/Almaty"},
      {QString::fromUtf8("ташкент"), "Asia/Tashkent"},
      {QString::fromUtf8("ташкенту"), "Asia/Tashkent"},
      {QString::fromUtf8("тбилиси"), "Asia/Tbilisi"},
      {QString::fromUtf8("ереван"), "Asia/Yerevan"},
      {QString::fromUtf8("еревану"), "Asia/Yerevan"},
      {QString::fromUtf8("белград"), "Europe/Belgrade"},
      {QString::fromUtf8("белграду"), "Europe/Belgrade"},
      {QString::fromUtf8("стамбул"), "Europe/Istanbul"},
      {QString::fromUtf8("стамбулу"), "Europe/Istanbul"},
      {QString::fromUtf8("дубай"), "Asia/Dubai"},
      {QString::fromUtf8("лондон"), "Europe/London"},
      {QString::fromUtf8("лондону"), "Europe/London"},
      {QString::fromUtf8("берлин"), "Europe/Berlin"},
      {QString::fromUtf8("берлину"), "Europe/Berlin"},
      {QString::fromUtf8("париж"), "Europe/Paris"},
      {QString::fromUtf8("парижу"), "Europe/Paris"},
      {QString::fromUtf8("амстердам"), "Europe/Amsterdam"},
      {QString::fromUtf8("амстердаму"), "Europe/Amsterdam"},
      {QString::fromUtf8("нью-йорк"), "America/New_York"},
      {QString::fromUtf8("нью-йорку"), "America/New_York"},
      {QString::fromUtf8("нью-йорке"), "America/New_York"},
      {QString::fromUtf8("нью йорк"), "America/New_York"},
      {QString::fromUtf8("нью йорку"), "America/New_York"},
      {QString::fromUtf8("чикаго"), "America/Chicago"},
      {QString::fromUtf8("лос-анджелес"), "America/Los_Angeles"},
      {QString::fromUtf8("лос-анджелесу"), "America/Los_Angeles"},
      {QString::fromUtf8("сан-франциско"), "America/Los_Angeles"},
      {QString::fromUtf8("сингапур"), "Asia/Singapore"},
      {QString::fromUtf8("сингапуру"), "Asia/Singapore"},
      {QString::fromUtf8("токио"), "Asia/Tokyo"},
      {QString::fromUtf8("сидней"), "Australia/Sydney"},
      {QString::fromUtf8("индия"), "Asia/Kolkata"},
      {QString::fromUtf8("индии"), "Asia/Kolkata"},
      {QString::fromUtf8("сиднею"), "Australia/Sydney"},
  };
  return names;
}

// Old IANA names to the ones that replaced them. Both are in the database,
// so a search listed Kiev beside Kyiv and Indianapolis twice; the old name is
// still read (a stored tz, "kiev" typed) and shown by the new one.
inline const QHash<QByteArray, QByteArray>& legacyZoneIds() {
  static const QHash<QByteArray, QByteArray> ids = {
      {"America/Indianapolis", "America/Indiana/Indianapolis"},
      {"America/Fort_Wayne", "America/Indiana/Indianapolis"},
      {"America/Louisville", "America/Kentucky/Louisville"},
      {"America/Buenos_Aires", "America/Argentina/Buenos_Aires"},
      {"America/Catamarca", "America/Argentina/Catamarca"},
      {"America/Cordoba", "America/Argentina/Cordoba"},
      {"America/Jujuy", "America/Argentina/Jujuy"},
      {"America/Mendoza", "America/Argentina/Mendoza"},
      {"America/Godthab", "America/Nuuk"},
      {"Europe/Kiev", "Europe/Kyiv"},
      {"Asia/Calcutta", "Asia/Kolkata"},
      {"Asia/Katmandu", "Asia/Kathmandu"},
      {"Asia/Saigon", "Asia/Ho_Chi_Minh"},
      {"Asia/Rangoon", "Asia/Yangon"},
      {"Asia/Dacca", "Asia/Dhaka"},
      {"Asia/Thimbu", "Asia/Thimphu"},
      {"Asia/Ulan_Bator", "Asia/Ulaanbaatar"},
      {"Asia/Ujung_Pandang", "Asia/Makassar"},
      {"Asia/Macao", "Asia/Macau"},
      {"Asia/Ashkhabad", "Asia/Ashgabat"},
      {"Asia/Chongqing", "Asia/Shanghai"},
      {"Asia/Chungking", "Asia/Shanghai"},
      {"Asia/Harbin", "Asia/Shanghai"},
      {"Asia/Kashgar", "Asia/Urumqi"},
      {"Asia/Tel_Aviv", "Asia/Jerusalem"},
      {"Asia/Istanbul", "Europe/Istanbul"},
      {"Atlantic/Faeroe", "Atlantic/Faroe"},
      {"Africa/Asmera", "Africa/Asmara"},
      {"Pacific/Ponape", "Pacific/Pohnpei"},
      {"Pacific/Truk", "Pacific/Chuuk"},
      {"Pacific/Enderbury", "Pacific/Kanton"},
      {"Australia/ACT", "Australia/Sydney"},
      {"Australia/Canberra", "Australia/Sydney"},
      {"Australia/NSW", "Australia/Sydney"},
  };
  return ids;
}

// The current name of a zone, when this build knows it; otherwise the id
// as given.
inline QByteArray canonicalZoneId(const QByteArray& id) {
  const auto it = legacyZoneIds().constFind(id);
  if(it != legacyZoneIds().cend() && QTimeZone(it.value()).isValid()) {
    return it.value();
  }
  return id;
}

// The zone a city name stands for (lower-case, as zoneCityNames() lists
// them), or an invalid zone.
inline QTimeZone cityZone(const QString& lowerName) {
  const auto it = zoneCityNames().constFind(lowerName);
  if(it == zoneCityNames().cend()) {
    return {};
  }
  const QTimeZone z(canonicalZoneId(it.value()));
  return z.isValid() ? z : QTimeZone();
}

// Short names typed into the zone search, upper-case, to the zone that uses
// them. "EST" in the picker means "where they say EST": New York, which
// follows its DST. (The date parser reads EST said with a time as the fixed
// -5 it names; see ChronoParser.)
inline const QHash<QString, QByteArray>& zoneAbbreviations() {
  static const QHash<QString, QByteArray> abbr = {
      {QStringLiteral("ET"), "America/New_York"},     {QStringLiteral("EST"), "America/New_York"},
      {QStringLiteral("EDT"), "America/New_York"},    {QStringLiteral("CT"), "America/Chicago"},
      {QStringLiteral("CST"), "America/Chicago"},     {QStringLiteral("CDT"), "America/Chicago"},
      {QStringLiteral("MT"), "America/Denver"},       {QStringLiteral("MST"), "America/Denver"},
      {QStringLiteral("MDT"), "America/Denver"},      {QStringLiteral("PT"), "America/Los_Angeles"},
      {QStringLiteral("PST"), "America/Los_Angeles"}, {QStringLiteral("PDT"), "America/Los_Angeles"},
      {QStringLiteral("GMT"), "Europe/London"},       {QStringLiteral("BST"), "Europe/London"},
      {QStringLiteral("WET"), "Europe/Lisbon"},       {QStringLiteral("CET"), "Europe/Berlin"},
      {QStringLiteral("CEST"), "Europe/Berlin"},      {QStringLiteral("EET"), "Europe/Helsinki"},
      {QStringLiteral("EEST"), "Europe/Helsinki"},    {QStringLiteral("MSK"), "Europe/Moscow"},
      {QString::fromUtf8("МСК"), "Europe/Moscow"},    {QStringLiteral("IST"), "Asia/Kolkata"},
      {QStringLiteral("SGT"), "Asia/Singapore"},      {QStringLiteral("JST"), "Asia/Tokyo"},
      {QStringLiteral("KST"), "Asia/Seoul"},          {QStringLiteral("AEST"), "Australia/Sydney"},
      {QStringLiteral("AEDT"), "Australia/Sydney"},
  };
  return abbr;
}

// "+3", "UTC+3", "GMT-5", "utc+05:30", "UTC" alone: the offset in seconds.
inline std::optional<int> parseUtcOffset(const QString& text) {
  static const QRegularExpression re(QStringLiteral(R"(^\s*(?:(?:utc|gmt)\s*)?([+\-−])\s*(\d{1,2})(?:[:.]?(\d{2}))?\s*$)"),
                                     QRegularExpression::CaseInsensitiveOption);
  const QString t = text.trimmed();
  if(t.compare(QStringLiteral("utc"), Qt::CaseInsensitive) == 0 || t.compare(QStringLiteral("gmt"), Qt::CaseInsensitive) == 0) {
    return 0;
  }
  const auto m = re.match(t);
  if(!m.hasMatch()) {
    return std::nullopt;
  }
  const int hours = m.captured(2).toInt();
  const int minutes = m.captured(3).isEmpty() ? 0 : m.captured(3).toInt();
  if(hours > 14 || minutes > 59) {
    return std::nullopt;
  }
  const int sign = m.captured(1) == QStringLiteral("+") ? 1 : -1;
  return sign * ((hours * 3600) + (minutes * 60));
}

// "UTC+3", "UTC−4", "UTC+5:30", "UTC".
inline QString offsetLabel(int secs) {
  if(secs == 0) {
    return QStringLiteral("UTC");
  }
  const int a = secs < 0 ? -secs : secs;
  QString out = QStringLiteral("UTC") + (secs < 0 ? QString(QChar(0x2212)) : QStringLiteral("+")) + QString::number(a / 3600);
  if((a % 3600) != 0) {
    out += QLatin1Char(':') + QStringLiteral("%1").arg((a % 3600) / 60, 2, 10, QLatin1Char('0'));
  }
  return out;
}

// The name a zone is shown by: the city of an IANA id ("America/New_York" is
// "New York"), the offset of a fixed one ("UTC-05:00" is "UTC−5").
inline QString zoneCity(const QString& id) {
  if(id.isEmpty()) {
    return {};
  }
  if(!id.contains(QLatin1Char('/'))) {
    const QTimeZone z(id.toUtf8());
    if(z.isValid() && (id.startsWith(QStringLiteral("UTC")) || id.startsWith(QStringLiteral("GMT")))) {
      return offsetLabel(z.offsetFromUtc(QDateTime::currentDateTimeUtc()));
    }
    return id;
  }
  QString city = id.section(QLatin1Char('/'), -1);
  city.replace(QLatin1Char('_'), QLatin1Char(' '));
  return city;
}

struct ZoneMatch {
  QString id;      // what CalEvent::tz stores
  QString city;    // "New York"
  QString region;  // "America"; empty for a fixed offset
  int offset = 0;  // seconds from UTC at the instant searched for
};

namespace detail {

inline QString normalizeZoneQuery(const QString& s) {
  QString out = s.toLower();
  out.replace(QLatin1Char('_'), QLatin1Char(' '));
  return out.simplified();
}

// Every IANA id this build knows, "Etc/…" and the bare aliases left out.
inline const QList<QByteArray>& searchableZoneIds() {
  static const QList<QByteArray> ids = [] {
    QList<QByteArray> out;
    for(const QByteArray& id : QTimeZone::availableTimeZoneIds()) {
      if(!id.contains('/') || id.startsWith("Etc/") || id.startsWith("SystemV/") || id.startsWith("US/") || id.startsWith("Canada/") ||
         id.startsWith("Mexico/") || id.startsWith("Brazil/") || id.startsWith("Chile/")) {
        continue;
      }
      out.append(id);
    }
    return out;
  }();
  return ids;
}

}  // namespace detail

// Zones for what was typed into the picker: a city ("new york", "москва"),
// part of an IANA id ("europe/ber"), a short name ("EST", "MSK"), an offset
// ("UTC+5", "+5:30" — that fixed offset first, then the places on it at `at`).
// With nothing typed: the zones in `recent`, in that order. A zone in `recent`
// goes before an equal match that is not.
inline QVector<ZoneMatch> searchZones(const QString& query, const QDateTime& at, const QStringList& recent, int limit = 12) {
  QHash<QString, int> score;
  const auto add = [&](const QByteArray& given, int s) {
    // One row per place: an old name ("Europe/Kiev") counts for the new one.
    const QByteArray id = canonicalZoneId(given);
    const QString key = QString::fromUtf8(id);
    if(!QTimeZone(id).isValid()) {
      return;
    }
    if(s > score.value(key, -1)) {
      score.insert(key, s);
    }
  };
  const QString q = detail::normalizeZoneQuery(query);
  if(q.isEmpty()) {
    for(int i = 0; i < recent.size(); ++i) {
      add(recent.at(i).toUtf8(), 1000 - i);
    }
  } else {
    if(const auto off = parseUtcOffset(q)) {
      add(QTimeZone(*off).id(), 300);
      // The places on it: the cities people name first, then the rest
      // (research stations last of all).
      for(const QByteArray& id : zoneCityNames()) {
        if(QTimeZone(id).offsetFromUtc(at) == *off) {
          add(id, 110);
        }
      }
      int n = 0;
      for(const QByteArray& id : detail::searchableZoneIds()) {
        if(n < limit && !id.startsWith("Antarctica/") && QTimeZone(id).offsetFromUtc(at) == *off) {
          add(id, 100);
          ++n;
        }
      }
    }
    if(const auto ab = zoneAbbreviations().constFind(query.trimmed().toUpper()); ab != zoneAbbreviations().cend()) {
      add(ab.value(), 280);
    }
    for(auto it = zoneCityNames().cbegin(); it != zoneCityNames().cend(); ++it) {
      if(it.key() == q) {
        add(it.value(), 260);
      } else if(it.key().startsWith(q)) {
        add(it.value(), 200);
      }
    }
    for(const QByteArray& id : detail::searchableZoneIds()) {
      const QString full = detail::normalizeZoneQuery(QString::fromUtf8(id));
      const QString city = full.section(QLatin1Char('/'), -1);
      if(city == q) {
        add(id, 250);
      } else if(city.startsWith(q)) {
        add(id, 180);
      } else if(q.size() >= 2 && full.contains(q)) {
        add(id, 120);
      }
    }
    for(const QString& given : recent) {
      const QString r = QString::fromUtf8(canonicalZoneId(given.toUtf8()));
      if(score.contains(r)) {
        score[r] += 10;
      }
    }
  }
  QVector<ZoneMatch> out;
  out.reserve(score.size());
  for(auto it = score.cbegin(); it != score.cend(); ++it) {
    const QTimeZone z(it.key().toUtf8());
    ZoneMatch m;
    m.id = it.key();
    m.city = zoneCity(m.id);
    m.region =
        m.id.contains(QLatin1Char('/')) ? m.id.section(QLatin1Char('/'), 0, -2).replace(QLatin1Char('_'), QLatin1Char(' ')) : QString();
    m.offset = z.offsetFromUtc(at);
    out.append(m);
  }
  std::sort(out.begin(), out.end(), [&](const ZoneMatch& a, const ZoneMatch& b) {
    const int sa = score.value(a.id);
    const int sb = score.value(b.id);
    if(sa != sb) {
      return sa > sb;
    }
    return a.id < b.id;
  });
  if(out.size() > limit) {
    out.resize(limit);
  }
  return out;
}

}  // namespace heap::cal
