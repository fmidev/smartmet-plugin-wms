#include "IsolineFilter.h"
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

  for (int i = 0; i < n; i++)
  {
    double x = geom->getX(i);
    double y = geom->getY(i);
    box.transform(x, y);
    x = std::round(x * rfactor) / rfactor;
    y = std::round(y * rfactor) / rfactor;
    data.points.push_back({x, y});
  }

  // Use vertex counter to identify break points.
  // count=0: isoline vertex (free) → smooth
  // count=1: unshared isoband edge (grid boundary) → break
  // count=2: shared isoband edge → smooth
  // count=4: shared isoband corner (grid vertex) → break
  for (int i = 1; i < n - 1; i++)
  {
    geom->getPoint(i, &pt);
    int count = counter.getCount(pt);
    if (count != 0 && count != 2)
      data.breakIndices.push_back(i);
  }

  return data;
}

}  // namespace

void IsolineFilter::writeBezierLineStringSvg(std::string& out,
                                             const OGRLineString* geom,
                                             const Fmi::Box& box,
                                             double rfactor,
                                             int decimals) const
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

    auto cubics =
        Fmi::BezierFit::fitPolylineWithBreaks(data.points, data.breakIndices, m_bezierAccuracy, m_bezierMaxDepth);

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
                                             int decimals) const
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
    // 3. For shared sub-segments (vertex count=2), normalize direction so that
    //    the same edge traversed in opposite directions by adjacent isobands
    //    produces identical Bezier curves — preventing gaps unconditionally.

    auto data = extractPolylineData(geom, box, rfactor, m_vertexCounter);
    if (data.points.size() < 3)
      return;

    // Build break indices for the ring (excluding closing duplicate)
    int ringSize = n - 1;  // number of unique vertices
    std::vector<int> breaks;
    for (int i = 0; i < ringSize; i++)
    {
      OGRPoint pt;
      geom->getPoint(i, &pt);
      int count = m_vertexCounter.getCount(pt);
      if (count != 0 && count != 2)
        breaks.push_back(i);
    }

    if (breaks.empty())
    {
      // No break points - the entire ring is smooth. Fit as one piece.
      std::vector<Fmi::BezierFit::Point> ringPoints(data.points.begin(), data.points.end() - 1);
      auto cubics = Fmi::BezierFit::fitPolyline(ringPoints, m_bezierAccuracy, m_bezierMaxDepth);
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

      // Check if this segment is shared (all interior vertices have count=2)
      bool isShared = true;
      for (std::size_t i = 1; i + 1 < segPoints.size(); i++)
      {
        int idx = (startIdx + static_cast<int>(i)) % ringSize;
        OGRPoint pt;
        geom->getPoint(idx, &pt);
        int count = m_vertexCounter.getCount(pt);
        if (count != 2)
        {
          isShared = false;
          break;
        }
      }

      // Direction normalization for shared segments
      bool reverseOutput = false;
      std::vector<Fmi::BezierFit::Point>* fitPoints = &segPoints;
      std::vector<Fmi::BezierFit::Point> reversedPoints;

      if (isShared && segPoints.size() >= 2)
      {
        // Canonical direction: first point should be "less than" last point
        auto& first = segPoints.front();
        auto& last = segPoints.back();
        if (last.x < first.x || (last.x == first.x && last.y < first.y))
        {
          // Need to reverse: fit in canonical direction, then reverse output
          reversedPoints.assign(segPoints.rbegin(), segPoints.rend());
          fitPoints = &reversedPoints;
          reverseOutput = true;
        }
      }

      auto cubics = Fmi::BezierFit::fitPolyline(*fitPoints, m_bezierAccuracy, m_bezierMaxDepth);

      if (reverseOutput)
        cubics = Fmi::BezierFit::reverseCubics(cubics);

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
                                   int decimals) const
{
  try
  {
    if (geom == nullptr || geom->IsEmpty())
      return;

    OGRwkbGeometryType id = geom->getGeometryType();
    switch (wkbFlatten(id))
    {
      case wkbLineString:
        writeBezierLineStringSvg(out, dynamic_cast<const OGRLineString*>(geom), box, rfactor, decimals);
        break;
      case wkbLinearRing:
        writeBezierLinearRingSvg(out, dynamic_cast<const OGRLinearRing*>(geom), box, rfactor, decimals);
        break;
      case wkbPolygon:
      {
        const auto* poly = dynamic_cast<const OGRPolygon*>(geom);
        writeBezierLinearRingSvg(out, poly->getExteriorRing(), box, rfactor, decimals);
        for (int i = 0, nHoles = poly->getNumInteriorRings(); i < nHoles; i++)
          writeBezierLinearRingSvg(out, poly->getInteriorRing(i), box, rfactor, decimals);
        break;
      }
      case wkbMultiLineString:
      {
        const auto* multi = dynamic_cast<const OGRMultiLineString*>(geom);
        for (int i = 0, ng = multi->getNumGeometries(); i < ng; i++)
          writeBezierLineStringSvg(out, multi->getGeometryRef(i), box, rfactor, decimals);
        break;
      }
      case wkbMultiPolygon:
      {
        const auto* multi = dynamic_cast<const OGRMultiPolygon*>(geom);
        for (int i = 0, ng = multi->getNumGeometries(); i < ng; i++)
          writeBezierSvg(out, multi->getGeometryRef(i), box, rfactor, decimals);
        break;
      }
      case wkbGeometryCollection:
      {
        const auto* coll = dynamic_cast<const OGRGeometryCollection*>(geom);
        for (int i = 0, ng = coll->getNumGeometries(); i < ng; i++)
          writeBezierSvg(out, coll->getGeometryRef(i), box, rfactor, decimals);
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
                                       double precision) const
{
  try
  {
    const double prec = std::max(0.0, precision);
    const int decimals = std::min(16.0, std::ceil(prec));
    const double rfactor = std::pow(10.0, prec);

    std::string out;
    writeBezierSvg(out, &geom, box, rfactor, decimals);
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
