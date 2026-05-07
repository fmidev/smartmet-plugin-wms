// ======================================================================
/*!
 * \brief Various filters for isolines/isobands to smoothen them etc
 */
// ======================================================================

#pragma once

#include "BezierFit.h"
#include <gis/GeometrySmoother.h>
#include <gis/Types.h>
#include <gis/VertexCounter.h>
#include <json/json.h>
#include <ogr_geometry.h>
#include <string>
#include <vector>

namespace Fmi
{
class Box;
}  // namespace Fmi

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

class BezierCache;

class IsolineFilter
{
 public:
  void init(Json::Value& theJson);
  std::size_t hash_value() const;

  void bbox(const Fmi::Box& box);
  void apply(std::vector<OGRGeometryPtr>& geoms, bool preserve_topology);

  // Bezier fitting support
  bool bezierEnabled() const { return m_bezierAccuracy > 0; }

  // Export geometry to SVG with Bezier curves.
  // Must be called after apply() which populates the vertex counter.
  // The box transforms from projection to pixel coordinates.
  // The cache shares fitted cubics between adjacent isobands (and any
  // isolines that overlay them) so shared edges are bit-identical and
  // gap-free. Pass nullptr to fit without caching.
  std::string toBezierSvg(const OGRGeometry& geom,
                          const Fmi::Box& box,
                          double precision,
                          BezierCache* cache = nullptr) const;

 private:
  Fmi::GeometrySmoother smoother;

  // Bezier fitting settings
  double m_bezierAccuracy = 0;  // 0 = disabled; accuracy in pixels
  int m_bezierMaxDepth = 10;

  // Vertex counter populated during apply() when bezier is enabled.
  // Always populated so that shared edges are detected unconditionally.
  Fmi::VertexCounter m_vertexCounter;

  // Fit a polyline to cubics, optionally reusing a cached result for the
  // same canonical-direction polyline. The returned cubics are in the
  // direction of the input points (cache reverses internally if needed).
  // When closed=true the polyline must include its closing duplicate
  // (v_0 at both ends) and the fit is C1-continuous across the closure.
  std::vector<Fmi::BezierFit::CubicBez> fitWithCache(
      const std::vector<Fmi::BezierFit::Point>& points,
      BezierCache* cache,
      bool closed = false) const;

  // Internal helpers for Bezier SVG export
  void writeBezierLineStringSvg(std::string& out,
                                const OGRLineString* geom,
                                const Fmi::Box& box,
                                double rfactor,
                                int decimals,
                                BezierCache* cache) const;

  void writeBezierLinearRingSvg(std::string& out,
                                const OGRLinearRing* geom,
                                const Fmi::Box& box,
                                double rfactor,
                                int decimals,
                                BezierCache* cache) const;

  void writeBezierSvg(std::string& out,
                      const OGRGeometry* geom,
                      const Fmi::Box& box,
                      double rfactor,
                      int decimals,
                      BezierCache* cache) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
