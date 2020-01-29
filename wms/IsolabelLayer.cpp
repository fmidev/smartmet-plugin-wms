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

    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      symbol = json.asString();

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

    candidates = select_best_candidates(candidates);

    // Fix the label orientations so numbers indicate in which direction the values increase.
    // In practise we just rotate the numbers 180 degrees if they seem to point to the wrong
    // direction.

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

        const auto radians = point.angle * M_PI / 180;
        const auto srad = sin(radians);
        const auto crad = cos(radians);
        auto transform = fmt::format("translate({} {}) rotate({})",
                                     std::round(point.x + label.dx * crad + label.dy * srad),
                                     std::round(point.y - label.dx * srad + label.dy * crad),
                                     std::round(point.angle));
        text_cdt["attributes"]["transform"] = transform;

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
    Dali::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    for (auto& angle : angles)
      Dali::hash_combine(hash, Dali::hash_value(angle));
    Dali::hash_combine(hash, max_total_count);
    Dali::hash_combine(hash, min_distance_other);
    Dali::hash_combine(hash, min_distance_self);
    Dali::hash_combine(hash, min_distance_edge);
    Dali::hash_combine(hash, max_distance_edge);
    Dali::hash_combine(hash, max_curvature);
    Dali::hash_combine(hash, curvature_length);
    Dali::hash_combine(hash, Dali::hash_value(isovalues));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------

void get_linestring_points(CandidateList& candidates, const OGRLineString* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;

  for (int i = 5, n = geom->getNumPoints(); i < n - 1; i += 10)
  {
    double x2 = geom->getX(i);
    double y2 = geom->getY(i);

    double x1 = geom->getX(i - 1);
    double y1 = geom->getY(i - 1);

    double x3 = geom->getX(i + 1);
    double y3 = geom->getY(i + 1);

    auto angle = 0.5 * (atan2(y3 - y2, x3 - x2) + atan2(y2 - y1, x2 - x1));
    angle *= 180 / M_PI;

    candidates.emplace_back(Candidate{x2, y2, angle});
  }
}

void get_linearring_points(CandidateList& candidates, const OGRLinearRing* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;

  for (int i = 5, n = geom->getNumPoints(); i < n - 1; i += 10)
  {
    double x2 = geom->getX(i);
    double y2 = geom->getY(i);

    double x1 = geom->getX(i - 1);
    double y1 = geom->getY(i - 1);

    double x3 = geom->getX(i + 1);
    double y3 = geom->getY(i + 1);

    auto angle = 0.5 * (atan2(y3 - y2, x3 - x2) + atan2(y2 - y1, x2 - x1));
    angle *= 180 / M_PI;

    candidates.emplace_back(Candidate{x2, y2, angle});
  }
}

void get_polygon_points(CandidateList& candidates, const OGRPolygon* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  get_linearring_points(candidates, geom->getExteriorRing());
}

void get_multilinestring_points(CandidateList& candidates, const OGRMultiLineString* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    get_linestring_points(candidates, dynamic_cast<const OGRLineString*>(geom->getGeometryRef(i)));
}

void get_multipolygon_points(CandidateList& candidates, const OGRMultiPolygon* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    get_polygon_points(candidates, dynamic_cast<const OGRPolygon*>(geom->getGeometryRef(i)));
}

void get_points(CandidateList& candidates, const OGRGeometry* geom);

void get_geometrycollection_points(CandidateList& candidates, const OGRGeometryCollection* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    get_points(candidates, geom->getGeometryRef(i));
}

void get_points(CandidateList& candidates, const OGRGeometry* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;

  auto id = geom->getGeometryType();
  switch (id)
  {
    case wkbLineString:
      return get_linestring_points(candidates, dynamic_cast<const OGRLineString*>(geom));
    case wkbLinearRing:
      return get_linearring_points(candidates, dynamic_cast<const OGRLinearRing*>(geom));
    case wkbPolygon:
      return get_polygon_points(candidates, dynamic_cast<const OGRPolygon*>(geom));
    case wkbMultiLineString:
      return get_multilinestring_points(candidates, dynamic_cast<const OGRMultiLineString*>(geom));
    case wkbMultiPolygon:
      return get_multipolygon_points(candidates, dynamic_cast<const OGRMultiPolygon*>(geom));
    case wkbGeometryCollection:
      return get_geometrycollection_points(candidates,
                                           dynamic_cast<const OGRGeometryCollection*>(geom));
    default:
      break;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Given geometries select candidate locations for labels
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
      // Select all points for now
      get_points(candidates, geom.get());
    }

    ret.emplace_back(candidates);
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Given candidates for each isoline select the best combination of them
 */
// ----------------------------------------------------------------------

std::vector<CandidateList> IsolabelLayer::select_best_candidates(
    const std::vector<CandidateList>& candidates) const
{
  return candidates;  // TODO
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
      }
    }
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
