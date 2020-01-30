//======================================================================

#include "IsolabelLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoband.h"
#include "Isoline.h"
#include "Layer.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <limits>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void IsolabelLayer::init(const Json::Value& theJson,
                         const State& theState,
                         const Config& theConfig,
                         const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Isolabel-layer JSON is not a JSON object");

    IsolineLayer::init(theJson, theState, theConfig, theProperties);

    Json::Value nulljson;

    auto json = theJson.get("label", nulljson);
    if (!json.isNull())
      label.init(json, theConfig);

    json = theJson.get("upright", nulljson);
    if (!json.isNull())
      upright = json.asBool();

    json = theJson.get("max_angle", nulljson);
    if (!json.isNull())
      max_angle = json.asDouble();

    json = theJson.get("min_distance_edge", nulljson);
    if (!json.isNull())
      min_distance_edge = json.asDouble();

    json = theJson.get("max_distance_edge", nulljson);
    if (!json.isNull())
      max_distance_edge = json.asDouble();

    json = theJson.get("max_curvature", nulljson);
    if (!json.isNull())
      max_curvature = json.asDouble();

    json = theJson.get("isobands", nulljson);
    if (!json.isNull())
    {
      std::vector<Isoband> isobands;
      Spine::JSON::extract_array("isobands", isobands, json, theConfig);
      for (const auto& isoband : isobands)
      {
        if (isoband.lolimit)
          isovalues.push_back(*isoband.lolimit);
        if (isoband.hilimit)
          isovalues.push_back(*isoband.hilimit);
      }
    }

    json = theJson.get("isovalues", nulljson);
    if (!json.isNull())
    {
      if (json.isArray())
      {
        // [ a, b, c, d, ... ]
        for (unsigned int i = 0; i < json.size(); i++)
          isovalues.push_back(json[i].asDouble());
      }
      else if (json.isObject())
      {
        // { start, stop, step=1 }

        auto start = json.get("start", nulljson);
        auto stop = json.get("stop", nulljson);
        auto step = json.get("step", nulljson);
        if (start.isNull() || stop.isNull())
          throw Spine::Exception(BCP, "Isolabel-layer isovalues start or stop setting missing");

        double iso1 = start.asDouble();
        double iso2 = stop.asDouble();
        double isostep = (step.isNull() ? 1.0 : step.asDouble());

        if (iso2 < iso1)
          throw Spine::Exception(
              BCP, "Isolabel-layer isovalues stop value must be greater than the start value");
        if (isostep < 0)
          throw Spine::Exception(BCP, "Isolabel-layer step value must be greater than zero");
        if ((iso2 - iso1) / isostep > 1000)
          throw Spine::Exception(BCP, "Isolabel-layer generates to many isovalues (> 1000)");

        // The end condition is used to make sure we do not get both 999.99999 and 1000.000 due to
        // numerical inaccuracies.
        for (double value = iso1; value < iso2 - isostep / 2; value += isostep)
          isovalues.push_back(value);
        isovalues.push_back(iso2);
      }
      else
        throw Spine::Exception(
            BCP, "Isolabel-layer isovalues setting must be an object or a vector of numbers");
    }

    // Add isovalues from the parent IsolineLayer class

    for (const auto& isoline : isolines)
      isovalues.push_back(isoline.value);

    // Make sure there are only unique isovalues since there are multiple ways to define them.
    // In particular using "isobands" will almost certainly produce duplicates

    std::sort(isovalues.begin(), isovalues.end());
    isovalues.erase(std::unique(isovalues.begin(), isovalues.end()), isovalues.end());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void IsolabelLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::string report = "IsolabelLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    auto geoms = IsolineLayer::getIsolines(isovalues, theState);

    // The above call guarantees these have been resolved:
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Project the geometries to pixel coordinates
    for (auto& geomptr : geoms)
    {
      if (geomptr && geomptr->IsEmpty() == 0)
        Fmi::OGR::transform(*geomptr, box);
    }

    // Generate candidate locations from the isolines

    auto candidates = find_candidates(geoms);

    // Select the actual label locations from the candidates

    candidates = select_best_candidates(candidates, box);

    // Fix the label orientations so numbers indicate in which direction the values increase.
    // In practise we just rotate the numbers 180 degrees if they seem to point to the wrong
    // direction, and then also apply "upright" condition if so requested.

    if (label.orientation == "auto")
      fix_orientation(candidates, box, *crs);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate isolabels as use tags statements inside <g>..</g>, but we add the </g> only after
    // the labels

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Add attributes to the group, not to the labels
    theState.addAttributes(theGlobals, group_cdt, attributes);

    theLayersCdt.PushBack(group_cdt);

    for (unsigned int i = 0; i < candidates.size(); i++)
    {
      const auto& points = candidates[i];

      if (points.empty())
        continue;

      const auto value = isovalues[i];
      const auto txt = label.print(value);

      if (txt.empty())
        continue;

      // Generate a text tag for each point

      for (const auto& point : points)
      {
        if (std::isnan(point.angle))
          continue;

        CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
        text_cdt["start"] = "<text";
        text_cdt["end"] = "</text>";
        text_cdt["cdata"] = txt;

        if (label.orientation == "auto")
        {
          const auto radians = point.angle * M_PI / 180;
          const auto srad = sin(radians);
          const auto crad = cos(radians);
          auto transform = fmt::format("translate({} {}) rotate({})",
                                       std::round(point.x + label.dx * crad + label.dy * srad),
                                       std::round(point.y + label.dx * srad - label.dy * crad),
                                       std::round(point.angle));
          text_cdt["attributes"]["transform"] = transform;
        }
        else
        {
          auto transform = fmt::format(
              "translate({} {})", std::round(point.x + label.dx), std::round(point.y - label.dy));
          text_cdt["attributes"]["transform"] = transform;
        }

        theLayersCdt.PushBack(text_cdt);
      }
    }
    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

