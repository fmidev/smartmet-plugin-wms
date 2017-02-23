// ======================================================================
/*!
 * \brief Specify regularly spaced grid positions
 */
// ======================================================================

#pragma once
#include "Intersections.h"
#include "Locations.h"
#include "Map.h"
#include <engines/geonames/Engine.h>
#include <gis/Box.h>
#include <json/json.h>
#include <boost/optional.hpp>
#include <cstddef>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;

class Positions
{
  enum class Layout
  {
    Grid,           // evenly spaced points
    Data,           // points in querydata
    Graticule,      // points in a graticule
    GraticuleFill,  // points in a graticule and graticule cells
    Keyword,        // points listed in the database for a keyword
    LatLon          // explicitly listed latlon coordinates
  };

 public:
  struct Point
  {
    int x;
    int y;
    NFmiPoint latlon;
    Point(int theX, int theY, const NFmiPoint& theLatLon) : x(theX), y(theY), latlon(theLatLon) {}
  };
  typedef std::vector<Point> Points;

  void init(const Json::Value& theJson, const Config& theConfig);
  virtual std::size_t hash_value(const State& theState) const;

  void init(SmartMet::Engine::Querydata::Q q,
            const Projection& theProjection,
            const boost::posix_time::ptime& theTime,
            const State& theState);

  Points getPoints(SmartMet::Engine::Querydata::Q theQ,
                   boost::shared_ptr<OGRSpatialReference> theCRS,
                   const Fmi::Box& theBox) const;

  bool inside(double theX, double theY) const;

  // Layout algorithm
  Layout layout = Layout::Grid;

  // Grid layout settings. dx,dy offsets for Keyword and Latlon settings

  boost::optional<int> x;    // start location for grid layout
  boost::optional<int> y;    //
  boost::optional<int> dx;   // location stepsizes in pixel units
  boost::optional<int> dy;   // or offset from location for other algorithms
  boost::optional<int> ddx;  // horizontal stagger for the next row

  // Graticule/GraticuleFill layout settings:

  int size = 10;  // degrees - size size of the graticule cell

  // Graticule layout settings:

  int step = 1;  // degrees - step size of individual symbols

  // GraticuleFill layout settings:

  double mindistance = 50;  // minimum distance between parallel or meridian points in pixels

  // Keyword layout settings;
  std::string keyword = "";

  // LatLon layout settings:
  Locations locations;

  // Common settings for all algorithms:

  // Optional directional adjustments
  int directionoffset = 0;
  int rotate = 0;
  std::string direction = "";
  std::string u = "";
  std::string v = "";

  // Optional regional requirements (map names)
  boost::optional<Map> outsidemap;
  boost::optional<Map> insidemap;
  Intersections intersections;

 private:
  Points getGridPoints(SmartMet::Engine::Querydata::Q theQ,
                       boost::shared_ptr<OGRSpatialReference> theCRS,
                       const Fmi::Box& theBox) const;

  Points getDataPoints(SmartMet::Engine::Querydata::Q theQ,
                       boost::shared_ptr<OGRSpatialReference> theCRS,
                       const Fmi::Box& theBox) const;

  Points getGraticulePoints(SmartMet::Engine::Querydata::Q theQ,
                            boost::shared_ptr<OGRSpatialReference> theCRS,
                            const Fmi::Box& theBox) const;

  Points getGraticuleFillPoints(SmartMet::Engine::Querydata::Q theQ,
                                boost::shared_ptr<OGRSpatialReference> theCRS,
                                const Fmi::Box& theBox) const;

  Points getKeywordPoints(SmartMet::Engine::Querydata::Q theQ,
                          boost::shared_ptr<OGRSpatialReference> theCRS,
                          const Fmi::Box& theBox) const;

  Points getLatLonPoints(SmartMet::Engine::Querydata::Q theQ,
                         boost::shared_ptr<OGRSpatialReference> theCRS,
                         const Fmi::Box& theBox) const;

  OGRGeometryPtr inshape;
  OGRGeometryPtr outshape;
  std::list<OGRGeometryPtr> intersectionshapes;

  // not part of the UI - not involved in the hash
  boost::posix_time::ptime time;
  const SmartMet::Engine::Geonames::Engine* geonames;

};  // class Positions

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
