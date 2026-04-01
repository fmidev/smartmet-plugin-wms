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
#include <engines/contour/Engine.h>
#include <engines/querydata/Engine.h>
#include <engines/querydata/Q.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <newbase/NFmiDataMatrix.h>
#include <newbase/NFmiGlobals.h>
#include <timeseries/ParameterFactory.h>
#include <trax/InterpolationType.h>
#include <cmath>
#include <limits>
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

// -----------------------------------------------------------------------
// Rendering helpers (shared between synthetic and grid paths)
// -----------------------------------------------------------------------

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

// Build the glyph group SVG (textPath elements at even intervals).
// Algorithm adapted from frontier::SvgRenderer::render_front().
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
        iri, offset, glyph);
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

// -----------------------------------------------------------------------
// TFP detection helpers
// -----------------------------------------------------------------------

// Holds the θ-gradient field computed in the first TFP pass.
struct GradField
{
  NFmiDataMatrix<float> dTdx;   // ∂θ/∂x (K/m)
  NFmiDataMatrix<float> dTdy;   // ∂θ/∂y (K/m)
  NFmiDataMatrix<float> M;      // |∇θ| (K/m)
};

// Compute the TFP field and the gradient components from a θ matrix.
// Coordinates in coords are WGS84 lon/lat (degrees); metric dx/dy are
// derived using the spherical Earth approximation.
// Grid points where |∇θ| < minGradient are set to kFloatMissing.
std::pair<NFmiDataMatrix<float>, GradField> computeTFP(
    const NFmiDataMatrix<float>& theta,
    const Fmi::CoordinateMatrix& coords,
    double minGradient)
{
  const std::size_t W = theta.NX();
  const std::size_t H = theta.NY();
  constexpr double kDegToM = 111319.5;   // metres per degree latitude
  constexpr double kPi = M_PI;

  GradField grad{
      NFmiDataMatrix<float>(W, H, kFloatMissing),
      NFmiDataMatrix<float>(W, H, kFloatMissing),
      NFmiDataMatrix<float>(W, H, kFloatMissing),
  };

  // First pass: ∂θ/∂x, ∂θ/∂y, |∇θ|
  for (std::size_t j = 1; j + 1 < H; ++j)
  {
    for (std::size_t i = 1; i + 1 < W; ++i)
    {
      const float tL = theta[i - 1][j];
      const float tR = theta[i + 1][j];
      const float tB = theta[i][j - 1];
      const float tT = theta[i][j + 1];

      if (tL == kFloatMissing || tR == kFloatMissing ||
          tB == kFloatMissing || tT == kFloatMissing)
        continue;

      const double lat = coords.y(i, j);
      const double lonL = coords.x(i - 1, j);
      const double lonR = coords.x(i + 1, j);
      const double latB = coords.y(i, j - 1);
      const double latT = coords.y(i, j + 1);

      const double dx_m = (lonR - lonL) * 0.5 * std::cos(lat * kPi / 180.0) * kDegToM;
      const double dy_m = (latT - latB) * 0.5 * kDegToM;

      if (std::abs(dx_m) < 1.0 || std::abs(dy_m) < 1.0)
        continue;

      const double dTdx = (tR - tL) / (2.0 * dx_m);
      const double dTdy = (tT - tB) / (2.0 * dy_m);
      const double m    = std::sqrt(dTdx * dTdx + dTdy * dTdy);

      grad.dTdx[i][j] = static_cast<float>(dTdx);
      grad.dTdy[i][j] = static_cast<float>(dTdy);
      grad.M[i][j]    = static_cast<float>(m);
    }
  }

  // Second pass: TFP = -(∂M/∂x * ∂θ/∂x + ∂M/∂y * ∂θ/∂y) / M
  NFmiDataMatrix<float> tfp(W, H, kFloatMissing);

  for (std::size_t j = 1; j + 1 < H; ++j)
  {
    for (std::size_t i = 1; i + 1 < W; ++i)
    {
      const float m = grad.M[i][j];
      if (m == kFloatMissing || m < static_cast<float>(minGradient))
        continue;

      const float mL = grad.M[i - 1][j];
      const float mR = grad.M[i + 1][j];
      const float mB = grad.M[i][j - 1];
      const float mT = grad.M[i][j + 1];

      if (mL == kFloatMissing || mR == kFloatMissing ||
          mB == kFloatMissing || mT == kFloatMissing)
        continue;

      const double lat  = coords.y(i, j);
      const double lonL = coords.x(i - 1, j);
      const double lonR = coords.x(i + 1, j);
      const double latB = coords.y(i, j - 1);
      const double latT = coords.y(i, j + 1);

      const double dx_m = (lonR - lonL) * 0.5 * std::cos(lat * kPi / 180.0) * kDegToM;
      const double dy_m = (latT - latB) * 0.5 * kDegToM;

      if (std::abs(dx_m) < 1.0 || std::abs(dy_m) < 1.0)
        continue;

      const double dMdx  = (mR - mL) / (2.0 * dx_m);
      const double dMdy  = (mT - mB) / (2.0 * dy_m);
      const double value = -(dMdx * grad.dTdx[i][j] + dMdy * grad.dTdy[i][j]) / m;

      tfp[i][j] = static_cast<float>(value);
    }
  }

  return {std::move(tfp), std::move(grad)};
}