std::vector<std::size_t> find_max_positions(const std::vector<double>& values, bool is_closed)
{
  std::vector<std::size_t> positions;

  const auto n = values.size();

  // Find positions which look like local minima over +-5 points
  const int minlen = 5;

  const int startpos = (is_closed ? 0 : minlen);
  const int endpos = (is_closed ? n - 1 : n - 1 - minlen);

  // Note that i%n would not work as intended below for negative i, hence +n
  for (int pos = startpos; pos <= endpos; ++pos)
  {
    bool ok = true;
    for (int i = 1; ok && i <= minlen; ++i)
    {
      const auto prev = (pos - i + n) % n;
      const auto next = (pos + i + n) % n;
      ok = (values[pos] < values[prev] && values[pos] < values[next]);
    }
    if (ok)
      positions.push_back(pos);
  }

  return positions;
}

// ----------------------------------------------------------------------

double curvature(const OGRLineString* geom, int pos)
{
  const int minlen = 5;  // measure over +-5 points

  const auto n = geom->getNumPoints();

  const auto minpos = (pos < minlen ? 0 : pos - minlen);
  const auto maxpos = (pos > n - minlen - 1 ? n - 1 : pos + minlen);

  // Cannot estimate unless we have at least a triangle
  if (maxpos - minpos < 3)
    return 999;

  double last_angle = 0;
  double sum = 0;

  for (int i = minpos; i <= maxpos - 1; ++i)
  {
    double angle =
        180 / M_PI * atan2(geom->getY(i + 1) - geom->getY(i), geom->getX(i + 1) - geom->getX(i));

    if (i > minpos)
    {
      // Add turn angle along the shortest route
      auto diff = angle - last_angle;
      if (diff < -180)
        diff += 360;
      else if (diff > 180)
        diff -= 360;
      sum += std::abs(diff);
      last_angle = angle;
    }

    last_angle = angle;
  }

  return sum;
}

void get_linestring_points(CandidateList& candidates,
                           double sine,
                           double cosine,
                           const OGRLineString* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;

  // Gather rotated coordinates
  std::vector<double> ycoords;
  const auto n = geom->getNumPoints();
  for (int i = 0; i < n; ++i)
    ycoords.push_back(sine * geom->getX(i) + cosine * geom->getY(i));

  const bool is_closed = (geom->getX(0) == geom->getX(n - 1) && geom->getY(0) == geom->getY(n - 1));

  auto positions = find_max_positions(ycoords, is_closed);

  for (const auto pos : positions)
  {
    double x2 = geom->getX(pos);
    double y2 = geom->getY(pos);

    double x1 = geom->getX((pos - 1) % n);
    double y1 = geom->getY((pos - 1) % n);

    double x3 = geom->getX((pos + 1) % n);
    double y3 = geom->getY((pos + 1) % n);

    auto angle = 0.5 * (atan2(y3 - y2, x3 - x2) + atan2(y2 - y1, x2 - x1));
    angle *= 180 / M_PI;

    auto curv = curvature(geom, pos);

    candidates.emplace_back(Candidate{x2, y2, angle, curv});
  }
}

