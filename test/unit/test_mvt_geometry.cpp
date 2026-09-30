// Unit tests for the Mapbox Vector Tile geometry encoder (MapboxVectorTile.cpp).
//
// These validate the MVT *command stream* a feature is encoded into, which the
// existing tiles_gettile_mvt_*.get regression files cannot: those compare raw
// protobuf TextFormat byte-for-byte against a frozen reference, so a
// self-consistent but spec-wrong encoding (e.g. swapping the CommandInteger
// bit fields) passes unnoticed -- the reference simply froze the wrong bytes.
//
// Per the MVT 2.1 spec a CommandInteger is (count << 3) | (id & 0x7): the
// command count occupies the high bits and the command id the low three. The
// only legal ids are MoveTo(1), LineTo(2) and ClosePath(7). We decode the
// emitted geometry, assert every command is legal with the right parameter
// count, and round-trip the coordinates. A field-swap regression makes LineTo
// decode as an illegal id and ClosePath as MoveTo(7), so these tests fail.

#define BOOST_TEST_MODULE MvtGeometry
#include "MapboxVectorTile.h"
#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <ogr_geometry.h>
#include <stdexcept>
#include <vector>

using SmartMet::Plugin::Dali::MVTTileBuilder;

namespace
{
constexpr uint32_t CMD_MOVETO = 1;
constexpr uint32_t CMD_LINETO = 2;
constexpr uint32_t CMD_CLOSEPATH = 7;

int32_t unzigzag(uint32_t u)
{
  return static_cast<int32_t>(u >> 1) ^ -static_cast<int32_t>(u & 1);
}

// A decoded geometry part: a run of absolute (x,y) integer tile coordinates,
// plus whether it was terminated by a ClosePath (i.e. it is a polygon ring).
struct Part
{
  std::vector<std::pair<int32_t, int32_t>> points;
  bool closed = false;
};

// Decode an MVT geometry command stream into parts, strictly validating every
// command id and parameter count. Throws std::runtime_error on any violation,
// which is exactly what a CommandInteger field-swap produces.
std::vector<Part> decodeGeometry(const google::protobuf::RepeatedField<uint32_t>& g)
{
  std::vector<Part> parts;
  int32_t cx = 0, cy = 0;
  int i = 0;
  const int n = g.size();

  while (i < n)
  {
    const uint32_t cmdint = g.Get(i++);
    const uint32_t id = cmdint & 0x7;
    const uint32_t count = cmdint >> 3;

    if (id == CMD_MOVETO)
    {
      // In line/polygon geometry every part begins with exactly one MoveTo.
      if (count != 1)
        throw std::runtime_error("MoveTo count must be 1, got " + std::to_string(count));
      if (i + 2 > n)
        throw std::runtime_error("MoveTo truncated");
      cx += unzigzag(g.Get(i++));
      cy += unzigzag(g.Get(i++));
      parts.push_back(Part{});
      parts.back().points.emplace_back(cx, cy);
    }
    else if (id == CMD_LINETO)
    {
      if (count < 1)
        throw std::runtime_error("LineTo count must be >= 1");
      if (parts.empty())
        throw std::runtime_error("LineTo before MoveTo");
      if (i + 2 * static_cast<int>(count) > n)
        throw std::runtime_error("LineTo truncated");
      for (uint32_t k = 0; k < count; ++k)
      {
        cx += unzigzag(g.Get(i++));
        cy += unzigzag(g.Get(i++));
        parts.back().points.emplace_back(cx, cy);
      }
    }
    else if (id == CMD_CLOSEPATH)
    {
      if (count != 1)
        throw std::runtime_error("ClosePath count must be 1, got " + std::to_string(count));
      if (parts.empty())
        throw std::runtime_error("ClosePath before MoveTo");
      parts.back().closed = true;
    }
    else
    {
      throw std::runtime_error("illegal MVT command id " + std::to_string(id) + " (count " +
                               std::to_string(count) + ")");
    }
  }
  return parts;
}

// Encode a single OGR geometry into one feature of a tile whose bbox is the
// identity (0..extent on both axes), so tile coordinates equal projected input
// (with the Y axis flipped: ty = extent - y). Returns the feature's geometry.
google::protobuf::RepeatedField<uint32_t> encodeOne(const OGRGeometry& geom, unsigned extent = 4096)
{
  MVTTileBuilder tile(0, 0, extent, extent, extent);
  tile.layer("l").addFeature(geom);
  vector_tile::Tile parsed;
  BOOST_REQUIRE(parsed.ParseFromString(tile.serialize()));
  BOOST_REQUIRE_EQUAL(parsed.layers_size(), 1);
  BOOST_REQUIRE_EQUAL(parsed.layers(0).features_size(), 1);
  return parsed.layers(0).features(0).geometry();
}
}  // namespace

