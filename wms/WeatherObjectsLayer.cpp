// ======================================================================
/*!
 * \brief WeatherObjectsLayer implementation.
 */
// ======================================================================

#include "WeatherObjectsLayer.h"
#include "Config.h"
#include "JsonTools.h"
#include "State.h"

#include <ctpp2/CDT.hpp>
#include <engines/querydata/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateMatrix.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <newbase/NFmiGlobals.h>
#include <timeseries/ParameterFactory.h>

#include <algorithm>
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
// Default CSS classes per object kind.
std::string defaultCssClass(WeatherObjectsLayer::ObjectKind kind)
{
  using K = WeatherObjectsLayer::ObjectKind;
  switch (kind)
  {
    case K::JetAxes:
      return "jet-axis";
    case K::TroughAxes:
      return "trough-axis";
    case K::ConvergenceLines:
      return "convergence-line";
    case K::DeformationLines:
      return "deformation-line";
    case K::VorticityLines:
      return "vorticity-line";
  }
  return "object";
}

WeatherObjectsLayer::ObjectKind parseKind(const std::string& s)
{
  using K = WeatherObjectsLayer::ObjectKind;
  if (s == "jet_axes")
    return K::JetAxes;
  if (s == "trough_axes")
    return K::TroughAxes;
  if (s == "convergence_lines")
    return K::ConvergenceLines;
  if (s == "deformation_lines")
    return K::DeformationLines;
  if (s == "vorticity_lines")
    return K::VorticityLines;
  if (s == "fronts" || s == "fronts_maxgrad" || s == "fronts_maxcurv")
    throw Fmi::Exception(BCP,
        "On-the-fly front detection is not supported via weather_objects; "
        "serve fronts from a precomputed external source instead");
  throw Fmi::Exception(BCP, "Unknown weather_objects type").addParameter("type", s);
}

// Convert an NFmiDataMatrix<float> to a contiguous Fmi::Matrix<double>.
Fmi::Matrix<double> toFmiMatrix(const NFmiDataMatrix<float>& m)
{
  const auto nx = static_cast<int>(m.NX());
  const auto ny = static_cast<int>(m.NY());
  Fmi::Matrix<double> out(nx, ny);
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i)
    {
      const float v = m[i][j];
      out(i, j) =
          (v == kFloatMissing) ? std::numeric_limits<double>::quiet_NaN() : static_cast<double>(v);
    }
  return out;
}

// Build double grid-spacing arrays from a coordinate matrix. Edge rows
// mirror the nearest interior spacing.
std::pair<Fmi::Matrix<double>, Fmi::Matrix<double>> buildGridSpacing(
    const Fmi::CoordinateMatrix& coords)
{
  const auto nx = static_cast<int>(coords.width());
  const auto ny = static_cast<int>(coords.height());
  Fmi::Matrix<double> dx(nx, ny);
  Fmi::Matrix<double> dy(nx, ny);

  auto dist = [&](int i0, int j0, int i1, int j1) {
    const double dx_ = coords.x(i1, j1) - coords.x(i0, j0);
    const double dy_ = coords.y(i1, j1) - coords.y(i0, j0);
    return std::sqrt(dx_ * dx_ + dy_ * dy_);
  };

  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i)
    {
      const int i_lo = (i == 0) ? 0 : i - 1;
      const int i_hi = (i == nx - 1) ? nx - 1 : i + 1;
      const int j_lo = (j == 0) ? 0 : j - 1;
      const int j_hi = (j == ny - 1) ? ny - 1 : j + 1;
      dx(i, j) = dist(i_lo, j, i_hi, j);
      dy(i, j) = dist(i, j_lo, i, j_hi);
    }
  return {std::move(dx), std::move(dy)};
}

// Bilinear grid-index-to-lon/lat.
std::pair<double, double> gridToLonLat(double jf,
                                       double if_,
                                       const Fmi::CoordinateMatrix& coords)
{
  const int nx = static_cast<int>(coords.width());
  const int ny = static_cast<int>(coords.height());
  jf = std::clamp(jf, 0.0, static_cast<double>(ny - 1));
  if_ = std::clamp(if_, 0.0, static_cast<double>(nx - 1));
  const int i0 = static_cast<int>(if_);
  const int j0 = static_cast<int>(jf);
  const int i1 = std::min(i0 + 1, nx - 1);
  const int j1 = std::min(j0 + 1, ny - 1);
  const double ti = if_ - i0;
  const double tj = jf - j0;
  const double x = (1 - ti) * (1 - tj) * coords.x(i0, j0) + ti * (1 - tj) * coords.x(i1, j0) +
                   (1 - ti) * tj * coords.x(i0, j1) + ti * tj * coords.x(i1, j1);
  const double y = (1 - ti) * (1 - tj) * coords.y(i0, j0) + ti * (1 - tj) * coords.y(i1, j0) +
                   (1 - ti) * tj * coords.y(i0, j1) + ti * tj * coords.y(i1, j1);
  return {x, y};
}

