//======================================================================

#include "IsolabelLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoband.h"
#include "Isoline.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include <boost/logic/tribool.hpp>
#include <boost/move/make_unique.hpp>
#include <optional>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/BoolMatrix.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <limits>
#include <vector>

// #define MYDEBUG 1
// #define MYDEBUG_DETAILS 1

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// Label status
enum class Status
{
  Undecided,
  Accepted,
  Rejected
};

// For minimum spanning tree searches
struct Edge
{
  std::size_t first = 0;
  std::size_t second = 0;
  double length = 0;
  double weight = 0;

  // needed for emplace_back to work
  Edge(std::size_t f, std::size_t s, double l, double w) : first(f), second(s), length(l), weight(w)
  {
  }
};

// Data structures needed for Kruskal's algorithm for minimum spanning tree:

using Edges = std::vector<Edge>;
using Statuses = std::vector<Status>;      // Status of each candidate
using Parents = std::vector<std::size_t>;  // Parent for each candidate

// The algorithm here places minimum length limits for edges. For each candidate we list
// the candidates it cannot connect to. If the candidate is selected, the ones in the
// list will be rejected from the minimum spanning tree.
using BadPairList = std::list<std::size_t>;
using BadPairs = std::vector<BadPairList>;

// If we need a fast test to see whether a particular pair of labels is disallowed,
// we could also use a BoolMatrix of size N*N to mark individual disabled pairs
//
// using BadPairMatrix = Fmi::BoolMatrix;

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void IsolabelLayer::init(Json::Value& theJson,
                         const State& theState,
                         const Config& theConfig,
                         const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isolabel-layer JSON is not a JSON object");

    IsolineLayer::init(theJson, theState, theConfig, theProperties);

    auto json = JsonTools::remove(theJson, "label");
    label.init(json, theConfig);

    JsonTools::remove_bool(upright, theJson, "upright");
    JsonTools::remove_double(max_angle, theJson, "max_angle");
    JsonTools::remove_double(min_isoline_length, theJson, "min_isoline_length");
    JsonTools::remove_double(min_distance_edge, theJson, "min_distance_edge");
    JsonTools::remove_double(max_distance_edge, theJson, "max_distance_edge");
    JsonTools::remove_double(min_distance_other, theJson, "min_distance_other");
    JsonTools::remove_double(min_distance_same, theJson, "min_distance_same");
    JsonTools::remove_double(min_distance_self, theJson, "min_distance_self");
    JsonTools::remove_double(max_curvature, theJson, "max_curvature");
    JsonTools::remove_int(stencil_size, theJson, "stencil_size");

    json = JsonTools::remove(theJson, "angles");
    if (!json.isNull())
      JsonTools::extract_vector("angles", angles, json);

    // Note that from now on we generate all possible isoline values and keep the unique ones only
    // at the end. A std::set would have been an alternative solution.

    json = JsonTools::remove(theJson, "isobands");
    if (!json.isNull())
    {
      std::vector<Isoband> isobands;
      JsonTools::extract_array("isobands", isobands, json, theConfig);
      for (const auto& isoband : isobands)
      {
        if (isoband.lolimit)
          isovalues.push_back(*isoband.lolimit);
        if (isoband.hilimit)
          isovalues.push_back(*isoband.hilimit);
      }
    }

    json = JsonTools::remove(theJson, "isovalues");
    if (!json.isNull())
    {
      if (json.isArray())
      {
        // [ a, b, c, d, ... ]
        for (const auto& j : json)
          isovalues.push_back(j.asDouble());
      }
      else if (json.isObject())
      {
        // { start, stop, step=1 }

        std::optional<double> start;
        std::optional<double> stop;
        double step = 1.0;
        JsonTools::remove_double(start, json, "start");
        JsonTools::remove_double(stop, json, "stop");
        JsonTools::remove_double(step, json, "step");

        if (!start || !stop)
          throw Fmi::Exception(BCP, "Isolabel-layer isovalues start or stop setting missing");

        if (!json.empty())
        {
          auto names = json.getMemberNames();
          auto namelist = boost::algorithm::join(names, ",");
          throw Fmi::Exception(
              BCP, "Unknown member variables in Isolabel layer isovalues setting: " + namelist);
        }

        double iso1 = *stop;
        double iso2 = *start;

        if (iso2 < iso1)
          throw Fmi::Exception(
              BCP, "Isolabel-layer isovalues stop value must be greater than the start value");
        if (step < 0)
          throw Fmi::Exception(BCP, "Isolabel-layer step value must be greater than zero");
        if ((iso2 - iso1) / step > 1000)
          throw Fmi::Exception(BCP, "Isolabel-layer generates to many isovalues (> 1000)");

        // The end condition is used to make sure we do not get both 999.99999 and 1000.000 due to
        // numerical inaccuracies.
        double value = iso1;
        while (value < iso2 - step / 2)
        {
          isovalues.push_back(value);
          value += step;
        }
        isovalues.push_back(iso2);
      }
      else
        throw Fmi::Exception(
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    if (isolines.empty())
      return;

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IsolabelLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    auto geoms = IsolineLayer::getIsolines(isovalues, theState);

    // Output image CRS and BBOX
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Convert filter pixel distance to metric distance for smoothing
    filter.bbox(box);

    // Smoothen the isolines
    filter.apply(geoms, false);

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
      fix_orientation(candidates, box, theState, crs);

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

    // Add attributes to the group, not to the labels
    theState.addAttributes(theGlobals, group_cdt, attributes);

    theLayersCdt.PushBack(group_cdt);

    for (const auto& point : candidates)
    {
      const auto txt = label.print(point.isovalue);

      if (txt.empty())
        continue;

      // Generate a text tag for each point

      if (std::isnan(point.angle))
        continue;

      CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
      text_cdt["start"] = "<text";
      text_cdt["end"] = "</text>";
      text_cdt["cdata"] = txt;

      // Assign isoline styles for the point
      for (auto i = 0UL; i < geoms.size(); i++)
      {
        const Isoline& isoline = isolines[i];
        if (isoline.value == point.isovalue)
        {
          theState.addPresentationAttributes(text_cdt, css, attributes, isoline.attributes);
          theState.addAttributes(theGlobals, text_cdt, isoline.attributes);
          break;
        }
      }

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

    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

// ----------------------------------------------------------------------

std::vector<std::size_t> find_max_positions(const std::vector<double>& values,
                                            bool is_closed,
                                            int stencil_size)
{
  std::vector<std::size_t> positions;

  const auto n = values.size();

  // Find positions which look like local minima over +-5 stencil_size points

  const int startpos = (is_closed ? 0 : stencil_size);
  const int endpos = (is_closed ? n - 1 : n - 1 - stencil_size);

  // Note that i%n would not work as intended below for negative i, hence +n
  for (int pos = startpos; pos <= endpos; ++pos)
  {
    bool ok = true;
    for (int i = 1; ok && i <= stencil_size; ++i)
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

double curvature(const OGRLineString* geom, int pos, int stencil_size)
{
  const auto n = geom->getNumPoints();

  const auto minpos = (pos < stencil_size ? 0 : pos - stencil_size);
  const auto maxpos = (pos > n - stencil_size - 1 ? n - 1 : pos + stencil_size);

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
    }

    last_angle = angle;
  }

  return sum;
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRLineString* geom,
                     double min_isoline_length)
{
  if (geom == nullptr || geom->IsEmpty() != 0 || geom->get_Length() < min_isoline_length)
    return;

  // Gather rotated coordinates
  const auto n = geom->getNumPoints();
  std::vector<double> ycoords;
  ycoords.reserve(n);
  for (int i = 0; i < n; ++i)
    ycoords.push_back(sine * geom->getX(i) + cosine * geom->getY(i));

  const bool is_closed = (geom->getX(0) == geom->getX(n - 1) && geom->getY(0) == geom->getY(n - 1));

  auto positions = find_max_positions(ycoords, is_closed, stencil_size);

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

    auto curv = curvature(geom, pos, stencil_size);

    candidates.emplace_back(isovalue, x2, y2, angle, curv, id, 0);
  }
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRLinearRing* geom,
                     double min_isoline_length)
{
  if (geom == nullptr || geom->IsEmpty() != 0 || geom->get_Length() < min_isoline_length)
    return;

  // Gather rotated coordinates
  const auto n = geom->getNumPoints();
  std::vector<double> ycoords;
  ycoords.reserve(n);
  for (int i = 0; i < n; ++i)
    ycoords.push_back(sine * geom->getX(i) + cosine * geom->getY(i));

  const auto is_closed = true;

  auto positions = find_max_positions(ycoords, is_closed, stencil_size);

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

    auto curv = curvature(geom, pos, stencil_size);

    candidates.emplace_back(isovalue, x2, y2, angle, curv, id, 0);
  }
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRPolygon* geom,
                     double min_isoline_length)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  find_candidates(candidates,
                  isovalue,
                  id,
                  sine,
                  cosine,
                  stencil_size,
                  geom->getExteriorRing(),
                  min_isoline_length);
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRMultiLineString* geom,
                     double min_isoline_length)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    find_candidates(candidates,
                    isovalue,
                    ++id,
                    sine,
                    cosine,
                    stencil_size,
                    dynamic_cast<const OGRLineString*>(geom->getGeometryRef(i)),
                    min_isoline_length);
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRMultiPolygon* geom,
                     double min_isoline_length)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    find_candidates(candidates,
                    isovalue,
                    ++id,
                    sine,
                    cosine,
                    stencil_size,
                    dynamic_cast<const OGRPolygon*>(geom->getGeometryRef(i)),
                    min_isoline_length);
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRGeometry* geom,
                     double min_isoline_length);

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRGeometryCollection* geom,
                     double min_isoline_length)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    find_candidates(candidates,
                    isovalue,
                    ++id,
                    sine,
                    cosine,
                    stencil_size,
                    geom->getGeometryRef(i),
                    min_isoline_length);
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRGeometry* geom,
                     double min_isoline_length)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;

  switch (geom->getGeometryType())
  {
    case wkbLineString:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRLineString*>(geom),
                             min_isoline_length);
    case wkbLinearRing:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRLinearRing*>(geom),
                             min_isoline_length);
    case wkbPolygon:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRPolygon*>(geom),
                             min_isoline_length);
    case wkbMultiLineString:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRMultiLineString*>(geom),
                             min_isoline_length);
    case wkbMultiPolygon:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRMultiPolygon*>(geom),
                             min_isoline_length);
    case wkbGeometryCollection:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRGeometryCollection*>(geom),
                             min_isoline_length);
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

