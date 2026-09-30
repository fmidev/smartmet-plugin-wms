// ======================================================================
// Unit tests for Dali::Smoother::init() JSON parsing.
//
// Verifies that the legacy Savitzky-Golay keys (size/degree) and the opt-in
// Trax grid-smoother keys (method/radius/passes/boundary/morphology) parse
// into the expected fields, and that the two paths stay independent. The
// actual smoothing is exercised by the contour engine's own tests; here we
// only check the config surface.
//
// Run with: make test
// ======================================================================

#define BOOST_TEST_MODULE SmootherOptionsTest
#include <boost/test/unit_test.hpp>

#include "Config.h"
#include "Smoother.h"

#include <json/json.h>
#include <exception>

using namespace SmartMet::Plugin::Dali;

namespace
{
// Config("") returns immediately (empty config); Smoother::init ignores it.
const Config cfg("");

Json::Value parse(const std::string& text)
{
  Json::Value json;
  Json::Reader reader;
  BOOST_REQUIRE_MESSAGE(reader.parse(text, json), "test JSON failed to parse: " << text);
  return json;
}

Smoother make(const std::string& text)
{
  Smoother smoother;
  auto json = parse(text);
  smoother.init(json, cfg);
  return smoother;
}
}  // namespace

// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(savitzky_golay_size_degree_still_parses)
{
  auto sm = make(R"({"size": 2, "degree": 3})");
  BOOST_REQUIRE(sm.size);
  BOOST_CHECK_EQUAL(*sm.size, 2.0);
  BOOST_REQUIRE(sm.degree);
  BOOST_CHECK_EQUAL(*sm.degree, 3.0);
  BOOST_CHECK(!sm.trax_options);  // no method => the Trax path is not engaged
}

BOOST_AUTO_TEST_CASE(empty_object_sets_nothing)
{
  auto sm = make("{}");
  BOOST_CHECK(!sm.size);
  BOOST_CHECK(!sm.degree);
  BOOST_CHECK(!sm.trax_options);
}

BOOST_AUTO_TEST_CASE(box_method_populates_trax_options)
{
  auto sm = make(R"({"method": "box", "radius": 3, "passes": 2, "boundary": "reflect"})");
  BOOST_REQUIRE(sm.trax_options);
  BOOST_CHECK(sm.trax_options->method == Trax::SmoothMethod::Box);
  BOOST_CHECK_EQUAL(sm.trax_options->radius, 3);
  BOOST_CHECK_EQUAL(sm.trax_options->passes, 2);
  BOOST_CHECK(sm.trax_options->boundary == Trax::SmoothBoundary::Reflect);
  BOOST_CHECK(sm.trax_options->active());
  // The Savitzky-Golay fields stay empty: the paths are independent.
  BOOST_CHECK(!sm.size);
  BOOST_CHECK(!sm.degree);
}

BOOST_AUTO_TEST_CASE(median_method)
{
  auto sm = make(R"({"method": "median", "radius": 1})");
  BOOST_REQUIRE(sm.trax_options);
  BOOST_CHECK(sm.trax_options->method == Trax::SmoothMethod::Median);
  BOOST_CHECK_EQUAL(sm.trax_options->radius, 1);
  BOOST_CHECK(sm.trax_options->active());
}

BOOST_AUTO_TEST_CASE(morphology_method_and_op)
{
  auto sm = make(R"({"method": "morphology", "radius": 2, "morphology": "close"})");
  BOOST_REQUIRE(sm.trax_options);
  BOOST_CHECK(sm.trax_options->method == Trax::SmoothMethod::Morphology);
  BOOST_CHECK(sm.trax_options->morphology == Trax::MorphologyOp::Close);
  BOOST_CHECK_EQUAL(sm.trax_options->radius, 2);
}

BOOST_AUTO_TEST_CASE(preserve_missing_flag)
{
  auto sm = make(R"({"method": "box", "radius": 1, "preserve_missing": false})");
  BOOST_REQUIRE(sm.trax_options);
  BOOST_CHECK(!sm.trax_options->preserve_missing);
}

BOOST_AUTO_TEST_CASE(unknown_key_throws)
{
  Smoother sm;
  auto json = parse(R"({"methodd": "box"})");
  BOOST_CHECK_THROW(sm.init(json, cfg), std::exception);
}

BOOST_AUTO_TEST_CASE(bad_method_value_throws)
{
  Smoother sm;
  auto json = parse(R"({"method": "bogus"})");
  BOOST_CHECK_THROW(sm.init(json, cfg), std::exception);
}

BOOST_AUTO_TEST_CASE(bad_boundary_value_throws)
{
  Smoother sm;
  auto json = parse(R"({"method": "box", "boundary": "wraparound"})");
  BOOST_CHECK_THROW(sm.init(json, cfg), std::exception);
}