// Build a polyline's `d` attribute in SVG path syntax.
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

// Parse one object specification from JSON.
WeatherObjectsLayer::ObjectSpec parseObject(Json::Value& obj)
{
  if (!obj.isObject())
    throw Fmi::Exception(BCP, "objects[] entry must be an object");

  WeatherObjectsLayer::ObjectSpec spec;
  std::string type;
  JsonTools::remove_string(type, obj, "type");
  if (type.empty())
    throw Fmi::Exception(BCP, "objects[] entry missing 'type'");
  spec.kind = parseKind(type);
  spec.css_class = defaultCssClass(spec.kind);

  JsonTools::remove_string(spec.u, obj, "u");
  JsonTools::remove_string(spec.v, obj, "v");
  JsonTools::remove_string(spec.producer, obj, "producer");
  JsonTools::remove_int(spec.level, obj, "level");

  int smoothing = -1;
  JsonTools::remove_int(smoothing, obj, "smoothing_passes");
  spec.line_options.smoothing_passes = smoothing;

  int mp = static_cast<int>(spec.line_options.max_points);
  int ml = static_cast<int>(spec.line_options.max_lines);
  JsonTools::remove_int(mp, obj, "max_points");
  JsonTools::remove_int(ml, obj, "max_lines");
  spec.line_options.max_points = static_cast<std::size_t>(mp);
  spec.line_options.max_lines = static_cast<std::size_t>(ml);

  JsonTools::remove_string(spec.css_class, obj, "css_class");
  JsonTools::remove_string(spec.css_file, obj, "css_file");

  if (!obj.isNull() && !obj.empty())
    throw Fmi::Exception(BCP, "Unknown keys in weather_objects entry")
        .addParameter("remaining", obj.toStyledString());

  return spec;
}
}  // namespace

// ---------------------------------------------------------------------------

void WeatherObjectsLayer::init(Json::Value& theJson,
                               const State& theState,
                               const Config& theConfig,
                               const Properties& theProperties)
{
  try
  {
    Layer::init(theJson, theState, theConfig, theProperties);

    auto objs = JsonTools::remove(theJson, "objects");
    if (objs.isNull() || !objs.isArray())
      throw Fmi::Exception(BCP, "weather_objects requires an 'objects' array");

    for (auto& o : objs)
      itsObjects.push_back(parseObject(o));

    if (itsObjects.empty())
      throw Fmi::Exception(BCP, "weather_objects: 'objects' array must be non-empty");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherObjectsLayer::init failed");
  }
}

// ---------------------------------------------------------------------------

namespace
{
struct DetectedLine
{
  std::string css_class;
  std::vector<std::pair<double, double>> lonlat;  // WGS84
};

// Resolve Q for a given object's producer at the layer's valid time.
Engine::Querydata::Q resolveQ(const State& s,
                              const std::string& producer,
                              const Fmi::DateTime& validTime)
{
  const std::string model = producer.empty() ? s.getConfig().defaultModel() : producer;
  return s.getModel(model, validTime);
}

// Fetch a Q's WGS84 coordinates.
Fmi::CoordinateMatrix fetchWgs84Coords(const State& s, const Engine::Querydata::Q& q)
{
  static const Fmi::SpatialReference wgs84("WGS84");
  auto coords = s.getQEngine().getWorldCoordinates(q, wgs84);
  if (!coords)
    throw Fmi::Exception(BCP, "WeatherObjectsLayer: unable to fetch grid coordinates");
  return *coords;
}

std::vector<DetectedLine> runLineDetector(
    const State& s,
    const WeatherObjectsLayer::ObjectSpec& spec,
    const Fmi::DateTime& validTime,
    WeatherObjectsLayer::ObjectKind kind)
{
  auto q = resolveQ(s, spec.producer, validTime);
  if (!q)
    return {};

  auto& qe = s.getQEngine();
  std::size_t vhash = Engine::Querydata::hash_value(q);
  auto u_m = qe.getValues(q, TS::ParameterFactory::instance().parse(spec.u), vhash, validTime);
  auto v_m = qe.getValues(q, TS::ParameterFactory::instance().parse(spec.v), vhash, validTime);
  if (!u_m || !v_m)
    return {};

  auto coords = fetchWgs84Coords(s, q);
  auto u = toFmiMatrix(*u_m);
  auto v = toFmiMatrix(*v_m);
  auto [dx, dy] = buildGridSpacing(coords);

  using K = WeatherObjectsLayer::ObjectKind;
  std::vector<Fmi::Dynlib::LineFeature> lines;
  switch (kind)
  {
    case K::JetAxes:
      lines = Fmi::Dynlib::detectJetAxes(u, v, dx, dy, spec.line_options);
      break;
    case K::TroughAxes:
    case K::VorticityLines:
      lines = Fmi::Dynlib::detectVorticityLines(u, v, dx, dy, spec.line_options);
      break;
    case K::ConvergenceLines:
      lines = Fmi::Dynlib::detectConvergenceLines(u, v, dx, dy, spec.line_options);
      break;
    case K::DeformationLines:
      lines = Fmi::Dynlib::detectDeformationLines(u, v, dx, dy, spec.line_options);
      break;
  }

  std::vector<DetectedLine> out;
  out.reserve(lines.size());
  for (const auto& l : lines)
  {
    DetectedLine dl;
    dl.css_class = spec.css_class;
    dl.lonlat.reserve(l.points.size());
    for (const auto& p : l.points)
      dl.lonlat.push_back(gridToLonLat(p.j, p.i, coords));
    if (dl.lonlat.size() >= 2)
      out.push_back(std::move(dl));
  }
  return out;
}
}  // namespace

