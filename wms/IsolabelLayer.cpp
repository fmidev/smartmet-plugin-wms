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
#include <boost/optional.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <limits>

// #define MYDEBUG 1
// #define MYDEBUG_DETAILS 1

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// For minimum spanning tree searches
struct Edge
{
  std::size_t first;
  std::size_t second;
  double length;
  bool valid;
};

using Edges = std::vector<Edge>;

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

        boost::optional<double> start;
        boost::optional<double> stop;
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

    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IsolabelLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    auto geoms = IsolineLayer::getIsolines(isovalues, theState);

    // The above call guarantees these have been resolved:
    const auto& crs = projection.getCRS();
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
                     const OGRLineString* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
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

    candidates.emplace_back(Candidate{isovalue, x2, y2, angle, curv, id});
  }
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRLinearRing* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
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

    candidates.emplace_back(Candidate{isovalue, x2, y2, angle, curv, id});
  }
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRPolygon* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  find_candidates(candidates, isovalue, id, sine, cosine, stencil_size, geom->getExteriorRing());
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRMultiLineString* geom)
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
                    dynamic_cast<const OGRLineString*>(geom->getGeometryRef(i)));
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRMultiPolygon* geom)
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
                    dynamic_cast<const OGRPolygon*>(geom->getGeometryRef(i)));
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRGeometry* geom);

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRGeometryCollection* geom)
{
  if (geom == nullptr || geom->IsEmpty() != 0)
    return;
  for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    find_candidates(
        candidates, isovalue, ++id, sine, cosine, stencil_size, geom->getGeometryRef(i));
}

void find_candidates(Candidates& candidates,
                     double isovalue,
                     int& id,
                     double sine,
                     double cosine,
                     int stencil_size,
                     const OGRGeometry* geom)
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
                             dynamic_cast<const OGRLineString*>(geom));
    case wkbLinearRing:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRLinearRing*>(geom));
    case wkbPolygon:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRPolygon*>(geom));
    case wkbMultiLineString:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRMultiLineString*>(geom));
    case wkbMultiPolygon:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRMultiPolygon*>(geom));
    case wkbGeometryCollection:
      return find_candidates(candidates,
                             isovalue,
                             ++id,
                             sine,
                             cosine,
                             stencil_size,
                             dynamic_cast<const OGRGeometryCollection*>(geom));
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

Candidates IsolabelLayer::find_candidates(const std::vector<OGRGeometryPtr>& geoms)
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

        Dali::find_candidates(candidates, isovalues[i], id, sine, cosine, stencil_size, geom.get());
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
 * Find the shortest valid edge
 */
// ----------------------------------------------------------------------

