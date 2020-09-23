//======================================================================

#include "IsolabelLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoband.h"
#include "Isoline.h"
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
#include <gis/OGR.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <limits>

//#define MYDEBUG 1
//#define MYDEBUG_DETAILS 1

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

void IsolabelLayer::init(const Json::Value& theJson,
                         const State& theState,
                         const Config& theConfig,
                         const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isolabel-layer JSON is not a JSON object");

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

    json = theJson.get("min_distance_other", nulljson);
    if (!json.isNull())
      min_distance_other = json.asDouble();

    json = theJson.get("min_distance_same", nulljson);
    if (!json.isNull())
      min_distance_same = json.asDouble();

    json = theJson.get("min_distance_self", nulljson);
    if (!json.isNull())
      min_distance_self = json.asDouble();

    json = theJson.get("max_curvature", nulljson);
    if (!json.isNull())
      max_curvature = json.asDouble();

    json = theJson.get("stencil_size", nulljson);
    if (!json.isNull())
      stencil_size = json.asInt();

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
          throw Fmi::Exception(BCP, "Isolabel-layer isovalues start or stop setting missing");

        double iso1 = start.asDouble();
        double iso2 = stop.asDouble();
        double isostep = (step.isNull() ? 1.0 : step.asDouble());

        if (iso2 < iso1)
          throw Fmi::Exception(
              BCP, "Isolabel-layer isovalues stop value must be greater than the start value");
        if (isostep < 0)
          throw Fmi::Exception(BCP, "Isolabel-layer step value must be greater than zero");
        if ((iso2 - iso1) / isostep > 1000)
          throw Fmi::Exception(BCP, "Isolabel-layer generates to many isovalues (> 1000)");

        // The end condition is used to make sure we do not get both 999.99999 and 1000.000 due to
        // numerical inaccuracies.
        for (double value = iso1; value < iso2 - isostep / 2; value += isostep)
          isovalues.push_back(value);
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
      fix_orientation(candidates, box, theState, *crs);

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
      last_angle = angle;
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
  std::vector<double> ycoords;
  const auto n = geom->getNumPoints();
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
  std::vector<double> ycoords;
  const auto n = geom->getNumPoints();
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
    OGRGeometryPtr geom = geoms[i];
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

#ifdef MYDEBUG
  std::cout << "Start edge: " << best_edge << " from " << edges[best_edge].first << " to "
            << edges[best_edge].second << " length=" << best_length << std::endl;
#endif

  if (best_length < 0)
    return {};
  return best_edge;
}

// ----------------------------------------------------------------------
/*!
 * \brief Find next shortest valid edge to be added to MSP
 */
// ----------------------------------------------------------------------

