// ======================================================================
/*!
 * \brief Specify regularly spaced grid positions
 */
// ======================================================================

#pragma once
#include "Intersections.h"
#include "Locations.h"
#include "Map.h"
#include "Stations.h"
#include <boost/optional.hpp>
#include <engines/geonames/Engine.h>
#include <gis/Box.h>
#include <json/json.h>
#include <cstddef>

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

class Positions
{
 public:
  enum class Layout
  {
    Grid,           // evenly spaced points
    Data,           // points in querydata, all stations or all flashes
    Graticule,      // points in a graticule
    GraticuleFill,  // points in a graticule and graticule cells
    Keyword,        // points listed in the database for a keyword
    LatLon,         // explicitly listed latlon coordinates
    Station         // explicitly listed stations
  };

  struct Point
  {
    int x;
    int y;
    NFmiPoint latlon;
    int dx = 0;  // point specific adjustment for associated data such as numbers
    int dy = 0;
    Point(int theX, int theY, const NFmiPoint& theLatLon) : x(theX), y(theY), latlon(theLatLon) {}
    Point(int theX, int theY, const NFmiPoint& theLatLon, int theDX, int theDY)
        : x(theX), y(theY), latlon(theLatLon), dx(theDX), dy(theDY)
    {
    }
    Point(double theX, double theY, const NFmiPoint& theLatLon, int theDX, int theDY)
        : x(lround(theX)), y(lround(theY)), latlon(theLatLon), dx(theDX), dy(theDY)
    {
    }
  };
  using Points = std::vector<Point>;

  void init(const Json::Value& theJson, const Config& theConfig);
  std::size_t hash_value(const State& theState) const;

  void init(const boost::optional<std::string>& theProducer,
            const Projection& theProjection,
            const boost::posix_time::ptime& theTime,
            const State& theState);

  Points getPoints(const Engine::Querydata::Q& theQ,
                   const std::shared_ptr<OGRSpatialReference>& theCRS,
                   const Fmi::Box& theBox,
                   bool forecastMode) const;

  bool inside(double theX, double theY, bool forecastMode) const;

  bool insideShapes(double theX, double theY) const;
  bool inside(double theX, double theY, const Intersections::IntersectValues& theValues) const;
  void addMargins(int theXMargin, int theYMargin);

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

  // Station layout settings
  Stations stations;

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

  // Extra clipping margins
  int xmargin = 0;
  int ymargin = 0;

 private:
  Points getGridPoints(const Engine::Querydata::Q& theQ,
                       const std::shared_ptr<OGRSpatialReference>& theCRS,
                       const Fmi::Box& theBox,
                       bool forecastMode) const;

  Points getDataPoints(const Engine::Querydata::Q& theQ,
                       const std::shared_ptr<OGRSpatialReference>& theCRS,
                       const Fmi::Box& theBox,
                       bool forecastMode) const;

  Points getGraticulePoints(const Engine::Querydata::Q& theQ,
                            const std::shared_ptr<OGRSpatialReference>& theCRS,
                            const Fmi::Box& theBox,
                            bool forecastMode) const;

  Points getGraticuleFillPoints(const Engine::Querydata::Q& theQ,
                                const std::shared_ptr<OGRSpatialReference>& theCRS,
                                const Fmi::Box& theBox,
                                bool forecastMode) const;

  Points getKeywordPoints(const Engine::Querydata::Q& theQ,
                          const std::shared_ptr<OGRSpatialReference>& theCRS,
                          const Fmi::Box& theBox,
                          bool forecastMode) const;

  Points getLatLonPoints(const Engine::Querydata::Q& theQ,
                         const std::shared_ptr<OGRSpatialReference>& theCRS,
                         const Fmi::Box& theBox,
                         bool forecastMode) const;

  Points getStationPoints(const Engine::Querydata::Q& theQ,
                          const std::shared_ptr<OGRSpatialReference>& theCRS,
                          const Fmi::Box& theBox,
                          bool forecastMode) const;

  OGRGeometryPtr inshape;
  OGRGeometryPtr outshape;
  std::list<OGRGeometryPtr> intersectionshapes;

  // not part of the UI - not involved in the hash
  boost::posix_time::ptime time;
  const Engine::Geonames::Engine* geonames;

  const Engine::Gis::Engine* gisengine;

};  // class Positions

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
