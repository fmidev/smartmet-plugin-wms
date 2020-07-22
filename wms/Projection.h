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

#include <boost/optional.hpp>
#include <engines/querydata/Q.h>
#include <json/json.h>
#include <string>

class OGRSpatialReference;

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

namespace HTTP
{
class Request;
}

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
  void init(const Json::Value& theJson, const State& theState, const Config& theConfig);

  // Coordinate reference system
  boost::optional<std::string> crs;

  // At least one of these must be given, both if cx & cy are used:
  boost::optional<int> xsize;
  boost::optional<int> ysize;

  // Bounding box with corners:
  boost::optional<double> x1;
  boost::optional<double> y1;
  boost::optional<double> x2;
  boost::optional<double> y2;

  // OR Bounding box with center and resolution:
  boost::optional<double> cx;
  boost::optional<double> cy;
  mutable boost::optional<double> resolution;

  // Optional spatial reference for the bounding box coordinates:
  boost::optional<std::string> bboxcrs;

  std::size_t hash_value(const State& theState) const;

  // Update with querydata if necessary

  void update(const Engine::Querydata::Q& theQ);

  std::string getProjString() const;
  std::shared_ptr<OGRSpatialReference> getCRS() const;
  const Fmi::Box& getBox() const;

  // User is responsible for calling getCRS or getBox first
  const NFmiPoint& bottomLeftLatLon() const { return itsBottomLeft; }
  const NFmiPoint& topRightLatLon() const { return itsTopRight; }

 private:
  // Cached results
  void prepareCRS() const;
  mutable std::shared_ptr<OGRSpatialReference> ogr_crs;
  mutable boost::shared_ptr<Fmi::Box> box;

  mutable NFmiPoint itsBottomLeft;
  mutable NFmiPoint itsTopRight;

  // Was the center set using geoid or name?
  bool latlon_center = false;

  const Engine::Gis::Engine* gisengine = nullptr;  // not owned

};  // class Projection

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
