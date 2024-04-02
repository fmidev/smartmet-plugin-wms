#include "IsolineFilter.h"
#include "Hash.h"
#include "JsonTools.h"
#include "VertexCounter.h"
#include <gis/Box.h>
#include <macgyver/Exception.h>
#include <ogr_geometry.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{

// Parse name given in config file into an enum type for the filter

IsolineFilter::Type parse_filter_name(const std::string& name)
{
  try
  {
    if (name == "none")
      return IsolineFilter::Type::None;
    if (name == "average")
      return IsolineFilter::Type::Average;
    if (name == "linear")
      return IsolineFilter::Type::Linear;
    if (name == "gaussian")
      return IsolineFilter::Type::Gaussian;
    if (name == "tukey")
      return IsolineFilter::Type::Tukey;
    throw Fmi::Exception(BCP, "Unknown isoline filter type '" + name + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

using Weight = std::function<double(double dist)>;

// Factory method to generate a lambda function for the chosen weighting function
Weight create_weight_lambda(IsolineFilter::Type type, double radius)
{
  switch (type)
  {
    case IsolineFilter::Type::Tukey:
      return [radius](double dist)
      {
        if (dist >= radius)
          return 0.0;

        double normalizedDistance = dist / radius;
        double t = (1.0 - (normalizedDistance * normalizedDistance));
        return t * t;
      };

    case IsolineFilter::Type::Linear:
      return [radius](double dist)
      {
        if (dist >= radius)
          return 0.0;
        return (radius - dist) / radius;
      };

    case IsolineFilter::Type::Average:
      return [radius](double dist)
      {
        if (dist >= radius)
          return 0.0;
        return 1.0;
      };

    case IsolineFilter::Type::Gaussian:
    default:
      return [radius](double dist)
      {
        if (dist >= radius)
          return 0.0;

        double sigma = 1.5 * radius;
        return std::exp(-(dist * dist) / (2 * sigma * sigma));
      };
  }
}

// Utility class to handle weighting by a distance based filter
class LineFilter
{
 public:
  LineFilter(IsolineFilter::Type t, double r) : filter(create_weight_lambda(t, r)) {}

  void init(const OGRLineString* geom)
  {
    init_counts(geom);
    init_distances(geom);
  }

  // Start filtering from a point. Returns false if smoothing at the vertex is disabled
  bool reset(const OGRPoint* point, int i)
  {
    debug = false;
    first_pos = i;
    first_point = *point;
    total_weight = 0;

    // Caching guarantees there are no gaps between isobands simply due to the different winding
    // order of the polygons
    auto pos = cache.find(*point);
    if (pos != cache.end())
    {
      sumX = pos->second.getX();  // Set result to be the cached value
      sumY = pos->second.getY();
      total_weight = 1;
      return false;
    }

    prev_point = *point;
    path_length = 0;
    sumX = 0;
    sumY = 0;

    return allowed(i);
  }

  void append(OGRLineString* geom)
  {
    if (total_weight == 0)
      geom->addPoint(first_point.getX(), first_point.getY());  // filtering was not allowed
    else
    {
      auto x = sumX / total_weight;
      auto y = sumY / total_weight;
      geom->addPoint(x, y);

      cache.insert(std::make_pair(first_point, OGRPoint(x, y)));
    }
  }

  // Reset accumulated path length
  void resetPathLength()
  {
    path_length = 0;
    prev_point = first_point;
  }

  // Add a point
  bool add(const OGRLineString* geom, int j, int dist_pos)
  {
    const auto n = geom->getNumPoints();
    if (j < 0)
      j = n + j - 1;  // j=-1 becomes n-2, the last point is skipped as a duplicate of the 1st one
    else if (j >= n)
      j = j - n + 1;  // j=n becomes 1, 1st point is skipped as a duplicate of the last one

    if (dist_pos < 0)
      dist_pos = n + dist_pos - 1;
    else if (dist_pos >= n)
      dist_pos = dist_pos - n + 1;

    OGRPoint pt;
    geom->getPoint(j, &pt);
    if (!allowed(j))
      return false;

    double dist = 0.0;
    if (j != first_pos)
      dist = distance(dist_pos);

    double weight = filter(dist);
    if (weight == 0)
      return false;

    sumX += weight * pt.getX();
    sumY += weight * pt.getY();
    total_weight += weight;
    path_length = dist;
    prev_point = pt;

    return true;
  }

  // Test whether the vertex is allowed in smoothing
  bool allowed(int j)
  {
    // n=0: isoline, since counting is then disabled
    // n=1: unshared isoband edge, in practise grid edges or there  etc
    // n=2: shared isoband edge
    // n=4: shared isoband corner at a grid cell vertex
    auto n = counts[j];
    return (n == 0 || n == 2);
  }

  // Distance metric
  double distance(int dist_pos) const { return path_length + distances[dist_pos]; }

  // Function to calculate the Euclidean distance between two 2D vertices
  static double distance(const OGRPoint& v1, const OGRPoint& v2)
  {
    double x = v1.getX() - v2.getX();
    double y = v1.getY() - v2.getY();
    return std::hypot(x, y);
  }

  void count(const OGRGeometry* geom) { counter.add(geom); }

 private:
  std::unordered_map<OGRPoint, OGRPoint, OGRPointHash, OGRPointEqual> cache;  // cached results
  VertexCounter counter;          // how many times a vertex appears in the full geometry
  Weight filter;                  // filterer
  std::vector<double> distances;  // distances between adjacent vertices in current polyline
  std::vector<int> counts;        // occurrance counts for current polyline
  OGRPoint first_point{0, 0};
  OGRPoint prev_point{0, 0};
  int first_pos = 0;
  double path_length = 0;
  double sumX = 0;
  double sumY = 0;
  double total_weight = 0;
  bool debug = false;

  // Initialize occurrance counts
  void init_counts(const OGRLineString* geom)
  {
    const auto n = geom->getNumPoints();
    counts.clear();
    counts.reserve(n);

    OGRPoint pt;
    for (int i = 0; i < n; i++)
    {
      geom->getPoint(i, &pt);
      counts.push_back(counter.getCount(pt));
    }
  }

  // Calculate distances between vertices in OGRLineString/OGRLinearRing
  void init_distances(const OGRLineString* geom)
  {
    const auto n = geom->getNumPoints();

    distances.clear();
    distances.reserve(n);

    for (int i = 0; i < n - 1; i++)
    {
      OGRPoint v1;
      OGRPoint v2;
      geom->getPoint(i, &v1);
      geom->getPoint(i + 1, &v2);
      distances.push_back(LineFilter::distance(v1, v2));
    }

    // To simplify subsequent processing
    if (geom->get_IsClosed())
      distances.push_back(distances.front());
  }
};

// Forward declaration needed since two functions call each other
OGRGeometry* apply_filter(const OGRGeometry* theGeom, LineFilter& filter, uint iterations);

// Linestrings may be closed, in which case we handle them like linearrings
OGRLineString* apply_filter(const OGRLineString* geom, LineFilter& filter, uint iterations)
{
  try
  {
    if (geom == nullptr || geom->IsEmpty() != 0)
      return nullptr;

    const bool closed = (geom->get_IsClosed() == 1);

    // To avoid repeated recalculation and unordered_map fetches
    filter.init(geom);

    const OGRLineString* g = geom;
    OGRLineString* out = nullptr;

    OGRPoint v1;

    for (uint iter = 0; iter < iterations; iter++)
    {
      if (iter > 0)
        g = out;                // replace geom with filtered linestring
      out = new OGRLineString;  // new filtered result

      const int n = g->getNumPoints();
      const int max_closed_length = n / 4;  // do not process closed rings too much
      const int imax = (closed ? n - 2 : n - 1);
      for (int i = 0; i <= imax; i++)
      {
        g->getPoint(i, &v1);

        if (filter.reset(&v1, i))
        {
          // Make sure the filter has a symmetric number of points to prevent excessive isoline
          // shrinkage at the ends if the number of iterations is > 1. The effective smoothing
          // radius thus shrinks when we get close to the isoline end points.

          // Closed isoline stencil is always symmetric
          int jmin = i - max_closed_length;
          int jmax = i + max_closed_length;

          if (!closed)
          {
            // Shorter of lengths to start (0) and end vertices (n-1)
            auto max_offset = std::min(i - 0, n - 1 - i);

            jmin = i - max_offset;  // symmetric stencil
            jmax = i + max_offset;
          }

          // Iterate backwards until radius is reached
          for (int j = i; j >= jmin; --j)
            if (!filter.add(g, j, j))
              break;

          // Iterate forwards until maxdistance is reached
          filter.resetPathLength();
          for (int j = i + 1; j <= jmax; ++j)
            if (!filter.add(g, j, j - 1))
              break;
        }
        filter.append(out);
      }
      if (closed)
        out->closeRings();  // Make sure the ring is exactly closed, rounding errors may be
                            // present

      if (iter > 1)
        delete g;
    }

    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Linearrings must be filtered with wraparound. There is a risk that if the specified radius
// is larger than the size of the ring, an averaging filter would transform all coordinates to
// the same value.
OGRLinearRing* apply_filter(const OGRLinearRing* geom, LineFilter& filter, uint iterations)
{
  try
  {
    if (geom == nullptr || geom->IsEmpty() != 0)
      return nullptr;

    // To avoid repeated recalculation and unordered_map fetches
    filter.init(geom);

    const OGRLinearRing* g = geom;
    OGRLinearRing* out = nullptr;

    OGRPoint v1;

    for (uint iter = 0; iter < iterations; iter++)
    {
      if (iter > 0)
        g = out;                // replace geom with filtered linestring
      out = new OGRLinearRing;  // new filtered result

      const int n = g->getNumPoints();
      const int max_closed_length = n / 4;  // do not process closed rings too much
      for (int i = 0; i < n - 1; i++)       // do not process the last point, duplicate it later
      {
        g->getPoint(i, &v1);

        if (filter.reset(&v1, i))
        {
          // Iterate backwards until max radius is reached
          const int jmin = i - max_closed_length;
          for (int j = i; j >= jmin; --j)
            if (!filter.add(g, j, j))
              break;

          // Iterate forwards until max radius is reached

          filter.resetPathLength();
          const int jmax = i + max_closed_length;
          for (int j = i + 1; j <= jmax; ++j)
            if (!filter.add(g, j, j - 1))
              break;
        }
        filter.append(out);
      }

      out->closeRings();  // Make sure the ring is exactly closed, rounding errors may be present

      if (iter > 1)
        delete g;
    }

    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

OGRMultiPolygon* apply_filter(const OGRMultiPolygon* geom, LineFilter& filter, uint iterations)
{
  try
  {
    if (geom == nullptr || geom->IsEmpty() != 0)
      return nullptr;

    auto* out = new OGRMultiPolygon();

    for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    {
      const auto* g = geom->getGeometryRef(i);
      auto* new_g = apply_filter(g, filter, iterations);
      if (new_g != nullptr)
        out->addGeometryDirectly(new_g);
    }

    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

OGRGeometryCollection* apply_filter(const OGRGeometryCollection* geom,
                                    LineFilter& filter,
                                    uint iterations)
{
  try
  {
    if (geom == nullptr || geom->IsEmpty() != 0)
      return nullptr;

    auto* out = new OGRGeometryCollection();

    for (int i = 0, n = geom->getNumGeometries(); i < n; ++i)
    {
      const auto* g = geom->getGeometryRef(i);
      auto* new_g = apply_filter(g, filter, iterations);
      if (new_g != nullptr)
        out->addGeometryDirectly(new_g);
    }

    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

OGRPolygon* apply_filter(const OGRPolygon* geom, LineFilter& filter, uint iterations)
{
  try
  {
    if (geom == nullptr || geom->IsEmpty() != 0)
      return nullptr;

    auto* out = new OGRPolygon();
    const auto* exterior = dynamic_cast<const OGRLinearRing*>(geom->getExteriorRing());
    auto* new_exterior = apply_filter(exterior, filter, iterations);
    out->addRingDirectly(new_exterior);

    for (int i = 0, n = geom->getNumInteriorRings(); i < n; ++i)
    {
      const auto* hole = dynamic_cast<const OGRLinearRing*>(geom->getInteriorRing(i));
      auto* new_hole = apply_filter(hole, filter, iterations);
      out->addRingDirectly(new_hole);
    }

    return out;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

OGRGeometry* apply_filter(const OGRGeometry* geom, LineFilter& filter, uint iterations)
{
  try
  {
    OGRwkbGeometryType id = geom->getGeometryType();

    switch (id)
    {
      case wkbMultiLineString:
        return apply_filter(dynamic_cast<const OGRMultiLineString*>(geom), filter, iterations);
      case wkbLineString:
        return apply_filter(dynamic_cast<const OGRLineString*>(geom), filter, iterations);
      case wkbLinearRing:
        return apply_filter(dynamic_cast<const OGRLinearRing*>(geom), filter, iterations);
      case wkbPolygon:
        return apply_filter(dynamic_cast<const OGRPolygon*>(geom), filter, iterations);
      case wkbMultiPolygon:
        return apply_filter(dynamic_cast<const OGRMultiPolygon*>(geom), filter, iterations);
      case wkbGeometryCollection:
        return apply_filter(dynamic_cast<const OGRGeometryCollection*>(geom), filter, iterations);
      default:
        throw Fmi::Exception::Trace(
            BCP, "Encountered an unknown geometry component while filtering an isoline/isoband");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Apply the filter to a single geometry
void apply_filter(OGRGeometryPtr& geom_ptr, LineFilter& filter, uint iterations)
{
  try
  {
    geom_ptr.reset(apply_filter(geom_ptr.get(), filter, iterations));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// Apply the filter
void IsolineFilter::apply(std::vector<OGRGeometryPtr>& geoms, bool preserve_topology) const
{
  try
  {
    if (type == IsolineFilter::Type::None || iterations == 0 || radius <= 0)
      return;

    LineFilter filter(type, radius);

    if (preserve_topology)
      for (const auto& geom_ptr : geoms)
        if (geom_ptr && !geom_ptr->IsEmpty())
          filter.count(geom_ptr.get());

    for (auto& geom_ptr : geoms)
      if (geom_ptr && !geom_ptr->IsEmpty())
        apply_filter(geom_ptr, filter, iterations);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Initialize from JSON config
void IsolineFilter::init(Json::Value& theJson)
{
  try
  {
    if (theJson.isNull())
      return;
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isoline filter settings must be in a JSON object");

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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Alter the pixel radius to a metric one based on a BBOX
void IsolineFilter::bbox(const Fmi::Box& box)
{
  double x1 = 0;
  double y1 = 0;
  double x2 = radius;
  double y2 = 0;
  box.itransform(x1, y1);
  box.itransform(x2, y2);
  radius = std::hypot(x2 - x1, y2 - y1);
}

// Hash value for caching purposes
std::size_t IsolineFilter::hash_value() const
{
  try
  {
    auto hash = Fmi::hash_value(static_cast<int>(type));
    Fmi::hash_combine(hash, Fmi::hash_value(radius));
    Fmi::hash_combine(hash, Fmi::hash_value(iterations));
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
