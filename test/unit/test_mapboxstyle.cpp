// Unit tests for mapboxStyleForIsobands(): turning a wms-conf isoband level
// table + its CSS colour ramp into a MapLibre/Mapbox GL style whose fill-color
// is a ["match", ["get","class"], class, colour, …, default] expression.
//
// This is the server half of "OGC API - Styles": the mechanism that propagates
// a WMS layer's configured colours onto the client-rendered MVT tiles, which
// carry the same "class" attribute on every isoband polygon. The CSS is the
// single source of truth — the same file the raster renderer uses — so the
// generator must resolve each band's class to exactly its CSS fill, in order,
// and fall back to transparent outside any band.

#define BOOST_TEST_MODULE MapboxStyle
#include "MapboxStyle.h"
#include <boost/test/unit_test.hpp>
#include <json/json.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using SmartMet::Plugin::Dali::mapboxStyle;
using SmartMet::Plugin::Dali::mapboxStyleForIsobands;
using SmartMet::Plugin::Dali::MapboxStyleLayer;

namespace
{
Json::Value parse(const std::string& text)
{
  Json::Value root;
  Json::CharReaderBuilder rb;
  std::string errs;
  std::istringstream in(text);
  BOOST_REQUIRE_MESSAGE(Json::parseFromStream(rb, in, &root, &errs), "invalid JSON: " + errs);
  return root;
}

// Two-band level table in the wms-conf convention: each band carries its class
// in attributes.class (mirrors ecmwf/tmean00/isobands/DailyMeanTemperature.json).
Json::Value levels()
{
  Json::Value arr(Json::arrayValue);
  Json::Value a(Json::objectValue);
  Json::Value b(Json::objectValue);
  a["lolimit"] = 2;
  a["hilimit"] = 4;
  a["attributes"]["class"] = "DailyMeanTemperature_2_4";
  b["lolimit"] = 4;
  b["hilimit"] = 6;
  b["attributes"]["class"] = "DailyMeanTemperature_4_6";
  arr.append(a);
  arr.append(b);
  return arr;
}

// The matching CSS ramp (verbatim from DailyMeanTemperature.css).
const std::string css =
    ".DailyMeanTemperature_2_4 { stroke: none; fill: rgba(138,237,187,1.00); }\n"
    ".DailyMeanTemperature_4_6 { stroke: none; fill: rgba(204,255,208,1.00); }\n";

const std::string tpl =
    "https://x/collections/fmi:ecmwf:dailymeantemperature/tiles/EPSG:3857/"
    "{tileMatrix}/{tileRow}/{tileCol}?f=application/vnd.mapbox-vector-tile";

// Collect the class→colour pairs from a ["match", ["get","class"], …, default]
// expression (everything between index 2 and the trailing default element).
std::map<std::string, std::string> matchPairs(const Json::Value& fc)
{
  std::map<std::string, std::string> pairs;
  for (Json::ArrayIndex i = 2; i + 1 < fc.size(); i += 2)
    pairs[fc[i].asString()] = fc[i + 1].asString();
  return pairs;
}

// Numeric variant (e.g. line-width match).
std::map<std::string, double> matchNumPairs(const Json::Value& expr)
{
  std::map<std::string, double> pairs;
  for (Json::ArrayIndex i = 2; i + 1 < expr.size(); i += 2)
    pairs[expr[i].asString()] = expr[i + 1].asDouble();
  return pairs;
}

// Two isolines with a stroke ramp (fill:none), mirroring an isoline layer's CSS.
Json::Value isolineLevels()
{
  Json::Value arr(Json::arrayValue);
  Json::Value a(Json::objectValue);
  Json::Value b(Json::objectValue);
  a["value"] = 0;
  a["attributes"]["class"] = "T_0";
  b["value"] = 5;
  b["attributes"]["class"] = "T_5";
  arr.append(a);
  arr.append(b);
  return arr;
}

const std::string isolineCss =
    ".T_0 { fill: none; stroke: rgb(0,0,255); stroke-width: 1.5px; }\n"
    ".T_5 { fill: none; stroke: black; stroke-width: 2px; }\n";

MapboxStyleLayer isobandLayer()
{
  MapboxStyleLayer l;
  l.geometry = MapboxStyleLayer::Geometry::Isoband;
  l.sourceLayer = "l";
  l.levels = levels();
  l.css = css;
  return l;
}

MapboxStyleLayer isolineLayer()
{
  MapboxStyleLayer l;
  l.geometry = MapboxStyleLayer::Geometry::Isoline;
  l.sourceLayer = "iso";
  l.levels = isolineLevels();
  l.css = isolineCss;
  return l;
}
}  // namespace