// Classify a front by computing V·∇θ at the grid point nearest to the
// isoline midpoint (in the CRS coordinate space).
// Positive advection → warm air being transported → warm front.
// Negative advection → cold air being transported → cold front.
FrontType classifyFront(const OGRLineString* line,
                        const NFmiDataMatrix<float>& u,
                        const NFmiDataMatrix<float>& v,
                        const GradField& grad,
                        const Fmi::CoordinateMatrix& coordsCrs)
{
  if (!line || line->getNumPoints() < 2)
    return FrontType::Cold;

  // Midpoint of the isoline in CRS coordinates.
  const int mid = line->getNumPoints() / 2;
  const double mx = line->getX(mid);
  const double my = line->getY(mid);

  // Find the nearest grid cell by scanning (acceptable for typical grid sizes).
  const std::size_t W = coordsCrs.width();
  const std::size_t H = coordsCrs.height();
  double minDist = std::numeric_limits<double>::max();
  std::size_t bestI = 0, bestJ = 0;

  for (std::size_t j = 0; j < H; ++j)
  {
    for (std::size_t i = 0; i < W; ++i)
    {
      const double dx = coordsCrs.x(i, j) - mx;
      const double dy = coordsCrs.y(i, j) - my;
      const double d  = dx * dx + dy * dy;
      if (d < minDist)
      {
        minDist = d;
        bestI = i;
        bestJ = j;
      }
    }
  }

  const float uVal  = u[bestI][bestJ];
  const float vVal  = v[bestI][bestJ];
  const float dtdx  = grad.dTdx[bestI][bestJ];
  const float dtdy  = grad.dTdy[bestI][bestJ];

  if (uVal == kFloatMissing || vVal == kFloatMissing ||
      dtdx == kFloatMissing || dtdy == kFloatMissing)
    return FrontType::Cold;  // default when data unavailable

  const double advection = uVal * dtdx + vVal * dtdy;
  constexpr double kAdvThreshold = 1e-8;   // K/(m·s) — near-zero → stationary
  if (advection > kAdvThreshold)
    return FrontType::Warm;
  if (advection < -kAdvThreshold)
    return FrontType::Cold;
  return FrontType::Stationary;
}

