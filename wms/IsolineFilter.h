// ======================================================================
/*!
 * \brief Various filters for isolines/isobands to smoothen them etc
 */
// ======================================================================

#pragma once

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
  // Returns SVG path data with cubic Bezier C commands.
  std::string toBezierSvg(const OGRGeometry& geom,
                          const Fmi::Box& box,
                          double precision) const;

 private:
  Fmi::GeometrySmoother smoother;

  // Bezier fitting settings
  double m_bezierAccuracy = 0;  // 0 = disabled; accuracy in pixels
  int m_bezierMaxDepth = 10;

  // Vertex counter populated during apply() when bezier is enabled.
  // Always populated so that shared edges are detected unconditionally.
  Fmi::VertexCounter m_vertexCounter;

  // Internal helpers for Bezier SVG export
  void writeBezierLineStringSvg(std::string& out,
                                const OGRLineString* geom,
                                const Fmi::Box& box,
                                double rfactor,
                                int decimals) const;

  void writeBezierLinearRingSvg(std::string& out,
                                const OGRLinearRing* geom,
                                const Fmi::Box& box,
                                double rfactor,
                                int decimals) const;

  void writeBezierSvg(std::string& out,
                      const OGRGeometry* geom,
                      const Fmi::Box& box,
                      double rfactor,
                      int decimals) const;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