// The document is a valid Mapbox GL v8 style with one fill layer over a vector
// source pointing at the MVT template, keyed on the "l" source-layer SmartMet
// emits.
BOOST_AUTO_TEST_CASE(style_structure)
{
  auto doc = parse(mapboxStyleForIsobands("fmi:ecmwf:dailymeantemperature", tpl, levels(), css));

  BOOST_CHECK_EQUAL(doc["version"].asInt(), 8);
  BOOST_REQUIRE(doc["layers"].isArray());
  BOOST_REQUIRE_EQUAL(doc["layers"].size(), 1u);

  const auto& layer = doc["layers"][0];
  BOOST_CHECK_EQUAL(layer["type"].asString(), "fill");
  BOOST_CHECK_EQUAL(layer["source-layer"].asString(), "l");

  const std::string src = layer["source"].asString();
  BOOST_REQUIRE(doc["sources"].isMember(src));
  BOOST_CHECK_EQUAL(doc["sources"][src]["type"].asString(), "vector");
  BOOST_REQUIRE(doc["sources"][src]["tiles"].isArray());
  BOOST_CHECK_EQUAL(doc["sources"][src]["tiles"][0].asString(), tpl);
}

// fill-color resolves each band's class to exactly its CSS fill, and ends with a
// transparent default for values outside every band.
BOOST_AUTO_TEST_CASE(class_to_colour_match)
{
  auto doc = parse(mapboxStyleForIsobands("t", tpl, levels(), css));
  const auto& fc = doc["layers"][0]["paint"]["fill-color"];

  BOOST_REQUIRE(fc.isArray());
  BOOST_CHECK_EQUAL(fc[0].asString(), "match");
  BOOST_REQUIRE(fc[1].isArray());
  BOOST_CHECK_EQUAL(fc[1][0].asString(), "get");
  BOOST_CHECK_EQUAL(fc[1][1].asString(), "class");

  auto pairs = matchPairs(fc);
  BOOST_CHECK_EQUAL(pairs["DailyMeanTemperature_2_4"], "rgba(138,237,187,1.00)");
  BOOST_CHECK_EQUAL(pairs["DailyMeanTemperature_4_6"], "rgba(204,255,208,1.00)");

  // Trailing element is the transparent default.
  BOOST_CHECK_EQUAL(fc[fc.size() - 1].asString(), "rgba(0,0,0,0)");
}

// A band whose class has no CSS rule contributes no colour (skipped, not a
// broken/empty match arm).
BOOST_AUTO_TEST_CASE(bands_without_css_are_skipped)
{
  Json::Value lv = levels();
  Json::Value extra(Json::objectValue);
  extra["lolimit"] = 6;
  extra["hilimit"] = 8;
  extra["attributes"]["class"] = "DailyMeanTemperature_6_8";  // no matching CSS rule
  lv.append(extra);

  auto doc = parse(mapboxStyleForIsobands("t", tpl, lv, css));
  auto pairs = matchPairs(doc["layers"][0]["paint"]["fill-color"]);

  BOOST_CHECK(pairs.find("DailyMeanTemperature_6_8") == pairs.end());
  BOOST_CHECK_EQUAL(pairs.size(), 2u);
}