// Extract screen-space points from an OGRLineString (CRS → pixels via box).
std::vector<std::pair<double, double>> lineStringToScreen(const OGRLineString* line,
                                                          const Fmi::Box& box)
{
  std::vector<std::pair<double, double>> pts;
  if (!line)
    return pts;
  pts.reserve(static_cast<std::size_t>(line->getNumPoints()));
  for (int k = 0; k < line->getNumPoints(); ++k)
  {
    double x = line->getX(k);
    double y = line->getY(k);
    box.transform(x, y);
    pts.emplace_back(x, y);
  }
  return pts;
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
    JsonTools::remove_string(itsSourceType, theJson, "source");

    if (itsSourceType == "synthetic")
    {
      itsSource = std::make_unique<SyntheticFrontSource>(theJson);
    }
    else if (itsSourceType == "grid")
    {
      GridConfig cfg;
      auto gridJson = JsonTools::remove(theJson, "grid");
      if (!gridJson.isNull())
      {
        JsonTools::remove_string(cfg.producer,     gridJson, "producer");
        JsonTools::remove_string(cfg.theta_param,  gridJson, "theta_param");
        JsonTools::remove_string(cfg.u_param,      gridJson, "u_param");
        JsonTools::remove_string(cfg.v_param,      gridJson, "v_param");
        JsonTools::remove_double(cfg.level,        gridJson, "level");
        JsonTools::remove_double(cfg.min_gradient, gridJson, "min_gradient");
        JsonTools::remove_double(cfg.min_length_px, gridJson, "min_length_px");
      }
      itsGridConfig = cfg;
    }
    else
    {
      throw Fmi::Exception(BCP, "Unknown front source '" + itsSourceType + "'");
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
    if (itsSourceType == "grid")
      generate_qEngine(theGlobals, theLayersCdt, theState);
    else
      generate_synthetic(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherFrontsLayer::generate failed");
  }
}

// ======================================================================

void WeatherFrontsLayer::generate_synthetic(CTPP::CDT& theGlobals,
                                            CTPP::CDT& theLayersCdt,
                                            State& theState)
{
  if (!itsSource)
    return;

  const auto& box = projection.getBox();
  auto crs = projection.getCRS();
  Fmi::CoordinateTransformation transformation("WGS84", crs);

  const auto curves = itsSource->getFronts(getValidTime());
  if (curves.empty())
    return;

  CTPP::CDT layerGroup(CTPP::CDT::HASH_VAL);
  layerGroup["start"] = "<g";
  layerGroup["end"]   = "";
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

// ======================================================================

void WeatherFrontsLayer::generate_qEngine(CTPP::CDT& theGlobals,
                                          CTPP::CDT& theLayersCdt,
                                          State& theState)
{
  if (!itsGridConfig)
    return;

  const auto& cfg = *itsGridConfig;

  // ---- 1. Establish querydata model and level --------------------------
  auto q = getModel(theState);
  if (!q || !q->isGrid())
    return;

  if (!q->firstLevel())
    throw Fmi::Exception(BCP, "WeatherFrontsLayer: unable to set first level");
  if (!q->selectLevel(cfg.level))
    throw Fmi::Exception(BCP, "WeatherFrontsLayer: level " +
                               Fmi::to_string(cfg.level) + " hPa not available");

  projection.update(q);
  const auto& crs     = projection.getCRS();
  const auto& box     = projection.getBox();
  const auto clipbox  = getClipBox(box);
  auto valid_time     = getValidTime();

  // ---- 2. Fetch θ, U, V -----------------------------------------------
  const auto& qEngine = theState.getQEngine();
  std::size_t qhash   = Engine::Querydata::hash_value(q);

  auto param_theta = TS::ParameterFactory::instance().parse(cfg.theta_param);
  auto param_u     = TS::ParameterFactory::instance().parse(cfg.u_param);
  auto param_v     = TS::ParameterFactory::instance().parse(cfg.v_param);

  auto makeHash = [&](const Spine::Parameter& p) {
    auto h = qhash;
    Fmi::hash_combine(h, p.hashValue());
    return h;
  };

  auto matrix_theta = qEngine.getValues(q, param_theta, makeHash(param_theta), valid_time);
  auto matrix_u     = qEngine.getValues(q, param_u,     makeHash(param_u),     valid_time);
  auto matrix_v     = qEngine.getValues(q, param_v,     makeHash(param_v),     valid_time);

  if (!matrix_theta || matrix_theta->NX() < 3 || matrix_theta->NY() < 3)
    return;

  // ---- 3. Coordinates for gradient computation (WGS84) and ContourEngine
  auto coords_wgs84 = qEngine.getWorldCoordinates(q);         // native lon/lat
  auto coords_crs   = qEngine.getWorldCoordinates(q, crs);    // target CRS

  if (!coords_wgs84 || !coords_crs)
    return;

  // ---- 4. Compute TFP field -------------------------------------------
  auto [tfp, grad] = computeTFP(*matrix_theta, *coords_wgs84, cfg.min_gradient);

  // ---- 5. Extract TFP=0 isolines via ContourEngine ---------------------
  const auto& contourer = theState.getContourEngine();

  // Unique hash for the TFP computation to avoid stale cache hits.
  auto tfpHash = qhash;
  Fmi::hash_combine(tfpHash, Fmi::hash_value(cfg.theta_param));
  Fmi::hash_combine(tfpHash, Fmi::hash_value(cfg.u_param));
  Fmi::hash_combine(tfpHash, Fmi::hash_value(cfg.v_param));
  Fmi::hash_combine(tfpHash, Fmi::hash_value(cfg.level));
  Fmi::hash_combine(tfpHash, Fmi::hash_value(std::string("TFP")));

  Engine::Contour::Options options(param_theta, valid_time, std::vector<double>{0.0});
  options.level         = cfg.level;
  options.bbox          = Fmi::BBox(box);
  options.interpolation = Trax::InterpolationType::Linear;

  auto geoms = contourer.contour(tfpHash, crs, tfp, *coords_crs, clipbox, options);

  if (geoms.empty())
    return;

  // ---- 6. Emit outer group, classify and render each isoline ----------
  CTPP::CDT layerGroup(CTPP::CDT::HASH_VAL);
  layerGroup["start"] = "<g";
  layerGroup["end"]   = "";
  if (!qid.empty())
    layerGroup["attributes"]["id"] = qid;
  layerGroup["attributes"]["class"] = "weatherfronts";
  theLayersCdt.PushBack(layerGroup);

  for (const auto& geomPtr : geoms)
  {
    if (!geomPtr)
      continue;

    // Handle both LineString and MultiLineString output from ContourEngine.
    std::vector<const OGRLineString*> lines;
    if (geomPtr->getGeometryType() == wkbLineString)
    {
      lines.push_back(dynamic_cast<const OGRLineString*>(geomPtr.get()));
    }
    else if (geomPtr->getGeometryType() == wkbMultiLineString)
    {
      const auto* multi = dynamic_cast<const OGRMultiLineString*>(geomPtr.get());
      for (int k = 0; k < multi->getNumGeometries(); ++k)
        lines.push_back(dynamic_cast<const OGRLineString*>(multi->getGeometryRef(k)));
    }

    for (const OGRLineString* line : lines)
    {
      if (!line || line->getNumPoints() < 2)
        continue;

      auto screenPts = lineStringToScreen(line, box);
      if (pathLength(screenPts) < cfg.min_length_px)
        continue;

      const FrontType type = (matrix_u && matrix_v)
          ? classifyFront(line, *matrix_u, *matrix_v, grad, *coords_crs)
          : FrontType::Cold;

      renderFrontLine(screenPts, type, theGlobals, theLayersCdt, theState);
    }
  }

  theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n</g>");
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
  pathCdt["iri"]       = iri;
  pathCdt["data"]      = buildPathData(screenPts);
  pathCdt["layertype"] = "front";
  theGlobals["paths"][iri] = pathCdt;

  const double len = pathLength(screenPts);
  const std::string glyphs = buildGlyphGroup(
      iri, style.glyph_css, style.glyph1, style.glyph2, style.font_size, style.spacing, len);

  CTPP::CDT frontCdt(CTPP::CDT::HASH_VAL);
  frontCdt["start"]                   = "<g";
  frontCdt["attributes"]["class"]     = "front-" + style.line_css;

  CTPP::CDT useCdt(CTPP::CDT::HASH_VAL);
  useCdt["start"]                    = "<use";
  useCdt["end"]                      = "/>";
  useCdt["attributes"]["class"]      = style.line_css;
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
    if (itsSourceType == "grid")
      Fmi::hash_combine(seed, Engine::Querydata::hash_value(getModel(theState)));
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
