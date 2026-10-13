// Time zones by the names people say (A6-7): the event editor's zone picker
// and the date parser read the same tables (src/cal/ZoneNames.h).

#include "cal/ZoneNames.h"

#include <gtest/gtest.h>

#include <algorithm>

using heap::cal::searchZones;

namespace {

const QDateTime kAt(QDate(2026, 10, 13), QTime(12, 0), QTimeZone::UTC);

QString first(const QString& query, const QStringList& recent = {}) {
  const auto out = searchZones(query, kAt, recent);
  return out.isEmpty() ? QString() : out.first().id;
}

bool has(const QString& query, const QString& id) {
  return std::ranges::any_of(searchZones(query, kAt, {}, 40), [&](const heap::cal::ZoneMatch& m) {
    return m.id == id;
  });
}

}  // namespace

TEST(ZoneNames, ACityFindsItsZone) {
  EXPECT_EQ(first(QStringLiteral("new york")), QStringLiteral("America/New_York"));
  EXPECT_EQ(first(QStringLiteral("New_York")), QStringLiteral("America/New_York"));
  EXPECT_EQ(first(QStringLiteral("Moscow")), QStringLiteral("Europe/Moscow"));
  EXPECT_EQ(first(QString::fromUtf8("москва")), QStringLiteral("Europe/Moscow"));
  EXPECT_EQ(first(QStringLiteral("tokyo")), QStringLiteral("Asia/Tokyo"));
  // Part of a name is enough while typing.
  EXPECT_TRUE(has(QStringLiteral("new y"), QStringLiteral("America/New_York")));
  EXPECT_TRUE(has(QStringLiteral("europe/ber"), QStringLiteral("Europe/Berlin")));
}

TEST(ZoneNames, AShortNameFindsWhereItIsSaid) {
  EXPECT_EQ(first(QStringLiteral("EST")), QStringLiteral("America/New_York"));
  EXPECT_EQ(first(QStringLiteral("est")), QStringLiteral("America/New_York"));
  EXPECT_EQ(first(QStringLiteral("PT")), QStringLiteral("America/Los_Angeles"));
  EXPECT_EQ(first(QStringLiteral("MSK")), QStringLiteral("Europe/Moscow"));
  EXPECT_EQ(first(QStringLiteral("CET")), QStringLiteral("Europe/Berlin"));
}

TEST(ZoneNames, AnOffsetIsThatOffsetFirstThenThePlacesOnIt) {
  const auto out = searchZones(QStringLiteral("UTC+5"), kAt, {});
  ASSERT_FALSE(out.isEmpty());
  EXPECT_EQ(out.first().offset, 5 * 3600);
  EXPECT_FALSE(out.first().id.contains(QLatin1Char('/'))) << "a fixed offset first: " << out.first().id.toStdString();
  EXPECT_EQ(heap::cal::zoneCity(out.first().id), QStringLiteral("UTC+5"));
  bool place = false;
  for(const auto& m : out) {
    place = place || (m.id.contains(QLatin1Char('/')) && m.offset == 5 * 3600);
  }
  EXPECT_TRUE(place) << "a place on UTC+5 is offered too";
  EXPECT_EQ(searchZones(QStringLiteral("gmt-3"), kAt, {}).first().offset, -3 * 3600);
  EXPECT_EQ(searchZones(QStringLiteral("+5:30"), kAt, {}).first().offset, (5 * 3600) + (30 * 60));
}

TEST(ZoneNames, NothingTypedOffersTheRecentZonesInOrder) {
  const QStringList recent = {QStringLiteral("Asia/Tokyo"), QStringLiteral("America/New_York")};
  const auto out = searchZones(QString(), kAt, recent);
  ASSERT_EQ(out.size(), 2);
  EXPECT_EQ(out.at(0).id, QStringLiteral("Asia/Tokyo"));
  EXPECT_EQ(out.at(1).id, QStringLiteral("America/New_York"));
  EXPECT_EQ(out.at(1).city, QStringLiteral("New York"));
  EXPECT_EQ(out.at(1).region, QStringLiteral("America"));
  EXPECT_EQ(out.at(1).offset, -4 * 3600) << "EDT on 13 Oct";
}

TEST(ZoneNames, NonsenseFindsNothing) {
  EXPECT_TRUE(searchZones(QStringLiteral("qwzx"), kAt, {}).isEmpty());
}

TEST(ZoneNames, NamesAndOffsetsAsShown) {
  EXPECT_EQ(heap::cal::zoneCity(QStringLiteral("America/Argentina/Buenos_Aires")), QStringLiteral("Buenos Aires"));
  EXPECT_EQ(heap::cal::zoneCity(QStringLiteral("UTC")), QStringLiteral("UTC"));
  EXPECT_EQ(heap::cal::offsetLabel(3 * 3600), QStringLiteral("UTC+3"));
  EXPECT_EQ(heap::cal::offsetLabel(-4 * 3600), QStringLiteral("UTC") + QChar(0x2212) + QStringLiteral("4"));
  EXPECT_EQ(heap::cal::offsetLabel((5 * 3600) + (30 * 60)), QStringLiteral("UTC+5:30"));
  EXPECT_EQ(heap::cal::parseUtcOffset(QStringLiteral("UTC")).value_or(-1), 0);
  EXPECT_FALSE(heap::cal::parseUtcOffset(QStringLiteral("UTC+15")).has_value());
  EXPECT_FALSE(heap::cal::parseUtcOffset(QStringLiteral("5")).has_value()) << "a bare number is not an offset";
  EXPECT_EQ(heap::cal::cityZone(QString::fromUtf8("москве")).id(), QByteArray("Europe/Moscow"));
  EXPECT_FALSE(heap::cal::cityZone(QStringLiteral("atlantis")).isValid());
}
