#include "Positions.h"
#include "Config.h"
#include "Hash.h"
#include "Projection.h"
#include <boost/move/make_unique.hpp>
#include <engines/querydata/ParameterOptions.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <grid-files/identification/GridDef.h>
#include <macgyver/Exception.h>
#include <macgyver/NearTree.h>
#include <spine/Convenience.h>
#include <timeseries/ParameterFactory.h>
#include <iomanip>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// For NearTree calculations
class XY
{
 public:
  XY(double theX, double theY) : itsX(theX), itsY(theY) {}
  double x() const { return itsX; }
  double y() const { return itsY; }

 private:
  double itsX;
  double itsY;
};

// ----------------------------------------------------------------------
/*!
 * \brief Length of a segment
 */
// ----------------------------------------------------------------------

double length(double x1, double y1, double x2, double y2)
{
  return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given graticule cell overlaps the bounding box
 */
// ----------------------------------------------------------------------

bool overlaps(const Fmi::Box& theBox,
              double x1,
              double y1,
              double x2,
              double y2,
              double x3,
              double y3,
              double x4,
              double y4)
{
  try
  {
    // extent of the grid cell
    double xmin = std::min(std::min(x1, x3), std::min(x2, x4));
    double xmax = std::max(std::max(x1, x3), std::max(x2, x4));
    double ymin = std::min(std::min(y1, y3), std::min(y2, y4));
    double ymax = std::max(std::max(y1, y3), std::max(y2, y4));

    // intersection with the box
    xmin = std::max(xmin, 0.0);
    xmax = std::min(xmax, static_cast<double>(theBox.width()));
    ymin = std::max(ymin, 0.0);
    ymax = std::min(ymax, static_cast<double>(theBox.height()));

    // is the intersection non-empty?
    return (xmax > xmin && ymax > ymin);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Apply direction based adjustments to the selected positions
 *
 * We know that only direction or u- and v-components are set
 */
// ----------------------------------------------------------------------

void apply_direction_offsets(Positions::Points& thePoints,
                             const Engine::Querydata::Q& theQ,
                             const Fmi::DateTime& theTime,
                             int theOffset,
                             int theRotation,
                             const std::string& theDirection,
                             const std::string& theU,
                             const std::string& theV)
{
  try
  {
    if (theOffset == 0 || !theQ)
      return;

    auto& q = *theQ;

    const double pi = 3.141592658979323;

    if (!theDirection.empty())
    {
      auto param = TS::ParameterFactory::instance().parse(theDirection);
      if (!q.param(param.number()))
        throw Fmi::Exception(
            BCP, "Parameter '" + theDirection + "' is not available for position selection!");

      for (auto& point : thePoints)
      {
        double dir = q.interpolate(point.latlon, theTime, 180);
        if (dir != kFloatMissing)
        {
          point.x += lround(theOffset * cos((dir + 90 + theRotation) * pi / 180.0));
          point.y += lround(theOffset * sin((dir + 90 + theRotation) * pi / 180.0));
        }
      }
    }
    else
    {
      auto uparam = TS::ParameterFactory::instance().parse(theU);
      auto vparam = TS::ParameterFactory::instance().parse(theV);

      // Q API SUCKS
      std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
      Fmi::LocalDateTime localdatetime(theTime, Fmi::TimeZonePtr::utc);
      std::string tmp;
      auto mylocale = std::locale::classic();
      NFmiPoint dummy;

      for (auto& point : thePoints)
      {
        // Q API SUCKS

        Spine::Location loc(point.latlon.X(), point.latlon.Y());

        auto up = Engine::Querydata::ParameterOptions(uparam,
                                                      tmp,
                                                      loc,
                                                      tmp,
                                                      tmp,
                                                      *timeformatter,
                                                      tmp,
                                                      tmp,
                                                      mylocale,
                                                      tmp,
                                                      false,
                                                      dummy,
                                                      dummy);
        auto uresult = q.value(up, localdatetime);

        auto vp = Engine::Querydata::ParameterOptions(vparam,
                                                      tmp,
                                                      loc,
                                                      tmp,
                                                      tmp,
                                                      *timeformatter,
                                                      tmp,
                                                      tmp,
                                                      mylocale,
                                                      tmp,
                                                      false,
                                                      dummy,
                                                      dummy);
        auto vresult = q.value(vp, localdatetime);

        if (boost::get<double>(&uresult) != nullptr && boost::get<double>(&vresult) != nullptr)
        {
          auto uspd = *boost::get<double>(&uresult);
          auto vspd = *boost::get<double>(&vresult);

          if (uspd != kFloatMissing && vspd != kFloatMissing && (uspd != 0 || vspd != 0))
          {
            double dir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
            point.x += lround(theOffset * cos((dir + 90 + theRotation) * pi / 180.0));
            point.y += lround(theOffset * sin((dir + 90 + theRotation) * pi / 180.0));
          }
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void Positions::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Positions JSON is not a JSON object!");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "layout")
      {
        std::string tmp = json.asString();
        if (tmp == "grid")
          layout = Layout::Grid;
        else if (tmp == "data")
          layout = Layout::Data;
        else if (tmp == "graticule")
          layout = Layout::Graticule;
        else if (tmp == "graticulefill")
          layout = Layout::GraticuleFill;
        else if (tmp == "keyword")
          layout = Layout::Keyword;
        else if (tmp == "latlon")
          layout = Layout::LatLon;
        else if (tmp == "station")
          layout = Layout::Station;
        else
          throw Fmi::Exception(BCP, "Unknown layout type for positions: '" + tmp + "'!");
      }

      else if (name == "x")
        x = json.asInt();
      else if (name == "y")
        y = json.asInt();
      else if (name == "dx")
        dx = json.asInt();
      else if (name == "dy")
        dy = json.asInt();
      else if (name == "ddx")
        ddx = json.asInt();

      else if (name == "xmargin")
        xmargin = json.asInt();
      else if (name == "ymargin")
        ymargin = json.asInt();

      else if (name == "directionoffset")
        directionoffset = json.asInt();
      else if (name == "rotate")
        rotate = json.asInt();
      else if (name == "direction")
        direction = json.asString();
      else if (name == "u")
        u = json.asString();
      else if (name == "v")
        v = json.asString();

      else if (name == "size")
        size = json.asUInt();
      else if (name == "step")
        step = json.asUInt();

      else if (name == "mindistance")
        mindistance = json.asDouble();

      else if (name == "keyword")
        keyword = json.asString();

      else if (name == "locations")
        locations.init(json, theConfig);

      else if (name == "stations")
        stations.init(json, theConfig);

      else if (name == "outside")
      {
        outsidemap.reset(Map());
        outsidemap->init(json, theConfig);
      }
      else if (name == "inside")
      {
        insidemap.reset(Map());
        insidemap->init(json, theConfig);
      }
      else if (name == "intersect")
        intersections.init(json, theConfig);

      else
        throw Fmi::Exception(BCP, "Positions does not have a setting named '" + name + "'!");
    }

    if (size <= 0)
      throw Fmi::Exception(BCP, "Positions size-setting must be nonnegative!");

    if (step <= 0)
      throw Fmi::Exception(BCP, "Positions step-setting must be nonnegative!");

    if (step > size)
      throw Fmi::Exception(BCP, "Positions step-setting must be smaller than the size-setting!");

    if (180 % step != 0)
      throw Fmi::Exception(BCP, "Positions step-size must divide 180 degrees evenly");

    // we cannot allow stuff like 1e-6, that'll generate a huge amounts of points
    if (mindistance < 2)
      throw Fmi::Exception(BCP, "Minimum distance between positions must be at least 2 pixels!");

    if (!direction.empty() && (!u.empty() || !v.empty()))
      throw Fmi::Exception(
          BCP, "Cannot specify position offsets using both direction and the u- and v-components!");

    if ((!u.empty() && v.empty()) || (!v.empty() && u.empty()))
      throw Fmi::Exception(BCP,
                           "Cannot specify only one of u- and v-components for position offsets!");

    if (directionoffset != 0 && direction.empty() && u.empty() && v.empty())
      throw Fmi::Exception(BCP,
                           "Must specify direction parameter for direction offset of positions!");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the geometries to be used for intersections
 */
// ----------------------------------------------------------------------

void Positions::init(const std::optional<std::string>& theProducer,
                     const Projection& theProjection,
                     const Fmi::DateTime& theTime,
                     const State& theState)
{
  try
  {
    // For applying directional corrections to positions
    time = theTime;

    // Needed engines
    geonames = &theState.getGeoEngine();
    gisengine = &theState.getGisEngine();

    // Initialize clipping shapes

    if (insidemap)
    {
      inshape = gisengine->getShape(&theProjection.getCRS(), insidemap->options);
      if (!inshape)
        throw Fmi::Exception(BCP, "Positions received empty inside-shape from database!");

      // This does not obey layer margins, hence we disable this speed optimization
      // inshape.reset(Fmi::OGR::polyclip(*inshape, theProjection.getBox()));
    }

    if (outsidemap)
    {
      outshape = gisengine->getShape(&theProjection.getCRS(), outsidemap->options);

      // This does not obey layer margings, hence we disable this speed optimization
      // if (outshape)
      // outshape.reset(Fmi::OGR::polyclip(*outshape, theProjection.getBox()));
    }

    intersections.init(theProducer, theProjection, theTime, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add layer margins to position margins
 */
// ----------------------------------------------------------------------

void Positions::addMargins(int theXMargin, int theYMargin)
{
  xmargin = std::max(xmargin, theXMargin);
  ymargin = std::max(ymargin, theYMargin);
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the locations to be rendered
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getPoints(const Engine::Querydata::Q& theQ,
                                       const Fmi::SpatialReference& theCRS,
                                       const Fmi::Box& theBox,
                                       bool forecastMode,
                                       const State& theState) const
{
  try
  {
    switch (layout)
    {
      case Layout::Grid:
        return getGridPoints(theQ, theCRS, theBox, forecastMode);
      case Layout::Data:
        return getDataPoints(theQ, theCRS, theBox, forecastMode, theState);
      case Layout::Graticule:
        return getGraticulePoints(theQ, theCRS, theBox, forecastMode);
      case Layout::GraticuleFill:
        return getGraticuleFillPoints(theQ, theCRS, theBox, forecastMode);
      case Layout::Keyword:
        return getKeywordPoints(theQ, theCRS, theBox, forecastMode);
      case Layout::LatLon:
        return getLatLonPoints(theQ, theCRS, theBox, forecastMode);
      case Layout::Station:
        return getStationPoints(theQ, theCRS, theBox, forecastMode);
    }
    // Dummy to prevent g++ from complaining
    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the locations to be rendered
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getPoints(const char* originalCrs,
                                       int originalWidth,
                                       int originalHeight,
                                       uint originalGeometryId,
                                       const Fmi::SpatialReference& theCRS,
                                       const Fmi::Box& theBox) const
{
  try
  {
    switch (layout)
    {
      case Layout::Grid:
        return getGridPoints(nullptr, theCRS, theBox, true);
      case Layout::Data:
        return getDataPoints(
            originalCrs, originalWidth, originalHeight, originalGeometryId, theCRS, theBox);
      case Layout::Graticule:
        return getGraticulePoints(nullptr, theCRS, theBox, true);
      case Layout::GraticuleFill:
        return getGraticuleFillPoints(nullptr, theCRS, theBox, true);
      case Layout::Keyword:
        return getKeywordPoints(nullptr, theCRS, theBox, true);
      case Layout::LatLon:
        return getLatLonPoints(nullptr, theCRS, theBox, true);
      case Layout::Station:
        return getStationPoints(nullptr, theCRS, theBox, true);
    }
    // Dummy to prevent g++ from complaining
    return {};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a grid of locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGridPoints(const Engine::Querydata::Q& theQ,
                                           const Fmi::SpatialReference& theCRS,
                                           const Fmi::Box& theBox,
                                           bool forecastMode) const
{
  try
  {
    // Get the data projection

    Fmi::CoordinateTransformation transformation(theCRS, "WGS84");

    // Use defaults for this layout if nothing is specified

    if (!!dx && *dx <= 0)
      throw Fmi::Exception(BCP, "Positions dx-setting must be nonnegative for grid-layouts!");

    if (!!dy && *dy <= 0)
      throw Fmi::Exception(BCP, "Positions dy-setting must be nonnegative for grid-layouts!");

    int xstart = (!!x ? *x : 5);
    int ystart = (!!y ? *y : 5);
    int deltax = (!!dx ? *dx : 20);
    int deltay = (!!dy ? *dy : 20);
    int deltaxx = (!!ddx ? *ddx : 0);

    // Sanity check
    if (deltaxx >= deltax)
      throw Fmi::Exception(BCP, "ddx must be smaller than dx when generating a grid layout!");

    // Generate the grid coordinates

    Points points;

    // Cover margins too
    while (xstart - deltax >= -xmargin)
      xstart -= deltax;
    while (ystart - deltay >= -ymargin)
      ystart -= deltay;

    int row = 0;

    // TODO(mheiskan): Must alter these if there is a margin
    const int height = theBox.height();
    const int width = theBox.width();

    // This is for backwards compatibility - original code which did not handle
    // the margins had a logic error
    xstart -= deltaxx;

    for (int ypos = ystart; ypos < (height + ymargin); ypos += deltay)
    {
      // Stagger every other row
      if (row++ % 2 == 0)
        xstart += deltaxx;
      else
        xstart -= deltaxx;

      for (int xpos = xstart; xpos < (width + xmargin); xpos += deltax)
      {
        // Convert pixel coordinate to world coordinate (or latlon for geographic spatial
        // references)
        auto xcoord = static_cast<double>(xpos);
        auto ycoord = static_cast<double>(ypos);

        theBox.itransform(xcoord, ycoord);

        // Skip if the coordinate is outside the desired shapes

        if (!inside(xcoord, ycoord, forecastMode))
          continue;

        // Convert world coordinate to latlon, skipping points which cannot be handled

        if (!transformation.transform(xcoord, ycoord))
          continue;

        points.emplace_back(Point(xpos, ypos, NFmiPoint(xcoord, ycoord)));
      }
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate querydata locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getDataPoints(const Engine::Querydata::Q& theQ,
                                           const Fmi::SpatialReference& theCRS,
                                           const Fmi::Box& theBox,
                                           bool forecastMode,
                                           const State& theState) const
{
  // Cannot generate any points without any querydata. If layout=data,
  // the layers will handle stations and flashes by themselves.

  if (!theQ)
    return {};

  try
  {
    // We need querydata coordinates in image world coordinates (theCRS). The engine caches the
    // result.

    const auto& qengine = theState.getQEngine();

    const auto latlons_ptr = qengine.getWorldCoordinates(theQ, "WGS84");
    if (!latlons_ptr)
      throw Fmi::Exception(BCP, "Latlon coordinates not available for the chosen data");
    const auto& latlons = *latlons_ptr;

    const auto coords_ptr = qengine.getWorldCoordinates(theQ, theCRS);
    if (!coords_ptr)
      throw Fmi::Exception(BCP, "Failed to get data coordinates in the chosen spatial reference");
    const auto& coords = *coords_ptr;

    // Generate the image coordinates

    Points points;

    for (auto j = 0UL; j < latlons.height(); j++)
      for (auto i = 0UL; i < latlons.width(); i++)
      {
        double xcoord = coords.x(i, j);
        double ycoord = coords.y(i, j);

        if (std::isnan(xcoord) || std::isnan(ycoord))
          continue;

        // Image world coordinate to pixel coordinate
        theBox.transform(xcoord, ycoord);

        int deltax = 0;
        if (dx)
          deltax += *dx;
        int deltay = 0;
        if (dy)
          deltay += *dy;

        // Skip if not inside desired shapes

        NFmiPoint latlon(latlons.x(i, j), latlons.y(i, j));
        if (inside(latlon.X(), latlon.Y(), forecastMode))
          points.emplace_back(Point(xcoord, ycoord, latlon, deltax, deltay));
      }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Positions::Points Positions::getDataPoints(const char* /* originalCrs */,
                                           int originalWidth,
                                           int originalHeight,
                                           uint originalGeometryId,
                                           const Fmi::SpatialReference& theCRS,
                                           const Fmi::Box& theBox) const
{
  try
  {
    // Create the coordinate transformation from image world coordinates
    // to querydata world coordinates

    Fmi::CoordinateTransformation transformation("WGS84", theCRS);
    Fmi::CoordinateTransformation transformation2(theCRS, "WGS84");

    int deltax = (!!dx ? *dx : 10);
    int deltay = (!!dy ? *dy : 10);

    T::Coordinate_svec originalCoordinates;
    if (originalGeometryId > 0)
      originalCoordinates =
          Identification::gridDef.getGridLatLonCoordinatesByGeometryId(originalGeometryId);

    Points points;
    if (originalCoordinates)
    {
      // TODO: This should be optimized for speed like get above getDataPoints method
      // by doing all the projections with one call, and preferably using an engine which
      // could cache the results.

      uint pos = 0;
      uint sz = originalCoordinates->size();
      for (int y = 0; y < originalHeight; y = y + deltay)
      {
        for (int x = 0; x < originalWidth; x = x + deltax)
        {
          pos = y * originalWidth + x;
          if (pos < sz)
          {
            auto cc = (*originalCoordinates)[pos];
            if (inside(cc.x(), cc.y(), false))
            {
              double xx = cc.x();
              double yy = cc.y();

              transformation.transform(xx, yy);

              double xp = xx;
              double yp = yy;

              theBox.transform(xp, yp);

              NFmiPoint coordinate(cc.x(), cc.y());
              points.emplace_back(Point(xp, yp, coordinate, deltax, deltay));
            }
          }
        }
      }
    }
    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGraticulePoints(const Engine::Querydata::Q& theQ,
                                                const Fmi::SpatialReference& theCRS,
                                                const Fmi::Box& theBox,
                                                bool forecastMode) const
{
  try
  {
    // First generate the candidates so we can project them all at once for speed

    std::vector<double> longitudes;
    std::vector<double> latitudes;
    longitudes.reserve(360 / step);
    latitudes.reserve(180 / step);

    // This loop order makes it easier to handle the poles only once
    for (int lat = -90; lat <= 90; lat += step)
      for (int lon = -180; lon < 180; lon += step)
      {
        if (lon % size == 0 || lat % size == 0)
        {
          longitudes.push_back(lon);
          latitudes.push_back(lat);
        }
        // Handle the poles only once
        if (lat == -90 || lat == 90)
          break;
      }

    // Create the coordinate transformation from WGS84 to projection coordinates

    Fmi::CoordinateTransformation transformation("WGS84", theCRS);

    auto xcoord = longitudes;
    auto ycoord = latitudes;
    transformation.transform(xcoord, ycoord);

    // Generate the graticule coordinates.

    Points points;

    // This loop order makes it easier to handle the poles only once
    for (auto i = 0UL; i < xcoord.size(); i++)
    {
      // world coordinate to pixel coordinate

      theBox.transform(xcoord[i], ycoord[i]);

      int deltax = 0;
      if (dx)
        deltax += *dx;

      int deltay = 0;
      if (dy)
        deltay += *dy;

      // Skip if not inside desired areas
      if (inside(longitudes[i], latitudes[i], forecastMode))
        points.emplace_back(
            Point(xcoord[i], ycoord[i], NFmiPoint(longitudes[i], latitudes[i]), deltax, deltay));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule fill locations
 *
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGraticuleFillPoints(const Engine::Querydata::Q& theQ,
                                                    const Fmi::SpatialReference& theCRS,
                                                    const Fmi::Box& theBox,
                                                    bool forecastMode) const
{
  try
  {
    // Create the coordinate transformation from WGS84 to projection coordinates

    Fmi::CoordinateTransformation transformation("WGS84", theCRS);

    // Generate the graticule coordinates. Algorithm:
    //
    // 1. Loop over all graticule cells
    // 2. Project the 4 corner coordinates
    // 3. If the cell overlaps the output
    //    a) calculate how many points would fit onto each edge
    //    b) generate the points for the edges
    //    c) choose the minimum of the number of points generated for the top & bottom edges
    //    d) choose the minimum of the number of points generated for the left & right edges
    //    e) generate the points inside the cell using dimensions determined in c) and d)
    //
    // Details: - we start from bottom left, and output only bottom & left edges.
    //          - the poles are handled separately

    Points points;

    // Do not allow too close pixels
    Fmi::NearTree<XY> selected_coordinates;

    for (int lat = -90; lat < 90; lat += size)
      for (int lon = -180; lon < 180; lon += size)
      {
        double x1 = lon;         // p3---p4
        double y1 = lat;         // |    |
        double x2 = lon + size;  // p1---p2
        double y2 = lat;
        double x3 = lon;
        double y3 = lat + size;
        double x4 = lon + size;
        double y4 = lat + size;

        // to projection coordinates
        if (!transformation.transform(x1, y1))
          continue;
        if (!transformation.transform(x2, y2))
          continue;
        if (!transformation.transform(x3, y3))
          continue;
        if (!transformation.transform(x4, y4))
          continue;

        // to pixel coordinates
        theBox.transform(x1, y1);
        theBox.transform(x2, y2);
        theBox.transform(x3, y3);
        theBox.transform(x4, y4);

        // We make the assumption that the bounding box very closely
        // matches the bounding box formed by the graticule cell.
        // One could use a margin-setting to allow positions slightly
        // outside the box to fix that.

        if (!overlaps(theBox, x1, y1, x2, y2, x3, y3, x4, y4))
          continue;

        double bottom = length(x1, y1, x2, y2);
        double left = length(x1, y1, x3, y3);
        double top = length(x3, y3, x4, y4);
        double right = length(x2, y2, x4, y4);

        int nbottom = std::floor(bottom / mindistance);
        int nleft = std::floor(left / mindistance);
        int ntop = std::floor(top / mindistance);
        int nright = std::floor(right / mindistance);

        int nparallel = std::min(nbottom, ntop);
        int nmeridian = std::min(nleft, nright);

        // Left edge

        for (int j = 0; j < nleft; j++)
        {
          double newlon = lon;
          double newlat = lat + 1.0 * j / nleft * size;
          double newx = newlon;
          double newy = newlat;

          if (!transformation.transform(newx, newy))
            continue;
          theBox.transform(newx, newy);

          int deltax = 0;
          if (dx)
            deltax += *dx;
          int deltay = 0;
          if (dy)
            deltay += *dy;

          // Skip if not inside desired areas
          if (inside(newlon, newlat, forecastMode))
          {
            XY xy(newx, newy);
            auto match = selected_coordinates.nearest(xy, mindistance);
            if (!match)
            {
              points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));
              selected_coordinates.insert(xy);
            }
          }
        }

        // Bottom edge
        for (int i = 1; i < nbottom; i++)
        {
          double newlon = lon + 1.0 * i / nbottom * size;
          double newlat = lat;
          double newx = newlon;
          double newy = newlat;

          if (!transformation.transform(newx, newy))
            continue;
          theBox.transform(newx, newy);

          int deltax = 0;
          if (dx)
            deltax += *dx;
          int deltay = 0;
          if (dy)
            deltay += *dy;

          // Skip if not inside desired shapes
          if (inside(newlon, newlat, forecastMode))
          {
            XY xy(newx, newy);
            auto match = selected_coordinates.nearest(xy, mindistance);
            if (!match)
            {
              points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));
              selected_coordinates.insert(xy);
            }
          }

          // Handle the south pole only once
          if (newlat == -90)
            break;
        }

        // Inner points

        for (int i = 1; i < nparallel; i++)
          for (int j = 1; j < nmeridian; j++)
          {
            double newlon = lon + 1.0 * i / nparallel * size;
            double newlat = lat + 1.0 * j / nmeridian * size;
            double newx = newlon;
            double newy = newlat;

            if (!transformation.transform(newx, newy))
              continue;
            theBox.transform(newx, newy);

            int deltax = 0;
            if (dx)
              deltax += *dx;
            int deltay = 0;
            if (dy)
              deltay += *dy;

            // Skip if not inside desired area
            if (inside(newlon, newlat, forecastMode))
            {
              XY xy(newx, newy);
              auto match = selected_coordinates.nearest(xy, mindistance);
              if (!match)
              {
                points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));
                selected_coordinates.insert(xy);
              }
            }
          }
      }

    // All that remains is the north pole
    double newlon = 0;
    double newlat = 90;
    double newx = newlon;
    double newy = newlat;

    if (!transformation.transform(newx, newy))
    {
      int deltax = 0;
      if (dx)
        deltax += *dx;
      int deltay = 0;
      if (dy)
        deltay += *dy;

      // Skip if not inside desired areas
      if (inside(newlon, newlat, forecastMode))
      {
        XY xy(newx, newy);
        auto match = selected_coordinates.nearest(xy, mindistance);
        if (!match)
        {
          points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));
          selected_coordinates.insert(xy);
        }
      }
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate keyword locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getKeywordPoints(const Engine::Querydata::Q& theQ,
                                              const Fmi::SpatialReference& theCRS,
                                              const Fmi::Box& theBox,
                                              bool forecastMode) const
{
  try
  {
    // Read the keyword

    if (keyword.empty())
      throw Fmi::Exception(
          BCP, "No keyword given when trying to use a keyword for location definitions!");

    Locus::QueryOptions options;
    auto locations = geonames->keywordSearch(options, keyword);

    // Project the coordinates just once

    std::vector<double> longitudes;
    std::vector<double> latitudes;
    longitudes.reserve(locations.size());
    latitudes.reserve(locations.size());

    for (const auto& location : locations)
    {
      longitudes.push_back(location->longitude);
      latitudes.push_back(location->latitude);
    }

    // Keyword locations are in WGS84

    Fmi::CoordinateTransformation transformation("WGS84", theCRS);

    auto xcoord = longitudes;
    auto ycoord = latitudes;
    transformation.transform(xcoord, ycoord);

    Points points;

    auto i = 0UL;

    for (const auto& location : locations)
    {
      // keyword location latlon
      double lon = location->longitude;
      double lat = location->latitude;

      // To pixel coordinate
      theBox.transform(xcoord[i], ycoord[i]);

      int deltax = 0;
      if (dx)
        deltax += *dx;
      int deltay = 0;
      if (dy)
        deltay += *dy;

      // Skip if not inside desired areas
      if (inside(lon, lat, forecastMode))
        points.emplace_back(Point(xcoord[i], ycoord[i], NFmiPoint(lon, lat), deltax, deltay));

      i++;
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate explicit locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getLatLonPoints(const Engine::Querydata::Q& theQ,
                                             const Fmi::SpatialReference& theCRS,
                                             const Fmi::Box& theBox,
                                             bool forecastMode) const
{
  try
  {
    Points points;

    if (locations.locations.empty())
      return points;

    // Project the coordinates just once

    std::vector<double> longitudes;
    std::vector<double> latitudes;
    longitudes.reserve(locations.locations.size());
    latitudes.reserve(locations.locations.size());

    for (const auto& location : locations.locations)
    {
      if (!location.longitude || !location.latitude)
        throw Fmi::Exception(BCP, "Incomplete location in the locations list!");

      longitudes.push_back(*location.longitude);
      latitudes.push_back(*location.latitude);
    }

    Fmi::CoordinateTransformation transformation("WGS84", theCRS);
    auto xcoord = longitudes;
    auto ycoord = latitudes;
    transformation.transform(xcoord, ycoord);

    for (auto i = 0UL; i < locations.locations.size(); i++)
    {
      double lon = longitudes[i];
      double lat = latitudes[i];

      // To pixel coordinate
      theBox.transform(xcoord[i], ycoord[i]);

      // Global position adjustment
      int deltax = 0;
      if (dx)
        deltax += *dx;

      int deltay = 0;
      if (dy)
        deltay = *dy;

      // Individual adjustments
      const auto& location = locations.locations[i];
      if (location.dx)
        deltax += *location.dx;

      if (location.dy)
        deltay += *location.dy;

      // Skip if not inside desired areas
      if (inside(lon, lat, forecastMode))
        points.emplace_back(Point(xcoord[i], ycoord[i], NFmiPoint(lon, lat), deltax, deltay));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate station locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getStationPoints(const Engine::Querydata::Q& theQ,
                                              const Fmi::SpatialReference& theCRS,
                                              const Fmi::Box& theBox,
                                              bool forecastMode) const
{
  try
  {
    Points points;

    // We pass the station numbers directly to obsengine

    if (!forecastMode)
      return points;

    if (stations.empty())
      return points;

    // Create the coordinate transformation from WGS84 to projection coordinates

    Fmi::CoordinateTransformation transformation("WGS84", theCRS);

    Locus::QueryOptions options;
    auto locations = geonames->keywordSearch(options, keyword);

    // This loop order makes it easier to handle the poles only once
    for (const auto& station : stations.stations)
    {
      options.SetFeatures("SYNOP");
      Spine::LocationList places;

      if (station.fmisid)
      {
        options.SetLanguage("fmisid");
        places = geonames->nameSearch(options, Fmi::to_string(*station.fmisid));
        if (places.empty())
          std::cerr << Spine::log_time_str() << " WMS could not find fmisid " << *station.fmisid
                    << std::endl;
      }
      else if (station.wmo)
      {
        options.SetLanguage("wmo");
        places = geonames->nameSearch(options, Fmi::to_string(*station.wmo));
        if (places.empty())
          std::cerr << Spine::log_time_str() << " WMS could not find wmo " << *station.wmo
                    << std::endl;
      }
      else if (station.lpnn)
      {
        options.SetLanguage("lpnn");
        places = geonames->nameSearch(options, Fmi::to_string(*station.lpnn));
        if (places.empty())
          std::cerr << Spine::log_time_str() << " WMS could not find lpnn " << *station.lpnn
                    << std::endl;
      }
      else if (station.geoid)
      {
        places = geonames->idSearch(options, *station.geoid);
        if (places.empty())
          std::cerr << Spine::log_time_str() << " WMS could not find geoid " << *station.geoid
                    << std::endl;
      }
      else if (station.longitude || station.latitude)
      {
        throw Fmi::Exception(
            BCP, "Use latlon instead of station layout for forecast data when listing coordinates");
      }
      else
        throw Fmi::Exception(BCP, "Station ID not set in the configuration");

      if (places.empty())
        continue;

      // We'll just use the first station instead of checking for duplicates,
      // which should never exist anyway

      auto location = *places.front();

      double lon = location.longitude;
      double lat = location.latitude;

      // To world coordinate
      double xcoord = lon;
      double ycoord = lat;
      if (!transformation.transform(xcoord, ycoord))
        continue;

      // to pixel coordinate
      theBox.transform(xcoord, ycoord);

      // Global position adjustment
      int deltax = 0;

      if (dx)
        deltax += *dx;

      int deltay = 0;
      if (dy)
        deltay += *dy;

      // Individual adjustments
      if (station.dx)
        deltax += *station.dx;

      if (station.dy)
        deltay += *station.dy;

      // Skip if not inside desired areas
      if (inside(lon, lat, forecastMode))
        points.emplace_back(Point(xcoord, ycoord, NFmiPoint(lon, lat), deltax, deltay));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given point is to be rendered
 */
// ----------------------------------------------------------------------

bool Positions::insideShapes(double theX, double theY) const
{
  try
  {
    if (outshape && Fmi::OGR::inside(*outshape, theX, theY))
      return false;

    if (inshape && !Fmi::OGR::inside(*inshape, theX, theY))
      return false;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given point is to be rendered
 */
// ----------------------------------------------------------------------

bool Positions::inside(double theX, double theY, bool forecastMode) const
{
  try
  {
    if (!insideShapes(theX, theY))
      return false;

    if (!forecastMode)
      return true;

    return intersections.inside(theX, theY);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given point is to be rendered or filtered out
 */
// ----------------------------------------------------------------------

bool Positions::inside(double theX,
                       double theY,
                       const Intersections::IntersectValues& theValues) const
{
  try
  {
    if (outshape && Fmi::OGR::inside(*outshape, theX, theY))
      return false;

    if (inshape && !Fmi::OGR::inside(*inshape, theX, theY))
      return false;

    return intersections.inside(theValues);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Positions::hash_value(const State& theState) const
{
  try
  {
    auto hash = Fmi::hash_value(static_cast<int>(layout));

    Fmi::hash_combine(hash, Fmi::hash_value(x));
    Fmi::hash_combine(hash, Fmi::hash_value(y));
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Fmi::hash_value(ddx));

    Fmi::hash_combine(hash, Fmi::hash_value(size));

    Fmi::hash_combine(hash, Fmi::hash_value(step));

    Fmi::hash_combine(hash, Fmi::hash_value(mindistance));

    Fmi::hash_combine(hash, Fmi::hash_value(keyword));

    Fmi::hash_combine(hash, Dali::hash_value(locations, theState));
    Fmi::hash_combine(hash, Dali::hash_value(stations, theState));

    Fmi::hash_combine(hash, Fmi::hash_value(directionoffset));
    Fmi::hash_combine(hash, Fmi::hash_value(rotate));
    Fmi::hash_combine(hash, Fmi::hash_value(direction));
    Fmi::hash_combine(hash, Fmi::hash_value(u));
    Fmi::hash_combine(hash, Fmi::hash_value(v));

    Fmi::hash_combine(hash, Dali::hash_value(outsidemap, theState));
    Fmi::hash_combine(hash, Dali::hash_value(insidemap, theState));
    Fmi::hash_combine(hash, Dali::hash_value(intersections, theState));

    Fmi::hash_combine(hash, Fmi::hash_value(xmargin));
    Fmi::hash_combine(hash, Fmi::hash_value(ymargin));

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