void get_linearring_points(CandidateList& candidates,
                           double sine,
                           double cosine,
                           const OGRLinearRing* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;

  // Gather rotated coordinates
  std::vector<double> ycoords;
  const auto n = geom->getNumPoints();
  for (int i = 0; i < n; ++i)
    ycoords.push_back(sine * geom->getX(i) + cosine * geom->getY(i));

  const auto is_closed = true;

  auto positions = find_max_positions(ycoords, is_closed);

  for (const auto pos : positions)
  {
    double x2 = geom->getX(pos);
    double y2 = geom->getY(pos);

    double x1 = geom->getX((pos - 1) % n);
    double y1 = geom->getY((pos - 1) % n);

    double x3 = geom->getX((pos + 1) % n);
    double y3 = geom->getY((pos + 1) % n);

    auto angle = 0.5 * (atan2(y3 - y2, x3 - x2) + atan2(y2 - y1, x2 - x1));
    angle *= 180 / M_PI;

    auto curv = curvature(geom, pos);

    candidates.emplace_back(Candidate{x2, y2, angle, curv});
  }
}

void get_polygon_points(CandidateList& candidates,
                        double sine,
                        double cosine,
                        const OGRPolygon* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  get_linearring_points(candidates, cosine, sine, geom->getExteriorRing());
}

void get_multilinestring_points(CandidateList& candidates,
                                double sine,
                                double cosine,
                                const OGRMultiLineString* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    get_linestring_points(
        candidates, sine, cosine, dynamic_cast<const OGRLineString*>(geom->getGeometryRef(i)));
}

void get_multipolygon_points(CandidateList& candidates,
                             double sine,
                             double cosine,
                             const OGRMultiPolygon* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    get_polygon_points(
        candidates, sine, cosine, dynamic_cast<const OGRPolygon*>(geom->getGeometryRef(i)));
}

void get_points(CandidateList& candidates, double sine, double cosine, const OGRGeometry* geom);

void get_geometrycollection_points(CandidateList& candidates,
                                   double sine,
                                   double cosine,
                                   const OGRGeometryCollection* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    get_points(candidates, sine, cosine, geom->getGeometryRef(i));
}