Candidates IsolabelLayer::find_candidates(const std::vector<OGRGeometryPtr>& geoms) const
{
  Candidates candidates;

  int id = 0;
  for (std::size_t i = 0; i < geoms.size(); i++)
  {
    const OGRGeometryPtr& geom = geoms[i];
    if (geom && geom->IsEmpty() == 0)
    {
      const auto old_id = id;
      for (auto angle : angles)
      {
        id = old_id;  // use same id for different angles
        const auto radians = angle * M_PI / 180;
        const auto cosine = cos(radians);
        const auto sine = sin(radians);

        Dali::find_candidates(candidates,
                              isovalues[i],
                              id,
                              sine,
                              cosine,
                              stencil_size,
                              geom.get(),
                              min_isoline_length);
      }
    }
  }

  return candidates;
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
 * Fix label orientations by peeking at querydata values
 */
// ----------------------------------------------------------------------

void IsolabelLayer::fix_orientation(Candidates& candidates,
                                    const Fmi::Box& box,
                                    const State& state,
                                    const Fmi::SpatialReference& crs) const
{
  if (source && *source == "grid")
  {
    fix_orientation_gridEngine(candidates, box, state, crs);
    return;
  }

  // The parameter being used
  auto param = TS::ParameterFactory::instance().parse(*parameter);

  std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
  Fmi::LocalDateTime localdatetime(getValidTime(), Fmi::TimeZonePtr::utc);
  auto mylocale = std::locale::classic();
  NFmiPoint dummy;

  // Querydata spatial reference
  Fmi::SpatialReference qcrs(q->SpatialReference());

  // From image world coordinates to querydata world coordinates

  Fmi::CoordinateTransformation transformation(crs, qcrs);

  // Check and fix orientations for each isovalue

  for (auto& cand : candidates)
  {
    const int length = 2;  // move in pixel units in the orientation marked for the label

    auto x = cand.x + length * sin(cand.angle * M_PI / 180);
    auto y = cand.y - length * cos(cand.angle * M_PI / 180);

    box.itransform(x, y);  // world xy coordinate in image crs

    if (!transformation.transform(x, y))
    {
      cand.angle = std::numeric_limits<double>::quiet_NaN();
    }
    else
    {
      // Must convert to native geocentric coordinates since the API does not support WorldXY
      // interpolation
      NFmiPoint latlon = q->area().WorldXYToLatLon(NFmiPoint(x, y));

      // Q API SUCKS!!
      Spine::Location loc(latlon.X(), latlon.Y());
      Engine::Querydata::ParameterOptions options(param,
                                                  "",
                                                  loc,
                                                  "",
                                                  "",
                                                  *timeformatter,
                                                  "",
                                                  "",
                                                  mylocale,
                                                  "",
                                                  false,
                                                  dummy,
                                                  dummy);

      auto result = q->value(options, localdatetime);

      double tmp = cand.isovalue;

      if (const double* ptr = std::get_if<double>(&result))
        tmp = *ptr;
      else if (const int* ptr = std::get_if<int>(&result))
        tmp = *ptr;
      else
        cand.angle = std::numeric_limits<double>::quiet_NaN();

      if (tmp < cand.isovalue)
      {
        cand.angle = cand.angle + 180;
        if (cand.angle > 360)
          cand.angle -= 360;
      }

      // Force labels upright if so requested
      if (upright && (cand.angle < -90 || cand.angle > 90))
      {
        cand.angle += 180;
        if (cand.angle > 360)
          cand.angle -= 360;
      }
    }
  }
}

void IsolabelLayer::fix_orientation_gridEngine(Candidates& candidates,
                                               const Fmi::Box& box,
                                               const State& state,
                                               const Fmi::SpatialReference& crs) const
{
  try
  {
    const auto* gridEngine = state.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    auto dataServer = gridEngine->getDataServer_sptr();

    Fmi::CoordinateTransformation transformation(crs, "WGS84");

    std::vector<T::Coordinate> pointList;

    for (const auto& cand : candidates)
    {
      const int length = 2;  // move in pixel units in the orientation marked for the label

      auto x1 = cand.x;
      auto y1 = cand.y;

      auto x2 = cand.x + length * sin(cand.angle * M_PI / 180);
      auto y2 = cand.y - length * cos(cand.angle * M_PI / 180);

      // printf("%f,%f  %f,%f => ",x1,y1,x2,y2);

      box.itransform(x1, y1);
      box.itransform(x2, y2);

      transformation.transform(x1, y1);
      transformation.transform(x2, y2);

      // printf("%f,%f  %f,%f\n",x1,y1,x2,y2);

      pointList.emplace_back(x1, y1);
      pointList.emplace_back(x2, y2);
    }

    T::GridValueList valueList;
    uint modificationOperation = 0;
    double_vec modificationParameters;
    dataServer->getGridValueListByPointList(0,
                                            fileId,
                                            messageIndex,
                                            T::CoordinateTypeValue::LATLON_COORDINATES,
                                            pointList,
                                            T::AreaInterpolationMethod::Linear,
                                            modificationOperation,
                                            modificationParameters,
                                            valueList);

    if (valueList.getLength() == pointList.size())
    {
      uint i = 0;
      for (auto& cand : candidates)
      {
        T::GridValue* val1 = valueList.getGridValuePtrByIndex(i);
        T::GridValue* val2 = valueList.getGridValuePtrByIndex(i + 1);
        i = i + 2;

        if (val1 != nullptr && val2 != nullptr && val1->mValue > val2->mValue)
        {
          cand.angle = cand.angle + 180;
          if (cand.angle > 360)
            cand.angle -= 360;
        }

        // Force labels upright if so requested
        if (upright && (cand.angle < -90 || cand.angle > 90))
        {
          cand.angle += 180;
          if (cand.angle > 360)
            cand.angle -= 360;
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove bad candidates
 */
// ----------------------------------------------------------------------

Candidates remove_bad_candidates(const Candidates& candidates,
                                 const Fmi::Box& box,
                                 double max_angle,
                                 double min_distance_edge,
                                 double max_distance_edge,
                                 double max_curvature)
{
  Candidates candis;

  // Discard too angled labels and labels too close or far from the edges
  for (const auto& cand : candidates)
  {
    // Angle condition
    bool ok = (cand.angle >= -max_angle and cand.angle <= max_angle);
    // Edge conditions
    if (ok)
    {
      const auto dist = distance(cand, box);
      ok = (dist >= min_distance_edge && dist <= max_distance_edge);
    }
    if (ok)
      ok = (cand.curvature < max_curvature);

    if (ok)
      candis.push_back(cand);
  }
  return candis;
}

// ----------------------------------------------------------------------
/*!
 * \brief Approximate distance between labels
 *
 * We calculate the smallest distance between the estimated four corner
 * locations of the label.
 */
// ----------------------------------------------------------------------

double distance(const Candidate& c1, const Candidate& c2)
{
  // Init with center point distance in case of overlap
  double minimum = std::hypot(c1.x - c2.x, c1.y - c2.y);

  // Then process rotated rectangle corners in case of no overlap
  for (const auto& p1 : c1.corners)
    for (const auto& p2 : c2.corners)
      minimum = std::min(minimum, std::hypot(p1.x - p2.x, p1.y - p2.y));

  return minimum;
}

// ----------------------------------------------------------------------
/*!
 * \bried Create candidate edges which satisfy distance conditions
 *
 * Too short edges will be marked for two separate structures for speed.
 */
// ----------------------------------------------------------------------

Edges create_possible_edges(const Candidates& candidates,
                            BadPairs& bad_pairs,
                            double min_distance_self,
                            double min_distance_same,
                            double min_distance_other)
{
  const auto n = candidates.size();
  const auto max_pairs = n * (n - 1) / 2;

  Edges edges;

  edges.reserve(max_pairs);

  for (std::size_t i = 0; i < n - 1; i++)
  {
    const auto& c1 = candidates[i];
    for (std::size_t j = i + 1; j < n; j++)
    {
      const auto& c2 = candidates[j];

      // auto length = std::hypot(c1.x - c2.x, c1.y - c2.y);
      auto length = distance(c1, c2);

      // We ignore edges do not place labels very close and hence bring nothing to the appearance
      if (length < 250)
      {
        bool allowed = false;
        if (c1.id == c2.id)
          allowed = (length >= min_distance_self);  // same isoline segment
        else if (c1.isovalue == c2.isovalue)
          allowed = (length >= min_distance_same);  // same isoline value, another segment
        else
          allowed = (length >= min_distance_other);  // different isovalue or isoline segment

        if (allowed)
          edges.emplace_back(i, j, length, c1.weight * c2.weight);

        if (!allowed)
        {
          bad_pairs[i].push_back(j);
          bad_pairs[j].push_back(i);
        }
      }
    }
  }
  return edges;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find root node of a subtree
 */
// ----------------------------------------------------------------------

std::size_t find_parent(const Parents& parents, std::size_t i)
{
  while (parents[i] != i)
    i = parents[i];
  return i;
}

// ----------------------------------------------------------------------
/*!
 * \brief Test whether an edge would connect two different subtrees
 */
// ----------------------------------------------------------------------

bool kruskal_test(const Edge& edge, Parents& parents)
{
  // test whether the edge would join two separate trees
  auto parent1 = find_parent(parents, edge.first);
  auto parent2 = find_parent(parents, edge.second);

  if (parent1 == parent2)
    return false;

  parents[parent1] = parent2;  // merge the two trees
  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief A label was chosen, mark all others too close to it as rejected
 */
// ----------------------------------------------------------------------

std::size_t reject_too_close_candidates(Statuses& status, const BadPairList& bad_pairs)
{
  std::size_t count = 0;
  for (auto i : bad_pairs)
    if (status[i] != Status::Rejected)
    {
      ++count;
      status[i] = Status::Rejected;
    }
  return count;
}

// ----------------------------------------------------------------------
/*!
 * \brief Assign weights for candidates
 *
 * Normally we would build a minimum spanning tree using lengths only.
 * However, we wish to prefer multiples of 10 over multiples over 5
 * over multiples of 2 and then the rest.
 *
 * Say for example we have possible edges 10 -- 5 ---- 10.
 * The distances are 2 and for, and 2+4=6 from 10 to 10. We wish to make
 * sure that both 10s are selected first, and hence we scale the distances
 * 2 and 4 by some factor. Using a factor 3 we'd get 6 and 12, which in this
 * case might or might not be a good value. Experiments are needed.
 */
// ----------------------------------------------------------------------

void assign_weights(Candidates& candidates)
{
  for (auto& c : candidates)
  {
    const auto isovalue = c.isovalue;
    if (fmod(isovalue, 10) == 0)
      c.weight = 1;
    else if (fmod(isovalue, 5) == 0)
      c.weight = 4;
    else if (fmod(isovalue, 2) == 0)
      c.weight = 16;
    else
      c.weight = 64;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate estimated label corner coordinates
 *
 * Assumptions:
 *
 *  label size 4 characters
 *  font height 10 pixels
 *  character width 8 pixels
 */
// ......................................................................

void assign_label_coordinates(Candidates& candidates)
{
  const double h = 10 / 2.0;     // half height
  const double w = 4 * 8 / 2.0;  // half width
  const double rads = M_PI / 180;

  for (auto& c : candidates)
  {
    const auto x = c.x;
    const auto y = c.y;
    const auto a = c.angle * rads;
    const auto cosa = cos(a);
    const auto sina = sin(a);
    c.corners[0].x = x - w * cosa - h * sina;
    c.corners[0].y = y - w * sina + h * cosa;
    c.corners[1].x = x + w * cosa - h * sina;
    c.corners[1].y = y + w * sina + h * cosa;
    c.corners[2].x = x - w * cosa + h * sina;
    c.corners[2].y = y - w * sina - h * cosa;
    c.corners[3].x = x + w * cosa + h * sina;
    c.corners[3].y = y + w * sina - h * cosa;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Given candidates for each isoline select the best combination of them
 */
// ----------------------------------------------------------------------

Candidates IsolabelLayer::select_best_candidates(const Candidates& candidates,
                                                 const Fmi::Box& box) const
{
  Candidates cands = remove_bad_candidates(
      candidates, box, max_angle, min_distance_edge, max_distance_edge, max_curvature);

  if (cands.empty())
    return cands;

  assign_weights(cands);
  assign_label_coordinates(cands);

  // We wish to prefer a 10-10 connection. By multiplying the distance between 5-10 edges
  // by 4, we can still get 5's selected between 10's, but if the distances between the 5-10
  // edges are similar, the 10's will be chosen first.
  //
  // 10 -- 5 --- 10   here distance between 10's is 2+3=5, 2/3 multiplied by 4 are 8/12.
  //    2     3                                                multiplied by 3 are 6/9
  //

  const auto n = cands.size();
  Statuses status(n, Status::Undecided);  // nothing decided yet

  BadPairs bad_pairs(n, BadPairList{});  // no edge marked as too short yet

  Parents parents(n, 0UL);
  std::iota(parents.begin(), parents.end(), 0UL);  // no parent for any candidate yet

  // Create edges between all possible candidates, marking too short edges separately
  Edges edges = create_possible_edges(
      cands, bad_pairs, min_distance_self, min_distance_same, min_distance_other);

  // Shortest edges will be processed first.
  // NOTE: It would be possible to round the sorting criteria (weight*length) to integers
  //       and then store the the edges into a vector of lists of edges. The vector could
  //       be fixed to some specific size, and any overflowing edges could be stored into
  //       a separate container which would be processed if and only if there are still
  //       candidates left. Hence we'd mostly get rid of the O(n log n) sorting phase.

  std::sort(edges.begin(),
            edges.end(),
            [](const Edge& a, const Edge& b) { return a.length * a.weight < b.length * b.weight; });

  // Start the minimum spanning tree

  std::size_t undecided = n;

  for (const auto& edge : edges)
  {
    // No need to consider the longest edges if everything has been decided already
    if (undecided == 0)
      break;

    const auto i = edge.first;
    const auto j = edge.second;
    const auto status1 = status[i];
    const auto status2 = status[j];

    // rejected candidates do not count in building the tree
    if (status1 != Status::Rejected && status2 != Status::Rejected)
    {
      // nothing to do if both candidates already accepted
      if (status1 != Status::Accepted && status2 != Status::Accepted)
      {
        // Now we have either undecided-undecided or accpeted-undecided, and we
        // must do the Kruskal test to see if the edge would build up the tree

        if (kruskal_test(edge, parents))
        {
          // Add the new candidate. Each Undecided reduces the remaining number of available
          // candidates by one, plus the number of new candidates it rejects (some may have been
          // rejected already, hence the size of the bad_pairs[x] container is not sufficient here,
          // but must be explicitly counted in the function.

          if (status1 == Status::Undecided)
          {
            status[i] = Status::Accepted;
            --undecided;
            undecided -= reject_too_close_candidates(status, bad_pairs[i]);
          }
          if (status2 == Status::Undecided)
          {
            status[j] = Status::Accepted;
            --undecided;
            undecided -= reject_too_close_candidates(status, bad_pairs[j]);
          }
        }
      }
    }
  }

  // Return accepted candidates only, not Undecided ones too
  Candidates ret;
  for (std::size_t i = 0; i < status.size(); i++)
    if (status[i] == Status::Accepted)
      ret.push_back(cands[i]);

  return ret;
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
    Fmi::hash_combine(hash, Dali::hash_value(label, theState));
    for (const auto& angle : angles)
      Fmi::hash_combine(hash, Fmi::hash_value(angle));
    Fmi::hash_combine(hash, Fmi::hash_value(upright));
    Fmi::hash_combine(hash, Fmi::hash_value(max_angle));
    Fmi::hash_combine(hash, Fmi::hash_value(min_isoline_length));
    Fmi::hash_combine(hash, Fmi::hash_value(min_distance_other));
    Fmi::hash_combine(hash, Fmi::hash_value(min_distance_same));
    Fmi::hash_combine(hash, Fmi::hash_value(min_distance_self));
    Fmi::hash_combine(hash, Fmi::hash_value(min_distance_edge));
    Fmi::hash_combine(hash, Fmi::hash_value(max_distance_edge));
    Fmi::hash_combine(hash, Fmi::hash_value(max_curvature));
    Fmi::hash_combine(hash, Fmi::hash_value(stencil_size));
    Fmi::hash_combine(hash, Fmi::hash_value(isovalues));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