// No levels → the layer is skipped entirely (no empty/degenerate match arm).
BOOST_AUTO_TEST_CASE(empty_levels)
{
  auto doc = parse(mapboxStyleForIsobands("t", tpl, Json::Value(Json::arrayValue), css));
  BOOST_CHECK(doc["layers"].isArray());
  BOOST_CHECK_EQUAL(doc["layers"].size(), 0u);
}

// An isoline layer becomes a "line" layer: line-color from CSS stroke, line-width
// from CSS stroke-width (px stripped), both keyed on class.
BOOST_AUTO_TEST_CASE(isoline_line_layer)
{
  auto doc = parse(mapboxStyle("t", tpl, {isolineLayer()}));
  BOOST_REQUIRE_EQUAL(doc["layers"].size(), 1u);
  const auto& layer = doc["layers"][0];
  BOOST_CHECK_EQUAL(layer["type"].asString(), "line");
  BOOST_CHECK_EQUAL(layer["source-layer"].asString(), "iso");

  const auto& lc = layer["paint"]["line-color"];
  BOOST_CHECK_EQUAL(lc[0].asString(), "match");
  auto colours = matchPairs(lc);
  BOOST_CHECK_EQUAL(colours["T_0"], "rgb(0,0,255)");
  BOOST_CHECK_EQUAL(colours["T_5"], "black");  // named colours pass through

  const auto& lw = layer["paint"]["line-width"];
  BOOST_REQUIRE(lw.isArray());
  auto widths = matchNumPairs(lw);
  BOOST_CHECK_CLOSE(widths["T_0"], 1.5, 1e-6);  // "1.5px" → 1.5
  BOOST_CHECK_CLOSE(widths["T_5"], 2.0, 1e-6);  // "2px"   → 2
}

// Isolines with no per-value class fall back to the parameter-level CSS class
// (".Temperature") as a uniform (constant) line-color / line-width — the common
// case for contour lines styled with a single rule.
BOOST_AUTO_TEST_CASE(isoline_parameter_fallback)
{
  MapboxStyleLayer l;
  l.geometry = MapboxStyleLayer::Geometry::Isoline;
  l.sourceLayer = "l2";
  l.parameter = "Temperature";
  Json::Value lv(Json::arrayValue);
  Json::Value a(Json::objectValue);
  a["value"] = 0;
  a["attributes"] = Json::Value(Json::objectValue);  // no class
  lv.append(a);
  l.levels = lv;
  l.css = ".Temperature { fill:none; stroke: #999; stroke-width: 0.4px }\n";

  auto doc = parse(mapboxStyle("t", tpl, {l}));
  BOOST_REQUIRE_EQUAL(doc["layers"].size(), 1u);
  const auto& layer = doc["layers"][0];
  BOOST_CHECK_EQUAL(layer["type"].asString(), "line");
  // Constant colour/width (a string / number), not a match expression.
  BOOST_CHECK_EQUAL(layer["paint"]["line-color"].asString(), "#999");
  BOOST_CHECK_CLOSE(layer["paint"]["line-width"].asDouble(), 0.4, 1e-6);
}

// A product with both an isoband and an isoline layer yields two style layers
// (fill then line) over a single shared vector source.
BOOST_AUTO_TEST_CASE(multi_layer_product)
{
  auto doc = parse(mapboxStyle("t", tpl, {isobandLayer(), isolineLayer()}));
  BOOST_REQUIRE_EQUAL(doc["layers"].size(), 2u);
  BOOST_CHECK_EQUAL(doc["layers"][0]["type"].asString(), "fill");
  BOOST_CHECK_EQUAL(doc["layers"][0]["source-layer"].asString(), "l");
  BOOST_CHECK_EQUAL(doc["layers"][1]["type"].asString(), "line");
  BOOST_CHECK_EQUAL(doc["layers"][1]["source-layer"].asString(), "iso");
  BOOST_CHECK_EQUAL(doc["sources"].getMemberNames().size(), 1u);  // one shared source
}
