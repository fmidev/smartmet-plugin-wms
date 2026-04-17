// ======================================================================
/*!
 * \brief WeatherFrontsLayer implementation
 */
// ======================================================================

#include "WeatherFrontsLayer.h"
#include "Config.h"
#include "JsonTools.h"
#include "State.h"
#include "SyntheticFrontSource.h"
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <cmath>
#include <sstream>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// Default styles matching the Frontier rendering engine conventions.
// Upper-case glyph = symbol on the left side of the directed path;
// lower-case = right side (determined by the weather font design).
const std::map<FrontType, WeatherFrontsLayer::FrontStyle> kDefaultStyles = {
    {FrontType::Cold, {"coldfront", "coldfrontglyph", "C", "c", 30.0, 60.0}},
    {FrontType::Warm, {"warmfront", "warmfrontglyph", "W", "w", 30.0, 60.0}},
    {FrontType::Occluded, {"occludedfront", "occludedfrontglyph", "CW", "cw", 30.0, 60.0}},
    {FrontType::Stationary, {"stationaryfront", "stationaryfrontglyph", "Cw", "Cw", 30.0, 60.0}},
    {FrontType::Trough, {"trough", "troughglyph", "t", "T", 30.0, 0.0}},
    {FrontType::Ridge, {"ridge", "ridgeglyph", "r", "R", 30.0, 0.0}},
};

// Build an SVG path d-string (polyline) from screen-space coordinates.
std::string buildPathData(const std::vector<std::pair<double, double>>& pts)
{
  std::ostringstream out;
  bool first = true;
  for (const auto& [x, y] : pts)
  {
    out << (first ? "M" : " L") << fmt::format(" {:.1f} {:.1f}", x, y);
    first = false;
  }
  return out.str();
}

double pathLength(const std::vector<std::pair<double, double>>& pts)
{
  double len = 0.0;
  for (std::size_t i = 1; i < pts.size(); ++i)
  {
    const double dx = pts[i].first - pts[i - 1].first;
    const double dy = pts[i].second - pts[i - 1].second;
    len += std::sqrt(dx * dx + dy * dy);
  }
  return len;
}

// Compute (x, y, angle_deg) positions at even arc-length intervals along a polyline.
std::vector<std::tuple<double, double, double>> symbolPositions(
    const std::vector<std::pair<double, double>>& pts, double spacing)
{
  std::vector<std::tuple<double, double, double>> result;
  if (pts.size() < 2 || spacing <= 0.0)
    return result;

  double len = pathLength(pts);
  if (len < spacing * 0.5)
    return result;

  const int n = std::max(1, static_cast<int>(std::floor(len / spacing + 0.5)));
  const double step = len / n;
  double target = step * 0.5;

  double walked = 0.0;
  for (std::size_t i = 0; i + 1 < pts.size() && result.size() < static_cast<std::size_t>(n); ++i)
  {
    const double dx = pts[i + 1].first - pts[i].first;
    const double dy = pts[i + 1].second - pts[i].second;
    const double seg = std::sqrt(dx * dx + dy * dy);
    if (seg < 1e-9)
      continue;

    while (target < walked + seg + 1e-9 && result.size() < static_cast<std::size_t>(n))
    {
      const double t = (target - walked) / seg;
      const double x = pts[i].first + t * dx;
      const double y = pts[i].second + t * dy;
      const double angle = std::atan2(dy, dx) * 180.0 / M_PI;
      result.emplace_back(x, y, angle);
      target += step;
    }
    walked += seg;
  }
  return result;
}