// ---------------------------------------------------------------------------

void WeatherObjectsLayer::generate(CTPP::CDT& theGlobals,
                                   CTPP::CDT& theLayersCdt,
                                   State& theState)
{
  try
  {
    const auto& box = projection.getBox();
    auto crs = projection.getCRS();
    Fmi::CoordinateTransformation transformation("WGS84", crs);

    CTPP::CDT layerGroup(CTPP::CDT::HASH_VAL);
    layerGroup["start"] = "<g";
    layerGroup["end"] = "\n</g>";
    if (!qid.empty())
      layerGroup["attributes"]["id"] = qid;
    layerGroup["attributes"]["class"] = "weather-objects";

    std::ostringstream body;

    for (const auto& spec : itsObjects)
    {
      if (!spec.css_file.empty() && css)
        theGlobals["css"][theState.getCustomer() + "/" + spec.css_file] =
            theState.getStyle(spec.css_file);

      std::vector<DetectedLine> lines =
          runLineDetector(theState, spec, getValidTime(), spec.kind);

      for (auto& dl : lines)
      {
        std::vector<double> lons, lats;
        lons.reserve(dl.lonlat.size());
        lats.reserve(dl.lonlat.size());
        for (const auto& [lon, lat] : dl.lonlat)
        {
          lons.push_back(lon);
          lats.push_back(lat);
        }
        transformation.transform(lons, lats);

        std::vector<std::pair<double, double>> screenPts;
        screenPts.reserve(lons.size());
        for (std::size_t i = 0; i < lons.size(); ++i)
        {
          double x = lons[i];
          double y = lats[i];
          box.transform(x, y);
          screenPts.emplace_back(x, y);
        }
        if (screenPts.size() < 2)
          continue;

        body << fmt::format("\n<path class=\"{}\" d=\"{}\"/>",
                            dl.css_class,
                            buildPathData(screenPts));
      }
    }

    layerGroup["end"] = body.str() + "\n</g>";
    theLayersCdt.PushBack(layerGroup);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherObjectsLayer::generate failed");
  }
}

// ---------------------------------------------------------------------------

void WeatherObjectsLayer::getFeatureInfo(CTPP::CDT& /*theInfo*/, const State& /*theState*/)
{
}

// ---------------------------------------------------------------------------

std::size_t WeatherObjectsLayer::hash_value(const State& theState) const
{
  try
  {
    std::size_t h = Layer::hash_value(theState);
    for (const auto& s : itsObjects)
    {
      Fmi::hash_combine(h, Fmi::hash_value(static_cast<int>(s.kind)));
      Fmi::hash_combine(h, Fmi::hash_value(s.u));
      Fmi::hash_combine(h, Fmi::hash_value(s.v));
      Fmi::hash_combine(h, Fmi::hash_value(s.producer));
      if (s.level)
        Fmi::hash_combine(h, Fmi::hash_value(*s.level));
      Fmi::hash_combine(h, Fmi::hash_value(s.line_options.smoothing_passes));
    }
    return h;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WeatherObjectsLayer::hash_value failed");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