boost::optional<std::size_t> find_next_edge(const Edges& edges, std::vector<boost::tribool>& status)
{
  std::size_t best_edge = 0;
  double best_length = -1;

  for (std::size_t i = 0; i < edges.size(); i++)
  {
    const auto& edge = edges[i];

    auto v1 = edges[i].first;
    auto v2 = edges[i].second;

#ifdef MYDEBUG_DETAILS
    std::cout << "\t\t\t\t";
    std::cout << v1 << "=";
    if (status[v1] == true)
      std::cout << "T";
    else if (status[v1] == false)
      std::cout << "F";
    else
      std::cout << "?";
    std::cout << "\t" << v2 << "=";
    if (status[v2] == true)
      std::cout << "T";
    else if (status[v2] == false)
      std::cout << "F";
    else
      std::cout << "?";
    std::cout << std::endl;
#endif

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

    if (edge.valid && (best_length < 0 || edge.length < best_length))
    {
      // The new vertex must not be connected to any of the already selected vertices
      // by an invalid edge, or it is too close to them. If so, we disable vertex
      // right away. It is always connected to all the vertices though, since
      // we created all possible edges.

      bool forbidden = false;

      for (const auto& edge : edges)
      {
        if (edge.first == new_vertex)
        {
          if (status[edge.second] && !edge.valid)
          {
            forbidden = true;
            break;
          }
        }
        else if (edge.second == new_vertex)
        {
          if (status[edge.first] && !edge.valid)
          {
            forbidden = true;
            break;
          }
        }
      }

#ifdef MYDEBUG_DETAILS
      std::cout << "\t\t" << v1 << "\t" << v2 << "\tlen=" << edge.length << "\t"
                << (forbidden ? "forbidden" : "not forbidden") << std::endl;
#endif

      if (forbidden)
      {
#ifdef MYDEBUG
        std::cout << "Vertex " << new_vertex << " forbidden" << std::endl;
#endif
        status[new_vertex] = false;
      }
      else
      {
        best_edge = i;
        best_length = edge.length;
      }
    }
  }

#ifdef MYDEBUG
  std::cout << "Next edge: " << best_edge << " from " << edges[best_edge].first << " to "
            << edges[best_edge].second << " length=" << best_length << std::endl;
#endif

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

#ifdef MYDEBUG
  for (std::size_t i = 0; i < candis.size(); i++)
  {
    const auto& c = candis[i];
    std::cout << "C:\t" << c.isovalue << "\tat " << c.x << "," << c.y << "\tid= " << c.id
              << std::endl;
  }
#endif

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

  // Create a vector of all possible undirected edges

  Edges edges;

  const auto n = candis.size();

  for (std::size_t i = 0; i < n - 1; i++)
    for (std::size_t j = i + 1; j < n; j++)
    {
      auto length = std::hypot(candis[i].x - candis[j].x, candis[i].y - candis[j].y);

      bool valid;
      if (candis[i].id == candis[j].id)
        valid = (length >= min_distance_self);  // same isoline segment
      else if (candis[i].isovalue == candis[j].isovalue)
        valid = (length >= min_distance_same);  // same isoline value, another segment
      else
        valid = (length >= min_distance_other);  // different isovalue or isoline segment

      edges.emplace_back(Edge{i, j, length, valid});

#ifdef MYDEBUG
      std::cout << "E:\t" << i << "\t" << j << "\t" << length << "\t" << candis[i].isovalue << " - "
                << candis[j].isovalue << "\t" << candis[i].id << " - " << candis[j].id << "\t"
                << (valid ? "OK" : "BAD") << std::endl;
#endif
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
#ifdef MYDEBUG
    std::cout << "\tCurrent selections: ";
    for (std::size_t i = 0; i < candidate_status.size(); i++)
      if (candidate_status[i] == true)
        std::cout << i << " ";
    std::cout << std::endl;
#endif

    auto opt_next = find_next_edge(edges, candidate_status);
    if (!opt_next)
      break;

    cand1 = edges[*opt_next].first;
    cand2 = edges[*opt_next].second;
    candidate_status[cand1] = true;
    candidate_status[cand2] = true;
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
                                    OGRSpatialReference& crs) const
{
  if (source && *source == "grid")
  {
    fix_orientation_gridEngine(candidates, box, state, crs);
    return;
  }

  // The parameter being used
  auto param = Spine::ParameterFactory::instance().parse(*parameter);

  boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
  boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
  boost::local_time::local_date_time localdatetime(getValidTime(), utc);
  auto mylocale = std::locale::classic();
  NFmiPoint dummy;

  // Querydata spatial reference
  auto qcrs = state.getGisEngine().getSpatialReference(q->area().WKT().c_str());

  // From image world coordinates to querydata world coordinates

  std::unique_ptr<OGRCoordinateTransformation> transformation(
      OGRCreateCoordinateTransformation(&crs, qcrs.get()));

  if (transformation == nullptr)
    throw Fmi::Exception(BCP,
                           "Failed to create coordinate transformation for orienting isolabels");

  // Check and fix orientations for each isovalue

  for (auto& cand : candidates)
  {
    const int length = 2;  // move in pixel units in the orientation marked for the label

    auto x = cand.x + length * sin(cand.angle * M_PI / 180);
    auto y = cand.y - length * cos(cand.angle * M_PI / 180);

    box.itransform(x, y);  // world xy coordinate in image crs

    if (transformation->Transform(1, &x, &y) == 0)
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
      Engine::Querydata::ParameterOptions options(
          param, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);

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
                                               OGRSpatialReference& sr_image) const
{
  try
  {
    auto gridEngine = state.getGridEngine();
    auto dataServer = gridEngine->getDataServer_sptr();

    OGRSpatialReference sr_latlon;
    sr_latlon.importFromEPSG(4326);

    OGRCoordinateTransformation* transformation =
        OGRCreateCoordinateTransformation(&sr_image, &sr_latlon);
    std::vector<T::Coordinate> pointList;

    for (auto& cand : candidates)
    {
      const int length = 2;  // move in pixel units in the orientation marked for the label

      auto x1 = cand.x;
      auto y1 = cand.y;

      auto x2 = cand.x + length * sin(cand.angle * M_PI / 180);
      auto y2 = cand.y - length * cos(cand.angle * M_PI / 180);

      // printf("%f,%f  %f,%f => ",x1,y1,x2,y2);

      box.itransform(x1, y1);
      box.itransform(x2, y2);

      transformation->Transform(1, &x1, &y1);
      transformation->Transform(1, &x2, &y2);

      // printf("%f,%f  %f,%f\n",x1,y1,x2,y2);

      pointList.push_back(T::Coordinate(x1, y1));
      pointList.push_back(T::Coordinate(x2, y2));
    }

    T::GridValueList valueList;
    dataServer->getGridValueListByPointList(0,
                                            fileId,
                                            messageIndex,
                                            T::CoordinateTypeValue::LATLON_COORDINATES,
                                            pointList,
                                            T::AreaInterpolationMethod::Linear,
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

    if (transformation != nullptr)
      OCTDestroyCoordinateTransformation(transformation);
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
    Dali::hash_combine(hash, Dali::hash_value(label, theState));
    for (auto& angle : angles)
      Dali::hash_combine(hash, Dali::hash_value(angle));
    Dali::hash_combine(hash, upright);
    Dali::hash_combine(hash, max_angle);
    Dali::hash_combine(hash, min_distance_other);
    Dali::hash_combine(hash, min_distance_same);
    Dali::hash_combine(hash, min_distance_self);
    Dali::hash_combine(hash, min_distance_edge);
    Dali::hash_combine(hash, max_distance_edge);
    Dali::hash_combine(hash, max_curvature);
    Dali::hash_combine(hash, stencil_size);
    Dali::hash_combine(hash, Dali::hash_value(isovalues));
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
