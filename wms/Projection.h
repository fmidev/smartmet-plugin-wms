// ======================================================================
/*!
 * \brief Spatial reference plus bounding box
 *
 * Alternative references:
 *
 * - EPSG using "epsg"
 * - WKT using "wkt"
 * - PROJ.4 using "proj"
 *
 * Alternative bounding box definitions:
 *
 * - x1, y2, x2, y2
 * - cx, cy, resolution
 * - bbox
 *
 * Size definitions:
 *
 * - xsize
 * - ysize
 *
 * The number of array elements determines the manner in which
 * the bounding box is defined. The coordinates are in the
 * given spatial reference unless an alternative is defined
 * using
 *
 * - "bboxeps"
 * - "bboxwkt"
 * - "bboxproj"
 *
 * The resolution defines how many kilometers one pixel is. For
 * example, a resolution of 0.5 means one pixel is 500 meters.
 */
// ======================================================================

#pragma once

#include <engines/querydata/Q.h>
#include <gis/SpatialReference.h>
#include <json/json.h>
#include <optional>
#include <string>

namespace Fmi
{
class Box;
}

namespace SmartMet
{
namespace Engine
{
namespace Gis
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace Dali
{
class Config;
class State;

class Projection
{
 public:
  // Initialize from JSON
  void init(Json::Value& theJson, const State& theState, const Config& theConfig);

  // Coordinate reference system
  std::optional<std::string> crs;

  // The parameter used for the projection (grid-suport)
  std::optional<std::string> projectionParameter;

  // If this is set then the original grid size is multiplied by this value.
  std::optional<double> size;

  // At least one of these must be given, both if cx & cy are used:
  std::optional<int> xsize;
  std::optional<int> ysize;

  // Bounding box with corners:
  std::optional<double> x1;
  std::optional<double> y1;
  std::optional<double> x2;
  std::optional<double> y2;

  // OR Bounding box with center and resolution:
  std::optional<double> cx;
  std::optional<double> cy;
  mutable std::optional<double> resolution;

  // Optional spatial reference for the bounding box coordinates:
  std::optional<std::string> bboxcrs;

  std::size_t hash_value(const State& theState) const;

  // Update with querydata if necessary

  void update(const Engine::Querydata::Q& theQ);

  const Fmi::SpatialReference& getCRS() const;
  const Fmi::Box& getBox() const;

  // User is responsible for calling getCRS or getBox first
  const NFmiPoint& bottomLeftLatLon() const { return itsBottomLeft; }
  const NFmiPoint& topRightLatLon() const { return itsTopRight; }

  void setBottomLeft(double x, double y) { itsBottomLeft.Set(x, y); }
  void setTopRight(double x, double y) { itsTopRight.Set(x, y); }

 private:
  // Cached results
  void prepareCRS() const;

  // The following are initialized lazily
  mutable std::shared_ptr<Fmi::SpatialReference> ogr_crs;
  mutable std::shared_ptr<Fmi::Box> box;

  mutable NFmiPoint itsBottomLeft;
  mutable NFmiPoint itsTopRight;

  // Was the center set using geoid or name?
  bool latlon_center = false;

  const Engine::Gis::Engine* gisengine = nullptr;  // not owned

};  // class Projection

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
