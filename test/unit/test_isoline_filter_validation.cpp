// Unit tests for IsolineFilter's adaptive validity backoff (the "validate"
// option). The backoff re-smooths the whole geometry set at a halved radius
// until every geometry is OGC-valid, so that smoothing never produces a
// self-intersecting isoband that would be invalid for clipping.
//
// A spiral band is used as the test fixture: under a wide gaussian smoothing
// radius the moving-average filter pulls neighbouring coils across each other
// and the ring self-intersects, but at roughly half that radius it stays
// valid. (Verified empirically against libsmartmet-gis.)
//
// Note: preserve_topology=false is used so a single ring is actually smoothed.
// With preserve_topology=true a lone ring's vertices are all unshared (count=1)
// and frozen; real folds occur on count=2 edges shared between adjacent bands,
// but the backoff loop being exercised here is identical in both cases.

#define BOOST_TEST_MODULE IsolineFilterValidation
#include "IsolineFilter.h"
#include <boost/test/unit_test.hpp>
#include <gis/Box.h>
#include <gis/Types.h>
#include <json/json.h>
#include <cmath>
#include <ogr_geometry.h>
#include <vector>

using SmartMet::Plugin::Dali::IsolineFilter;

namespace
{
// Archimedean spiral band that self-intersects under wide smoothing.
OGRGeometryPtr makeSpiral(double turns, double width)
{
  std::vector<std::pair<double, double>> out, in;
  const int N = 300;
  for (int i = 0; i <= N; ++i)
  {
    double t = turns * 2 * M_PI * i / N;
    double r = 1.0 + 0.6 * t;
    out.emplace_back((r + width / 2) * std::cos(t), (r + width / 2) * std::sin(t));
    in.emplace_back((r - width / 2) * std::cos(t), (r - width / 2) * std::sin(t));
  }
  auto* poly = new OGRPolygon;
  auto* ring = new OGRLinearRing;
  for (auto& p : out)
    ring->addPoint(p.first, p.second);
  for (auto it = in.rbegin(); it != in.rend(); ++it)
    ring->addPoint(it->first, it->second);
  ring->addPoint(out.front().first, out.front().second);
  ring->closeRings();
  poly->addRingDirectly(ring);
  return OGRGeometryPtr(poly);
}

IsolineFilter makeFilter(double radius, const Json::Value& validate)
{
  Json::Value json(Json::objectValue);
  json["type"] = "gaussian";
  json["radius"] = radius;
  json["iterations"] = 1;
  if (!validate.isNull())
    json["validate"] = validate;
  IsolineFilter f;
  f.init(json);
  f.bbox(Fmi::Box::identity());  // 1:1 pixel<->metric so radius stays as given
  return f;
}
}  // namespace

// Control: without validation, a wide gaussian smoothing of the spiral really
// does produce a self-intersecting (invalid) geometry. If this ever stops
// holding, the other tests are meaningless.
BOOST_AUTO_TEST_CASE(smoothing_without_validation_self_intersects)
{
  std::vector<OGRGeometryPtr> geoms{makeSpiral(3, 1.5)};
  auto filter = makeFilter(12.0, Json::Value());  // no "validate"
  filter.apply(geoms, false);
  BOOST_REQUIRE(geoms[0] && !geoms[0]->IsEmpty());
  BOOST_CHECK_MESSAGE(!geoms[0]->IsValid(),
                      "fixture no longer folds at radius 12; pick a wider radius");
}

// With validation enabled, the output must be valid...
BOOST_AUTO_TEST_CASE(validation_yields_valid_geometry)
{
  std::vector<OGRGeometryPtr> geoms{makeSpiral(3, 1.5)};
  auto filter = makeFilter(12.0, Json::Value(true));
  filter.apply(geoms, false);
  BOOST_REQUIRE(geoms[0] && !geoms[0]->IsEmpty());
  BOOST_CHECK(geoms[0]->IsValid());
}

// ...and it must still have smoothed (backed off to a smaller valid radius
// rather than silently giving up and returning the original geometry).
BOOST_AUTO_TEST_CASE(validation_retains_some_smoothing)
{
  OGRGeometryPtr original = makeSpiral(3, 1.5);
  std::vector<OGRGeometryPtr> geoms{OGRGeometryPtr(original->clone())};
  auto filter = makeFilter(12.0, Json::Value(true));
  filter.apply(geoms, false);
  BOOST_REQUIRE(geoms[0] && !geoms[0]->IsEmpty());
  BOOST_CHECK(geoms[0]->IsValid());
  BOOST_CHECK_MESSAGE(!geoms[0]->Equals(original.get()),
                      "validation fell back to the unsmoothed input (no smoothing retained)");
}

// A tiny radius does not fold, so validation must be a no-op there: the result
// equals the plain smoothed geometry and stays valid.
BOOST_AUTO_TEST_CASE(small_radius_is_unaffected_by_validation)
{
  std::vector<OGRGeometryPtr> a{makeSpiral(3, 1.5)};
  std::vector<OGRGeometryPtr> b{makeSpiral(3, 1.5)};
  makeFilter(3.0, Json::Value()).apply(a, false);
  makeFilter(3.0, Json::Value(true)).apply(b, false);
  BOOST_REQUIRE(a[0] && b[0]);
  BOOST_CHECK(a[0]->IsValid());
  BOOST_CHECK(b[0]->Equals(a[0].get()));
}

// The "taubin" filter type is accepted and smooths the geometry. Taubin
// reduces but does not by itself guarantee validity (its inflating pass can
// still push a thin feature across itself) -- that is what the validate
// backoff below is for -- so this only checks that it runs and smooths.
BOOST_AUTO_TEST_CASE(taubin_type_is_accepted_and_smooths)
{
  OGRGeometryPtr original = makeSpiral(3, 1.5);
  std::vector<OGRGeometryPtr> geoms{OGRGeometryPtr(original->clone())};

  Json::Value json(Json::objectValue);
  json["type"] = "taubin";
  json["radius"] = 4.0;
  json["iterations"] = 1;
  IsolineFilter f;
  f.init(json);
  f.bbox(Fmi::Box::identity());
  BOOST_REQUIRE_NO_THROW(f.apply(geoms, false));

  BOOST_REQUIRE(geoms[0] && !geoms[0]->IsEmpty());
  BOOST_CHECK_MESSAGE(!geoms[0]->Equals(original.get()), "taubin did not smooth the geometry");
}

// Taubin combined with the validate backoff yields a valid geometry even at a
// radius where plain Taubin self-intersects.
BOOST_AUTO_TEST_CASE(taubin_with_validation_is_valid)
{
  std::vector<OGRGeometryPtr> geoms{makeSpiral(3, 1.5)};

  Json::Value json(Json::objectValue);
  json["type"] = "taubin";
  json["radius"] = 12.0;
  json["iterations"] = 1;
  json["validate"] = true;
  IsolineFilter f;
  f.init(json);
  f.bbox(Fmi::Box::identity());
  f.apply(geoms, false);

  BOOST_REQUIRE(geoms[0] && !geoms[0]->IsEmpty());
  BOOST_CHECK(geoms[0]->IsValid());
}

// Invalid Taubin parameters are rejected at init().
BOOST_AUTO_TEST_CASE(taubin_rejects_bad_lambda_mu)
{
  Json::Value json(Json::objectValue);
  json["type"] = "taubin";
  json["radius"] = 8.0;
  json["lambda"] = 0.5;
  json["mu"] = -0.4;  // |mu| < lambda is not allowed
  IsolineFilter f;
  BOOST_CHECK_THROW(f.init(json), std::exception);
}