// SVG snippet for a single meteorological glyph character.
// Uppercase = symbol points to the left of the directed path (-y in local space).
// Lowercase = symbol points to the right (+y in local space).
std::string glyphShape(char ch, double r, double h)
{
  switch (ch)
  {
    case 'C':
      return fmt::format("<polygon points=\"{:.1f},0 {:.1f},0 0,{:.1f}\"/>", -r, r, -h);
    case 'c':
      return fmt::format("<polygon points=\"{:.1f},0 {:.1f},0 0,{:.1f}\"/>", -r, r, h);
    case 'W':
      return fmt::format("<path d=\"M {:.1f},0 A {:.1f},{:.1f} 0 0 0 {:.1f},0 Z\"/>", -r, r, r, r);
    case 'w':
      return fmt::format("<path d=\"M {:.1f},0 A {:.1f},{:.1f} 0 0 1 {:.1f},0 Z\"/>", -r, r, r, r);
    default:
      return "";
  }
}

// Build the glyph group SVG (positioned shapes at even arc-length intervals).
std::string buildGlyphGroup(const std::vector<std::pair<double, double>>& screenPts,
                            const std::string& glyphClass,
                            const std::string& glyph1,
                            const std::string& glyph2,
                            double fontSize,
                            double spacing)
{
  if (glyph1.empty() && glyph2.empty())
    return {};
  if (fontSize <= 0.0 || spacing <= 0.0)
    return {};

  const auto positions = symbolPositions(screenPts, spacing);
  if (positions.empty())
    return {};

  const double r = fontSize / 3.0;
  const double h = fontSize * 0.6;

  std::ostringstream out;
  out << fmt::format("\n<g class=\"{}\">", glyphClass);
  for (std::size_t j = 0; j < positions.size(); ++j)
  {
    const auto& [x, y, angle] = positions[j];
    const std::string& glyph = (j % 2 == 0) ? glyph1 : glyph2;
    if (glyph.empty())
      continue;

    out << fmt::format("\n <g transform=\"translate({:.1f},{:.1f}) rotate({:.1f})\">", x, y, angle);
    for (char ch : glyph)
      out << glyphShape(ch, r, h);
    out << "\n </g>";
  }
  out << "\n</g>";
  return out.str();
}

void parseStyle(WeatherFrontsLayer::FrontStyle& style, Json::Value json)
{
  if (json.isNull() || !json.isObject())
    return;
  JsonTools::remove_string(style.line_css, json, "line_css");
  JsonTools::remove_string(style.glyph_css, json, "glyph_css");
  JsonTools::remove_string(style.glyph1, json, "glyph1");
  JsonTools::remove_string(style.glyph2, json, "glyph2");
  JsonTools::remove_double(style.font_size, json, "font_size");
  JsonTools::remove_double(style.spacing, json, "spacing");
}

}  // namespace

// ======================================================================

