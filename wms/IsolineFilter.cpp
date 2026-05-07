#include "IsolineFilter.h"
#include "BezierCache.h"
#include "BezierFit.h"
#include "JsonTools.h"
#include <fmt/core.h>
#include <gis/Box.h>
#include <gis/GeometrySmoother.h>
#include <gis/VertexCounter.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <ogr_geometry.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{

// Parse name given in config file into an enum type for the filter

Fmi::GeometrySmoother::Type parse_filter_name(const std::string& name)
{
  try
  {
    if (name == "none")
      return Fmi::GeometrySmoother::Type::None;
    if (name == "average")
      return Fmi::GeometrySmoother::Type::Average;
    if (name == "linear")
      return Fmi::GeometrySmoother::Type::Linear;
    if (name == "gaussian")
      return Fmi::GeometrySmoother::Type::Gaussian;
    if (name == "tukey")
      return Fmi::GeometrySmoother::Type::Tukey;
    throw Fmi::Exception(BCP, "Unknown isoline filter type '" + name + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// Initialize from JSON config
void IsolineFilter::init(Json::Value& theJson)
{
  try
  {
    if (theJson.isNull())
      return;
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isoline filter settings must be in a JSON object");

    Fmi::GeometrySmoother::Type type = Fmi::GeometrySmoother::Type::None;
    double radius = 0.0;
    uint iterations = 1;

    JsonTools::remove_double(radius, theJson, "radius");
    JsonTools::remove_uint(iterations, theJson, "iterations");

    std::string stype;
    JsonTools::remove_string(stype, theJson, "type");
    if (!stype.empty())
      type = parse_filter_name(stype);

    // Sanity checks
    if (radius < 0)
      throw Fmi::Exception(BCP, "Isoline filter radius must be nonnegative");
    if (radius > 100)
      throw Fmi::Exception(BCP, "Isoline filter radius must be less than 100 pixels");
    if (iterations > 100)
      throw Fmi::Exception(BCP, "Isoline filter number of iterations must be less than 100");

    smoother.type(type);
    smoother.radius(radius);
    smoother.iterations(iterations);

    // Bezier fitting settings (optional)
    // Can be specified as a number (accuracy in pixels) or as an object with settings
    if (theJson.isMember("bezier"))
    {
      auto& bezierJson = theJson["bezier"];
      if (bezierJson.isDouble() || bezierJson.isInt())
      {
        m_bezierAccuracy = bezierJson.asDouble();
      }
      else if (bezierJson.isObject())
      {
        JsonTools::remove_double(m_bezierAccuracy, bezierJson, "accuracy");
        int maxDepth = m_bezierMaxDepth;
        JsonTools::remove_int(maxDepth, bezierJson, "maxdepth");
        m_bezierMaxDepth = maxDepth;
      }
      theJson.removeMember("bezier");

      if (m_bezierAccuracy < 0)
        throw Fmi::Exception(BCP, "Bezier fitting accuracy must be nonnegative");
      // 0 disables fitting; below ~2 px the moment-matching fitter starts
      // returning extreme control points that produce visible artifacts
      // (very large d0/d1 in unit-chord coords project far outside the view).
      if (m_bezierAccuracy > 0 && m_bezierAccuracy < 2)
        throw Fmi::Exception(BCP, "Bezier fitting accuracy must be at least 2 pixels");
      if (m_bezierAccuracy > 100)
        throw Fmi::Exception(BCP, "Bezier fitting accuracy must be less than 100 pixels");
      if (m_bezierMaxDepth < 1 || m_bezierMaxDepth > 20)
        throw Fmi::Exception(BCP, "Bezier fitting maxdepth must be between 1 and 20");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Apply smoother and prepare vertex counter for Bezier fitting
void IsolineFilter::apply(std::vector<OGRGeometryPtr>& geoms, bool preserve_topology)
{
  // Apply the weighted average smoother as before
  smoother.apply(geoms, preserve_topology);

  // When Bezier fitting is enabled, always count vertices across all geometries.
  // The VertexCounter identifies shared edges (count=2) and grid corners (count=4)
  // so that Bezier fitting places break points correctly and fits shared edges in
  // a canonical direction. This prevents gaps unconditionally — for isobands the
  // shared edges get identical curves, and for isolines the counts are simply zero
  // (unshared) so every vertex is freely fittable.
  if (m_bezierAccuracy > 0)
  {
    m_vertexCounter = Fmi::VertexCounter();  // reset
    for (const auto& geom_ptr : geoms)
      if (geom_ptr && !geom_ptr->IsEmpty())
        m_vertexCounter.add(geom_ptr.get());
  }
}

// Alter the pixel radius to a metric one based on a BBOX
void IsolineFilter::bbox(const Fmi::Box& box)
{
  smoother.bbox(box);
}

// ======================================================================
// Bezier SVG export
// ======================================================================

// Helper: extract points from a linestring, transform to pixel coords,
// and identify break indices using the vertex counter.
// Break points are vertices where the Bezier curve must have a corner:
// - Grid corners (vertex count = 4 or >= 3)
// - Grid boundary edges (vertex count = 1)
// For isolines all counts are 0, so no break points are generated and
// the fitter treats the entire polyline as one smooth curve.

namespace
{

struct PolylineData
{
  std::vector<Fmi::BezierFit::Point> points;
  std::vector<int> breakIndices;
};

PolylineData extractPolylineData(const OGRLineString* geom,
                                 const Fmi::Box& box,
                                 double rfactor,
                                 const Fmi::VertexCounter& counter)
{
  PolylineData data;
  const int n = geom->getNumPoints();
  if (n < 2)
    return data;

  data.points.reserve(n);
  OGRPoint pt;

  // Track which vertices land exactly on the view boundary in pixel
  // coordinates (before rfactor rounding). These are the "fixed points"
  // where the contour intersects the grid edge — they must be break
  // points so neighbouring isobands' shared sub-segment endpoints line
  // up exactly. Without this, two adjacent isobands' ghostline tails
  // (which differ between the two) get merged with the contour
  // sub-segment and the cache misses despite the contour itself being
  // bit-identical.
  std::vector<bool> onBoundary(n, false);
  const double bx_max = static_cast<double>(box.width());
  const double by_max = static_cast<double>(box.height());

  for (int i = 0; i < n; i++)
  {
    double x = geom->getX(i);
    double y = geom->getY(i);
    box.transform(x, y);
    onBoundary[i] = (x <= 0.0 || x >= bx_max || y <= 0.0 || y >= by_max);
    x = std::round(x * rfactor) / rfactor;
    y = std::round(y * rfactor) / rfactor;
    data.points.push_back({x, y});
  }

  // Break points fall into two categories:
  // 1. Vertex counter says "corner" (count != 0 && count != 2):
  //    - count=1: unshared isoband edge (only one ring touches)
  //    - count=4: grid corner shared by 4 isobands
  // 2. Vertex sits exactly on the view boundary, regardless of count.
  //    These are the contour-meets-grid-edge points the user described.
  //    Without breaking here a count=2 boundary vertex (where two
  //    adjacent isobands' contours converge before each follows its
  //    own ghostline tail along the boundary) would be smoothed
  //    through, and the resulting sub-segments wouldn't share a cache
  //    key with their neighbour.
  for (int i = 1; i < n - 1; i++)
  {
    geom->getPoint(i, &pt);
    int count = counter.getCount(pt);
    if ((count != 0 && count != 2) || onBoundary[i])
      data.breakIndices.push_back(i);
  }

  return data;
}

}  // namespace

std::vector<Fmi::BezierFit::CubicBez> IsolineFilter::fitWithCache(
    const std::vector<Fmi::BezierFit::Point>& points, BezierCache* cache, bool closed) const
{
  if (points.size() < 2)
    return {};

  if (cache == nullptr)
    return Fmi::BezierFit::fitPolyline(points, m_bezierAccuracy, m_bezierMaxDepth, closed);

  // Closed rings: canonicalize so the same loop, regardless of its
  // OGR ring's start vertex or traversal direction, produces the same
  // cache key. Cubics are stored in canonical direction. On hit we
  // reverse them back to the caller's traversal direction so the
  // isoband's winding (CCW exterior, CW hole) is preserved.
  if (closed)
  {
    const auto cr = BezierCache::canonicalizeClosedRing(points);
    const std::size_t key = BezierCache::hashCanonical(cr.canonical);

    if (const auto* cached = cache->find(key))
    {
      return cr.callerForward ? *cached : Fmi::BezierFit::reverseCubics(*cached);
    }

    auto cubics = Fmi::BezierFit::fitPolyline(
        cr.canonical, m_bezierAccuracy, m_bezierMaxDepth, /*closed=*/true);
    cache->insert(key, cubics);
    return cr.callerForward ? std::move(cubics) : Fmi::BezierFit::reverseCubics(cubics);
  }

  const bool canonical = BezierCache::isCanonical(points);

  std::vector<Fmi::BezierFit::Point> reversed;
  const std::vector<Fmi::BezierFit::Point>* canonicalPts = &points;
  if (!canonical)
  {
    reversed.assign(points.rbegin(), points.rend());
    canonicalPts = &reversed;
  }

  const std::size_t key = BezierCache::hashCanonical(*canonicalPts);

  if (const auto* cached = cache->find(key))
  {
    return canonical ? *cached : Fmi::BezierFit::reverseCubics(*cached);
  }

  auto cubics = Fmi::BezierFit::fitPolyline(*canonicalPts, m_bezierAccuracy, m_bezierMaxDepth);
  cache->insert(key, cubics);
  return canonical ? std::move(cubics) : Fmi::BezierFit::reverseCubics(cubics);
}

void IsolineFilter::writeBezierLineStringSvg(std::string& out,
                                             const OGRLineString* geom,
                                             const Fmi::Box& box,
                                             double rfactor,
                                             int decimals,
                                             BezierCache* cache) const
{
  try
  {
    if (geom == nullptr || geom->IsEmpty() != 0)
      return;
    if (geom->getNumPoints() < 2)
      return;

    auto data = extractPolylineData(geom, box, rfactor, m_vertexCounter);
    if (data.points.size() < 2)
      return;

    // Isolines are independent polylines that don't share edges with other
    // geometries within the isoline filter's own counter — fit the whole
    // component as one smooth curve. The vertex counter built by
    // IsolineFilter::apply gives count==1 for every vertex along a
    // non-self-intersecting isoline, so applying break logic here would
    // force a corner at every interior vertex and reduce the bezier fitter
    // to one cubic per source segment.
    //
    // Closed isolines (first vertex == last vertex, common for low-pressure
    // contour loops) must be fit as rings; without the closed flag the fit
    // has a visible kink at the start/end junction.
    const auto& first = data.points.front();
    const auto& last = data.points.back();
    const bool closed = (first.x == last.x && first.y == last.y);

    auto cubics = fitWithCache(data.points, cache, closed);

    if (cubics.empty())
      return;

    Fmi::BezierFit::appendBezierSvg(out, cubics, true, false, decimals);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Bezier SVG export of linestring failed");
  }
}

void IsolineFilter::writeBezierLinearRingSvg(std::string& out,
                                             const OGRLinearRing* geom,
                                             const Fmi::Box& box,
                                             double rfactor,
                                             int decimals,
                                             BezierCache* cache) const
{
  try
  {
    if (geom == nullptr || geom->IsEmpty() != 0)
      return;

    const int n = geom->getNumPoints();
    if (n < 4)  // need at least 3 unique points + closing duplicate
      return;

    // For closed rings:
    // 1. Identify break points using vertex counter (grid corners, boundary edges)
    // 2. Split the ring into sub-segments at break points
    // 3. Each sub-segment goes through fitWithCache, which normalizes the
    //    direction and shares the fit with the neighbouring isoband (whose
    //    ring traverses the same sub-segment in the opposite direction).
    //    The shared fit guarantees bit-identical SVG on both sides of the
    //    edge — no gaps from floating-point drift.

    auto data = extractPolylineData(geom, box, rfactor, m_vertexCounter);
    if (data.points.size() < 3)
      return;

    // Build break indices for the ring (excluding closing duplicate).
    // Two reasons to break at a vertex:
    // 1. Counter says corner (count != 0 && count != 2)
    // 2. Vertex lies on the view boundary (in pixel coordinates) — these
    //    are the contour-meets-grid-edge points where the ring's
    //    ghostline tail diverges from the shared contour.
    int ringSize = n - 1;  // number of unique vertices
    std::vector<int> breaks;
    const double bx_max = static_cast<double>(box.width());
    const double by_max = static_cast<double>(box.height());
    for (int i = 0; i < ringSize; i++)
    {
      OGRPoint pt;
      geom->getPoint(i, &pt);
      int count = m_vertexCounter.getCount(pt);
      double px = pt.getX();
      double py = pt.getY();
      box.transform(px, py);
      const bool boundary = (px <= 0.0 || px >= bx_max || py <= 0.0 || py >= by_max);
      if ((count != 0 && count != 2) || boundary)
        breaks.push_back(i);
    }

    if (breaks.empty())
    {
      // No break points - the entire ring is smooth. Fit as a closed ring
      // so the curve is C1-continuous across the closure (otherwise a
      // straight-line 'Z' close leaves a visible corner).
      auto cubics = fitWithCache(data.points, cache, /*closed=*/true);
      Fmi::BezierFit::appendBezierSvg(out, cubics, true, false, decimals);
      out += 'Z';
      return;
    }

    // We have break points. Split the ring into segments and fit each one.
    // For direction normalization: compare first and last point of each segment.
    // Always fit in canonical direction, reverse output if actual direction differs.

    bool firstSegment = true;
    int numBreaks = static_cast<int>(breaks.size());

    for (int b = 0; b < numBreaks; b++)
    {
      int startIdx = breaks[b];
      int endIdx = breaks[(b + 1) % numBreaks];

      // Build sub-segment points (may wrap around ring)
      std::vector<Fmi::BezierFit::Point> segPoints;
      if (endIdx > startIdx)
      {
        segPoints.assign(data.points.begin() + startIdx, data.points.begin() + endIdx + 1);
      }
      else
      {
        // Wraps around: take from startIdx to end, then from beginning to endIdx
        segPoints.assign(data.points.begin() + startIdx, data.points.begin() + ringSize);
        segPoints.insert(segPoints.end(), data.points.begin(), data.points.begin() + endIdx + 1);
      }

      if (segPoints.size() < 2)
        continue;

      // fitWithCache handles direction normalization and shared-edge reuse:
      // each sub-segment's canonical-direction fit is cached and replayed
      // (reversed if needed) for the neighbouring isoband's matching edge.
      auto cubics = fitWithCache(segPoints, cache);

      Fmi::BezierFit::appendBezierSvg(out, cubics, firstSegment, false, decimals);
      firstSegment = false;
    }

    out += 'Z';
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Bezier SVG export of linear ring failed");
  }
}

void IsolineFilter::writeBezierSvg(std::string& out,
                                   const OGRGeometry* geom,
                                   const Fmi::Box& box,
                                   double rfactor,
                                   int decimals,
                                   BezierCache* cache) const
{
  try
  {
    if (geom == nullptr || geom->IsEmpty())
      return;

    OGRwkbGeometryType id = geom->getGeometryType();
    switch (wkbFlatten(id))
    {
      case wkbLineString:
        writeBezierLineStringSvg(out, dynamic_cast<const OGRLineString*>(geom), box, rfactor, decimals, cache);
        break;
      case wkbLinearRing:
        writeBezierLinearRingSvg(out, dynamic_cast<const OGRLinearRing*>(geom), box, rfactor, decimals, cache);
        break;
      case wkbPolygon:
      {
        const auto* poly = dynamic_cast<const OGRPolygon*>(geom);
        writeBezierLinearRingSvg(out, poly->getExteriorRing(), box, rfactor, decimals, cache);
        for (int i = 0, nHoles = poly->getNumInteriorRings(); i < nHoles; i++)
          writeBezierLinearRingSvg(out, poly->getInteriorRing(i), box, rfactor, decimals, cache);
        break;
      }
      case wkbMultiLineString:
      {
        const auto* multi = dynamic_cast<const OGRMultiLineString*>(geom);
        for (int i = 0, ng = multi->getNumGeometries(); i < ng; i++)
          writeBezierLineStringSvg(out, multi->getGeometryRef(i), box, rfactor, decimals, cache);
        break;
      }
      case wkbMultiPolygon:
      {
        const auto* multi = dynamic_cast<const OGRMultiPolygon*>(geom);
        for (int i = 0, ng = multi->getNumGeometries(); i < ng; i++)
          writeBezierSvg(out, multi->getGeometryRef(i), box, rfactor, decimals, cache);
        break;
      }
      case wkbGeometryCollection:
      {
        const auto* coll = dynamic_cast<const OGRGeometryCollection*>(geom);
        for (int i = 0, ng = coll->getNumGeometries(); i < ng; i++)
          writeBezierSvg(out, coll->getGeometryRef(i), box, rfactor, decimals, cache);
        break;
      }
      default:
        // Fall through silently for unsupported types
        break;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Bezier SVG export failed");
  }
}

std::string IsolineFilter::toBezierSvg(const OGRGeometry& geom,
                                       const Fmi::Box& box,
                                       double precision,
                                       BezierCache* cache) const
{
  try
  {
    const double prec = std::max(0.0, precision);
    const int decimals = std::min(16.0, std::ceil(prec));
    const double rfactor = std::pow(10.0, prec);

    std::string out;
    writeBezierSvg(out, &geom, box, rfactor, decimals, cache);
    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Hash value for caching purposes
std::size_t IsolineFilter::hash_value() const
{
  try
  {
    auto hash = smoother.hash_value();
    Fmi::hash_combine(hash, Fmi::hash_value(m_bezierAccuracy));
    Fmi::hash_combine(hash, Fmi::hash_value(m_bezierMaxDepth));
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