boost::optional<std::size_t> find_tree_start_edge(const Edges& edges)
{
  std::size_t best_edge = 0;
  double best_length = -1;

  for (std::size_t i = 0; i < edges.size(); i++)
  {
    const auto& edge = edges[i];
    if (edge.valid && (best_length < 0 || edge.length < best_length))
    {
      best_edge = i;
      best_length = edge.length;
    }
  }

  if (best_length < 0)
    return {};
  return best_edge;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find next shortest valid edge to be added to MSP
 */
// ----------------------------------------------------------------------

boost::optional<std::size_t> find_next_edge(const Edges& edges,
                                            const Edges& bad_edges,
                                            std::vector<boost::tribool>& status)
{
  std::size_t best_edge = 0;
  double best_length = -1;

  for (std::size_t i = 0; i < edges.size(); i++)
  {
    const auto& edge = edges[i];

    auto v1 = edges[i].first;
    auto v2 = edges[i].second;

    // Note: tribool logic. We require one "true", one "indeterminate"

    if (status[v1])
    {
      if (status[v2] || !status[v2])
        continue;
    }
    else if (status[v2])
    {
      if (status[v1] || !status[v1])
        continue;
    }
    else
      continue;

    // Now we know one must have been selected, one is indeterminate
    auto new_vertex = (status[v1] ? v2 : v1);

    if (best_length < 0 || edge.length < best_length)
    {
      // The new vertex must not be connected to any of the already selected vertices
      // by an invalid edge, or it is too close to them. If so, we disable vertex
      // right away. It is always connected to all the vertices though, since
      // we created all possible edges.

      bool forbidden = false;

      for (const auto& test_edge : bad_edges)
      {
        if (test_edge.first == new_vertex)
        {
          if (status[test_edge.second] && !test_edge.valid)
          {
            forbidden = true;
            break;
          }
        }
        else if (test_edge.second == new_vertex)
        {
          if (status[test_edge.first] && !test_edge.valid)
          {
            forbidden = true;
            break;
          }
        }
      }

      if (forbidden)
      {
        status[new_vertex] = false;
      }
      else
      {
        best_edge = i;
        best_length = edge.length;
      }
    }
  }

  if (best_length < 0)
    return {};

  return best_edge;
}

// ----------------------------------------------------------------------
/*!
 * \brief Given candidates for each isoline select the best combination of them
 */
// ----------------------------------------------------------------------

Candidates IsolabelLayer::select_best_candidates(const Candidates& candidates,
                                                 const Fmi::Box& box) const
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

  if (candis.empty())
    return candis;

  // Find Euclician "minimum" spanning tree using Prim's algorithm with
  // modifications:
  //   1. Do not choose random vertex as starting point, select shortest edge satisfying distance
  //      constraints as the first pair of points.
  //   2. Choose the next shortest edge satisfying distance constraints to the selected points
  //      and add the end vertex to the set of selected points.
  //   3. Repeat until no edge satisfies the constraints
  //
  // We assume this approximates the true solution under the distance constraints without
  // attempting to prove it does.

  Edges edges;
  Edges bad_edges;

  const auto n = candis.size();

  int invalid = 0;

  for (std::size_t i = 0; i < n - 1; i++)
    for (std::size_t j = i + 1; j < n; j++)
    {
      auto length = std::hypot(candis[i].x - candis[j].x, candis[i].y - candis[j].y);

      bool valid = false;
      if (candis[i].id == candis[j].id)
        valid = (length >= min_distance_self);  // same isoline segment
      else if (candis[i].isovalue == candis[j].isovalue)
        valid = (length >= min_distance_same);  // same isoline value, another segment
      else
        valid = (length >= min_distance_other);  // different isovalue or isoline segment

      if (!valid)
        ++invalid;

      if (valid)
        edges.emplace_back(Edge{i, j, length, valid});
      else
        bad_edges.emplace_back(Edge{i, j, length, valid});
    }

  // Start the minimum spanning tree

  auto opt_start = find_tree_start_edge(edges);
  if (!opt_start)
    return {};

  // First selected candidates

  auto cand1 = edges[*opt_start].first;
  auto cand2 = edges[*opt_start].second;

  // Start building the tree
  std::vector<boost::tribool> candidate_status(candis.size(), boost::logic::indeterminate);
  candidate_status[cand1] = true;
  candidate_status[cand2] = true;

  while (true)
  {
    auto opt_next = find_next_edge(edges, bad_edges, candidate_status);
    if (!opt_next)
      break;

    cand1 = edges[*opt_next].first;
    cand2 = edges[*opt_next].second;
    candidate_status[cand1] = true;
    candidate_status[cand2] = true;

    // Remove the edge from further consideration by replacing it with the last element of the array
    edges[*opt_next] = edges.back();
    edges.pop_back();
  }

  Candidates ret;
  for (std::size_t i = 0; i < candidate_status.size(); i++)
  {
    if (candidate_status[i])  // fails for false|indeterminate
      ret.push_back(candis[i]);
  }

  return ret;
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

  boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
  boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
  boost::local_time::local_date_time localdatetime(getValidTime(), utc);
  auto mylocale = std::locale::classic();
  NFmiPoint dummy;
  TS::LocalTimePoolPtr localTimePool = nullptr;

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
                                                  dummy,
                                                  localTimePool);

      auto result = q->value(options, localdatetime);

      double tmp = cand.isovalue;

      if (boost::get<double>(&result) != nullptr)
        tmp = *boost::get<double>(&result);
      else if (boost::get<int>(&result) != nullptr)
        tmp = *boost::get<int>(&result);
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