void WeatherFrontsLayer::init(Json::Value& theJson,
                              const State& theState,
                              const Config& theConfig,
                              const Properties& theProperties)
{
  try
  {
    Layer::init(theJson, theState, theConfig, theProperties);

    itsStyles = kDefaultStyles;

    const std::map<std::string, FrontType> typeNames = {
        {"cold", FrontType::Cold},
        {"warm", FrontType::Warm},
        {"occluded", FrontType::Occluded},
        {"stationary", FrontType::Stationary},
        {"trough", FrontType::Trough},
        {"ridge", FrontType::Ridge},
    };
    for (const auto& [name, type] : typeNames)
    {
      auto styleJson = JsonTools::remove(theJson, name);
      if (!styleJson.isNull())
        parseStyle(itsStyles.at(type), styleJson);
    }

    // Data source selector. Key is "front_source" rather than "source"
    // because Properties::init already consumes "source" for paraminfo.
    JsonTools::remove_string(itsSourceType, theJson, "front_source");

    if (itsSourceType == "synthetic")
    {
      auto frontsJson = JsonTools::remove(theJson, "fronts");
      itsSource = std::make_unique<SyntheticFrontSource>(frontsJson);
    }
    else
    {
      throw Fmi::Exception(BCP, "Unsupported front_source '" + itsSourceType + "'")
          .addParameter("supported", "synthetic");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherFrontsLayer::init failed");
  }
}

// ======================================================================

void WeatherFrontsLayer::generate(CTPP::CDT& theGlobals,
                                  CTPP::CDT& theLayersCdt,
                                  State& theState)
{
  try
  {
    if (!itsSource)
      return;

    if (css)
      theGlobals["css"][theState.getCustomer() + "/" + *css] = theState.getStyle(*css);

    const auto& box = projection.getBox();
    auto crs = projection.getCRS();
    Fmi::CoordinateTransformation transformation("WGS84", crs);

    const auto curves = itsSource->getFronts(getValidTime());
    if (curves.empty())
      return;

    CTPP::CDT layerGroup(CTPP::CDT::HASH_VAL);
    layerGroup["start"] = "<g";
    layerGroup["end"] = "";
    if (!qid.empty())
      layerGroup["attributes"]["id"] = qid;
    layerGroup["attributes"]["class"] = "weatherfronts";
    theLayersCdt.PushBack(layerGroup);

    for (const auto& curve : curves)
    {
      std::vector<double> lons, lats;
      lons.reserve(curve.points.size());
      lats.reserve(curve.points.size());
      for (const auto& [lon, lat] : curve.points)
      {
        lons.push_back(lon);
        lats.push_back(lat);
      }
      transformation.transform(lons, lats);

      std::vector<std::pair<double, double>> screenPts;
      screenPts.reserve(lons.size());
      for (std::size_t i = 0; i < lons.size(); ++i)
      {
        double x = lons[i], y = lats[i];
        box.transform(x, y);
        screenPts.emplace_back(x, y);
      }
      renderFrontLine(screenPts, curve.type, theGlobals, theLayersCdt, theState);
    }

    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n</g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherFrontsLayer::generate failed");
  }
}

// ======================================================================

void WeatherFrontsLayer::renderFrontLine(const std::vector<std::pair<double, double>>& screenPts,
                                         FrontType type,
                                         CTPP::CDT& theGlobals,
                                         CTPP::CDT& theLayersCdt,
                                         State& theState) const
{
  if (screenPts.size() < 2)
    return;

  const auto& style = styleFor(type);
  const std::string iri = theState.makeQid("front");
  if (!theState.addId(iri))
    throw Fmi::Exception(BCP, "Non-unique ID for front path").addParameter("ID", iri);

  CTPP::CDT pathCdt(CTPP::CDT::HASH_VAL);
  pathCdt["iri"] = iri;
  pathCdt["data"] = buildPathData(screenPts);
  pathCdt["layertype"] = "front";
  theGlobals["paths"][iri] = pathCdt;

  const std::string glyphs = buildGlyphGroup(
      screenPts, style.glyph_css, style.glyph1, style.glyph2, style.font_size, style.spacing);

  CTPP::CDT frontCdt(CTPP::CDT::HASH_VAL);
  frontCdt["start"] = "<g";
  frontCdt["attributes"]["class"] = "front-" + style.line_css;

  CTPP::CDT useCdt(CTPP::CDT::HASH_VAL);
  useCdt["start"] = "<use";
  useCdt["end"] = "/>";
  useCdt["attributes"]["class"] = style.line_css;
  useCdt["attributes"]["xlink:href"] = "#" + iri;
  frontCdt["tags"].PushBack(useCdt);

  frontCdt["end"] = glyphs + "\n</g>";
  theLayersCdt.PushBack(frontCdt);
}

// ======================================================================

const WeatherFrontsLayer::FrontStyle& WeatherFrontsLayer::styleFor(FrontType type) const
{
  auto it = itsStyles.find(type);
  if (it != itsStyles.end())
    return it->second;
  return itsStyles.at(FrontType::Cold);
}

// ======================================================================

void WeatherFrontsLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
  // Front curves do not expose per-pixel feature info.
}

// ======================================================================

std::size_t WeatherFrontsLayer::hash_value(const State& theState) const
{
  try
  {
    return Layer::hash_value(theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherFrontsLayer::hash_value failed");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