// The most direct regression guard: a unit square encodes to an exact, known
// command stream. With the bit fields swapped LineTo becomes 19 and ClosePath
// 57 instead of 26 and 15, so this comparison fails immediately.
BOOST_AUTO_TEST_CASE(square_polygon_exact_command_stream)
{
  OGRPolygon poly;
  auto* ring = new OGRLinearRing;
  ring->addPoint(0, 0);
  ring->addPoint(10, 0);
  ring->addPoint(10, 10);
  ring->addPoint(0, 10);
  ring->addPoint(0, 0);  // explicit closing vertex
  poly.addRingDirectly(ring);

  auto g = encodeOne(poly);
  // bbox identity, extent 4096 -> project(x,y) = (x, 4096 - y)
  //   MoveTo(1)        = (1<<3)|1 = 9 ; dx=zz(0)=0, dy=zz(4096)=8192
  //   LineTo(3)        = (3<<3)|2 = 26; (10,4096)(10,4086)(0,4086) deltas
  //   ClosePath(1)     = (1<<3)|7 = 15
  const std::vector<uint32_t> expected = {9, 0, 8192, 26, 20, 0, 0, 19, 19, 0, 15};
  std::vector<uint32_t> actual(g.begin(), g.end());
  BOOST_TEST(actual == expected, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(polygon_with_hole_decodes_to_two_closed_rings)
{
  OGRPolygon poly;
  auto* outer = new OGRLinearRing;
  outer->addPoint(0, 0);
  outer->addPoint(100, 0);
  outer->addPoint(100, 100);
  outer->addPoint(0, 100);
  outer->addPoint(0, 0);
  poly.addRingDirectly(outer);
  auto* hole = new OGRLinearRing;
  hole->addPoint(25, 25);
  hole->addPoint(25, 75);
  hole->addPoint(75, 75);
  hole->addPoint(75, 25);
  hole->addPoint(25, 25);
  poly.addRingDirectly(hole);

  std::vector<Part> parts;
  BOOST_REQUIRE_NO_THROW(parts = decodeGeometry(encodeOne(poly)));
  BOOST_REQUIRE_EQUAL(parts.size(), 2u);  // exterior + hole
  for (const auto& p : parts)
  {
    BOOST_CHECK(p.closed);                // both rings ClosePath-terminated
    BOOST_CHECK_GE(p.points.size(), 4u);  // a polygon ring needs >= 4 corners
  }
}

BOOST_AUTO_TEST_CASE(linestring_has_no_closepath_and_roundtrips)
{
  OGRLineString ls;
  ls.addPoint(0, 0);
  ls.addPoint(30, 40);
  ls.addPoint(60, 0);

  std::vector<Part> parts;
  BOOST_REQUIRE_NO_THROW(parts = decodeGeometry(encodeOne(ls)));
  BOOST_REQUIRE_EQUAL(parts.size(), 1u);
  BOOST_CHECK(!parts[0].closed);  // open line: no ClosePath
  BOOST_REQUIRE_EQUAL(parts[0].points.size(), 3u);
  // identity bbox, extent 4096: (x, 4096 - y)
  BOOST_CHECK(parts[0].points[0] == std::make_pair(0, 4096));
  BOOST_CHECK(parts[0].points[1] == std::make_pair(30, 4056));
  BOOST_CHECK(parts[0].points[2] == std::make_pair(60, 4096));
}

BOOST_AUTO_TEST_CASE(point_is_single_moveto)
{
  OGRPoint pt(12, 34);
  std::vector<Part> parts;
  BOOST_REQUIRE_NO_THROW(parts = decodeGeometry(encodeOne(pt)));
  BOOST_REQUIRE_EQUAL(parts.size(), 1u);
  BOOST_CHECK(!parts[0].closed);
  BOOST_REQUIRE_EQUAL(parts[0].points.size(), 1u);
  BOOST_CHECK(parts[0].points[0] == std::make_pair(12, 4096 - 34));
}

BOOST_AUTO_TEST_CASE(multipolygon_two_parts_each_closed)
{
  OGRMultiPolygon mp;
  for (double ox : {0.0, 200.0})
  {
    auto* poly = new OGRPolygon;
    auto* ring = new OGRLinearRing;
    ring->addPoint(ox, 0);
    ring->addPoint(ox + 50, 0);
    ring->addPoint(ox + 50, 50);
    ring->addPoint(ox, 50);
    ring->addPoint(ox, 0);
    poly->addRingDirectly(ring);
    mp.addGeometryDirectly(poly);
  }

  std::vector<Part> parts;
  BOOST_REQUIRE_NO_THROW(parts = decodeGeometry(encodeOne(mp)));
  BOOST_REQUIRE_EQUAL(parts.size(), 2u);
  BOOST_CHECK(parts[0].closed);
  BOOST_CHECK(parts[1].closed);
}
