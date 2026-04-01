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
    {FrontType::Cold,       {"coldfront",       "coldfrontglyph",       "C",  "c",  30.0, 60.0}},
    {FrontType::Warm,       {"warmfront",       "warmfrontglyph",       "W",  "w",  30.0, 60.0}},
    {FrontType::Occluded,   {"occludedfront",   "occludedfrontglyph",   "CW", "cw", 30.0, 60.0}},
    {FrontType::Stationary, {"stationaryfront", "stationaryfrontglyph", "CW", "CW", 30.0, 60.0}},
    {FrontType::Trough,     {"trough",          "troughglyph",          "t",  "T",  30.0,  0.0}},
    {FrontType::Ridge,      {"ridge",           "ridgeglyph",           "r",  "R",  30.0,  0.0}},
};

// Build an SVG path d-string (polyline) from screen-space coordinates.
std::string buildPathData(const std::vector<std::pair<double, double>>& screenPts)
{
  std::ostringstream out;
  bool first = true;
  for (const auto& [x, y] : screenPts)
  {
    out << (first ? "M" : " L") << fmt::format(" {:.1f} {:.1f}", x, y);
    first = false;
  }
  return out.str();
}

// Sum of Euclidean distances between consecutive screen-space points.
double pathLength(const std::vector<std::pair<double, double>>& pts)
{
  double len = 0.0;
  for (std::size_t i = 1; i < pts.size(); ++i)
  {
    const double dx = pts[i].first  - pts[i - 1].first;
    const double dy = pts[i].second - pts[i - 1].second;
    len += std::sqrt(dx * dx + dy * dy);
  }
  return len;
}

// Build the glyph group SVG string (textPath elements at even intervals).
// Algorithm taken directly from frontier::SvgRenderer::render_front().
std::string buildGlyphGroup(const std::string& iri,
                            const std::string& glyphClass,
                            const std::string& glyph1,
                            const std::string& glyph2,
                            double fontSize,
                            double spacing,
                            double len)
{
  if (glyph1.empty() && glyph2.empty())
    return {};

  const double textlength = 0.5 * static_cast<double>(glyph1.size() + glyph2.size());
  if (textlength <= 0.0 || len <= 0.0)
    return {};

  const int intervals =
      static_cast<int>(std::floor(len / (fontSize * textlength + spacing) + 0.5));
  if (intervals <= 0)
    return {};

  const double interval   = len / intervals;
  const double startpoint = interval / 2.0;

  std::ostringstream out;
  out << fmt::format("\n<g class=\"{}\">", glyphClass);
  for (int j = 0; j < intervals; ++j)
  {
    const double offset = startpoint + j * interval;
    const auto& glyph   = (j % 2 == 0) ? glyph1 : glyph2;
    out << fmt::format(
        "\n <text><textPath xlink:href=\"#{}\" startOffset=\"{:.1f}\">{}</textPath></text>",
        iri,
        offset,
        glyph);
  }
  out << "\n</g>";
  return out.str();
}

void parseStyle(WeatherFrontsLayer::FrontStyle& style, Json::Value json)
{
  if (json.isNull() || !json.isObject())
    return;
  JsonTools::remove_string(style.line_css,  json, "line_css");
  JsonTools::remove_string(style.glyph_css, json, "glyph_css");
  JsonTools::remove_string(style.glyph1,    json, "glyph1");
  JsonTools::remove_string(style.glyph2,    json, "glyph2");
  JsonTools::remove_double(style.font_size, json, "font_size");
  JsonTools::remove_double(style.spacing,   json, "spacing");
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

    // Populate style map with defaults, then apply JSON overrides.
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

    // Create the data source.
    std::string sourceName = "synthetic";
    JsonTools::remove_string(sourceName, theJson, "source");

    if (sourceName == "synthetic")
      itsSource = std::make_unique<SyntheticFrontSource>(theJson);
    else
      throw Fmi::Exception(BCP, "Unknown front source '" + sourceName + "'");
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

    const auto& box = projection.getBox();
    auto crs        = projection.getCRS();
    Fmi::CoordinateTransformation transformation("WGS84", crs);

    const auto curves = itsSource->getFronts(getValidTime());
    if (curves.empty())
      return;

    // Outer group for the whole layer.
    CTPP::CDT layerGroup(CTPP::CDT::HASH_VAL);
    layerGroup["start"] = "<g";
    layerGroup["end"]   = "";
    if (!qid.empty())
      layerGroup["attributes"]["id"] = qid;
    layerGroup["attributes"]["class"] = "weatherfronts";
    theLayersCdt.PushBack(layerGroup);

    for (const auto& curve : curves)
    {
      const auto& style = styleFor(curve.type);

      // Project points from WGS84 to screen pixels.
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

      // Store the path in <defs>.
      const std::string iri = theState.makeQid("front");
      if (!theState.addId(iri))
        throw Fmi::Exception(BCP, "Non-unique ID assigned to front path").addParameter("ID", iri);

      CTPP::CDT pathCdt(CTPP::CDT::HASH_VAL);
      pathCdt["iri"]       = iri;
      pathCdt["data"]      = buildPathData(screenPts);
      pathCdt["layertype"] = "front";
      theGlobals["paths"][iri] = pathCdt;

      // Compute path length for glyph spacing.
      const double len = pathLength(screenPts);

      // Build the glyph group SVG (appended after the <use> in end).
      const std::string glyphs = buildGlyphGroup(
          iri, style.glyph_css, style.glyph1, style.glyph2, style.font_size, style.spacing, len);

      // One layer CDT per front: <g> opening + <use> tag + glyph group + </g>.
      CTPP::CDT frontCdt(CTPP::CDT::HASH_VAL);
      frontCdt["start"] = "<g";
      frontCdt["attributes"]["class"] = "front-" + style.line_css;

      CTPP::CDT useCdt(CTPP::CDT::HASH_VAL);
      useCdt["start"]                    = "<use";
      useCdt["end"]                      = "/>";
      useCdt["attributes"]["class"]      = style.line_css;
      useCdt["attributes"]["xlink:href"] = "#" + iri;
      frontCdt["tags"].PushBack(useCdt);

      frontCdt["end"] = glyphs + "\n</g>";

      theLayersCdt.PushBack(frontCdt);
    }

    // Close the outer layer group.
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n</g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherFrontsLayer::generate failed");
  }
}

// ======================================================================

const WeatherFrontsLayer::FrontStyle& WeatherFrontsLayer::styleFor(FrontType type) const
{
  auto it = itsStyles.find(type);
  if (it != itsStyles.end())
    return it->second;
  // Fallback to cold front style — should never happen since all types
  // are populated in init(), but avoids undefined behaviour.
  return itsStyles.at(FrontType::Cold);
}

// ======================================================================

void WeatherFrontsLayer::getFeatureInfo(CTPP::CDT& /* theInfo */,
                                        const State& /* theState */)
{
  // Front curves do not expose per-pixel feature info.
}

// ======================================================================

std::size_t WeatherFrontsLayer::hash_value(const State& theState) const
{
  try
  {
    auto seed = Layer::hash_value(theState);
    // The curves from a synthetic source are fixed at init time, so we
    // only need the base Layer hash (which covers qid and projection).
    return seed;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherFrontsLayer::hash_value failed");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