void get_points(CandidateList& candidates, double sine, double cosine, const OGRGeometry* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;

  auto id = geom->getGeometryType();
  switch (id)
  {
    case wkbLineString:
      return get_linestring_points(
          candidates, sine, cosine, dynamic_cast<const OGRLineString*>(geom));
    case wkbLinearRing:
      return get_linearring_points(
          candidates, sine, cosine, dynamic_cast<const OGRLinearRing*>(geom));
    case wkbPolygon:
      return get_polygon_points(candidates, sine, cosine, dynamic_cast<const OGRPolygon*>(geom));
    case wkbMultiLineString:
      return get_multilinestring_points(
          candidates, sine, cosine, dynamic_cast<const OGRMultiLineString*>(geom));
    case wkbMultiPolygon:
      return get_multipolygon_points(
          candidates, sine, cosine, dynamic_cast<const OGRMultiPolygon*>(geom));
    case wkbGeometryCollection:
      return get_geometrycollection_points(
          candidates, sine, cosine, dynamic_cast<const OGRGeometryCollection*>(geom));
    default:
      break;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Given geometries select candidate locations for labels
 *
 * For all selected angles, rotate the isoline by the angle, and find
 * locations of local min/max in the Y-coordinate.
 *
 * The angles are in order of preference, hence we select candidates
 * for the first angle first.
 */
// ----------------------------------------------------------------------

std::vector<CandidateList> IsolabelLayer::find_candidates(const std::vector<OGRGeometryPtr>& geoms)
{
  std::vector<CandidateList> ret;

  for (std::size_t i = 0; i < geoms.size(); i++)
  {
    CandidateList candidates;
    OGRGeometryPtr geom = geoms[i];
    if (geom && geom->IsEmpty() == 0)
    {
      for (auto angle : angles)
      {
        const auto radians = angle * M_PI / 180;
        const auto cosine = cos(radians);
        const auto sine = sin(radians);

        get_points(candidates, sine, cosine, geom.get());
      }
    }

    ret.emplace_back(candidates);
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Distance of candidate to image edge
 *
 * We assume the candidate is inside the image so that we do not bother
 * calculating true distances to the corners even though they might be
 * the closest points.
 */
// ----------------------------------------------------------------------

double distance(const Candidate& candidate, const Fmi::Box& box)
{
  const auto dleft = std::abs(candidate.x);
  const auto dright = std::abs(candidate.x - box.width());
  const auto dtop = std::abs(candidate.y);
  const auto dbottom = std::abs(candidate.y - box.height());

  const auto dx = std::min(dleft, dright);
  const auto dy = std::min(dtop, dbottom);

  return std::min(dx, dy);
}

// ----------------------------------------------------------------------
/*!
 * \brief Given candidates for each isoline select the best combination of them
 */
// ----------------------------------------------------------------------

std::vector<CandidateList> IsolabelLayer::select_best_candidates(
    const std::vector<CandidateList>& candidates, const Fmi::Box& box) const
{
  std::vector<CandidateList> ret;

  // Discard too angled labels and labels too close or far from the edges
  for (const auto& cands : candidates)
  {
    CandidateList ok_cands;
    for (const auto& point : cands)
    {
      // Angle condition
      bool ok = (point.angle >= -max_angle and point.angle <= max_angle);
      // Edge conditions
      if (ok)
      {
        const auto dist = distance(point, box);
        ok = (dist >= min_distance_edge && dist <= max_distance_edge);
      }
      if (ok)
        ok = (point.curvature < max_curvature);

      if (ok)
        ok_cands.push_back(point);
    }
    ret.push_back(ok_cands);
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * Fix label orientations by peeking at querydata values
 */
// ----------------------------------------------------------------------

void IsolabelLayer::fix_orientation(std::vector<CandidateList>& candidates,
                                    const Fmi::Box& box,
                                    OGRSpatialReference& crs) const
{
  // The parameter being used
  auto param = Spine::ParameterFactory::instance().parse(*parameter);

  boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
  boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
  boost::local_time::local_date_time localdatetime(getValidTime(), utc);
  auto mylocale = std::locale::classic();
  NFmiPoint dummy;

  // Querydata spatial reference
  std::unique_ptr<OGRSpatialReference> qcrs(new OGRSpatialReference());
  OGRErr err = qcrs->SetFromUserInput(q->area().WKT().c_str());

  if (err != OGRERR_NONE)
    throw Spine::Exception(BCP, "GDAL does not understand this FMI WKT: " + q->area().WKT());

  // From image world coordinates to querydata world coordinates

  std::unique_ptr<OGRCoordinateTransformation> transformation(
      OGRCreateCoordinateTransformation(&crs, qcrs.get()));

  if (transformation == nullptr)
    throw Spine::Exception(BCP,
                           "Failed to create coordinate transformation for orienting isolabels");

  // Check and fix orientations for each isovalue
  for (std::size_t i = 0; i < candidates.size(); i++)
  {
    const auto isovalue = isovalues[i];

    for (auto& candidate : candidates[i])
    {
      const int length = 2;  // move in pixel units in the orientation marked for the label

      auto x = candidate.x + length * sin(candidate.angle * M_PI / 180);
      auto y = candidate.y - length * cos(candidate.angle * M_PI / 180);

      box.itransform(x, y);  // world xy coordinate in image crs

      if (transformation->Transform(1, &x, &y) == 0)
      {
        candidate.angle = std::numeric_limits<double>::quiet_NaN();
      }
      else
      {
        // Must convert to native geocentric coordinates since the API does not support WorldXY
        // interpolation
        NFmiPoint latlon = q->area().WorldXYToLatLon(NFmiPoint(x, y));

        // Q API SUCKS!!
        Spine::Location loc(latlon.X(), latlon.Y());
        Engine::Querydata::ParameterOptions options(
            param, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);

        auto result = q->value(options, localdatetime);

        double tmp = isovalue;

        if (boost::get<double>(&result) != nullptr)
          tmp = *boost::get<double>(&result);
        else if (boost::get<int>(&result) != nullptr)
          tmp = *boost::get<int>(&result);
        else
          candidate.angle = std::numeric_limits<double>::quiet_NaN();

        if (tmp < isovalue)
        {
          candidate.angle = candidate.angle + 180;
          if (candidate.angle > 360)
            candidate.angle -= 360;
        }

        // Force labels upright if so requested
        if (upright && (candidate.angle < -90 || candidate.angle > 90))
        {
          candidate.angle += 180;
          if (candidate.angle > 360)
            candidate.angle -= 360;
        }
      }
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t IsolabelLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = IsolineLayer::hash_value(theState);
    Dali::hash_combine(hash, Dali::hash_value(label, theState));
    for (auto& angle : angles)
      Dali::hash_combine(hash, Dali::hash_value(angle));
    Dali::hash_combine(hash, upright);
    Dali::hash_combine(hash, max_angle);
    Dali::hash_combine(hash, min_distance_other);
    Dali::hash_combine(hash, max_distance_other);
    Dali::hash_combine(hash, min_distance_self);
    Dali::hash_combine(hash, min_distance_edge);
    Dali::hash_combine(hash, max_distance_edge);
    Dali::hash_combine(hash, max_curvature);
    Dali::hash_combine(hash, Dali::hash_value(isovalues));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
