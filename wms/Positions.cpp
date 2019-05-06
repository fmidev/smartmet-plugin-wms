#include "Positions.h"
#include "Config.h"
#include "Hash.h"
#include "Projection.h"
#include <boost/move/make_unique.hpp>
#include <engines/querydata/ParameterOptions.h>
#include <gis/OGR.h>
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/ParameterFactory.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
                             const boost::posix_time::ptime& theTime,
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
      auto param = Spine::ParameterFactory::instance().parse(theDirection);
      if (!q.param(param.number()))
        throw Spine::Exception(
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
      auto uparam = Spine::ParameterFactory::instance().parse(theU);
      auto vparam = Spine::ParameterFactory::instance().parse(theV);

      // Q API SUCKS
      boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
      boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
      boost::local_time::local_date_time localdatetime(theTime, utc);
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void Positions::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Positions JSON is not a JSON object!");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

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
          throw Spine::Exception(BCP, "Unknown layout type for positions: '" + tmp + "'!");
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
        throw Spine::Exception(BCP, "Positions does not have a setting named '" + name + "'!");
    }

    if (size <= 0)
      throw Spine::Exception(BCP, "Positions size-setting must be nonnegative!");

    if (step <= 0)
      throw Spine::Exception(BCP, "Positions step-setting must be nonnegative!");

    if (step > size)
      throw Spine::Exception(BCP, "Positions step-setting must be smaller than the size-setting!");

    if (180 % step != 0)
      throw Spine::Exception(BCP, "Positions step-size must divide 180 degrees evenly");

    // we cannot allow stuff like 1e-6, that'll generate a huge amounts of points
    if (mindistance < 2)
      throw Spine::Exception(BCP, "Minimum distance between positions must be at least 2 pixels!");

    if (!direction.empty() && (!u.empty() || !v.empty()))
      throw Spine::Exception(
          BCP, "Cannot specify position offsets using both direction and the u- and v-components!");

    if ((!u.empty() && v.empty()) || (!v.empty() && u.empty()))
      throw Spine::Exception(
          BCP, "Cannot specify only one of u- and v-components for position offsets!");

    if (directionoffset != 0 && direction.empty() && u.empty() && v.empty())
      throw Spine::Exception(BCP,
                             "Must specify direction parameter for direction offset of positions!");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the geometries to be used for intersections
 */
// ----------------------------------------------------------------------

void Positions::init(const boost::optional<std::string>& theProducer,
                     const Projection& theProjection,
                     const boost::posix_time::ptime& theTime,
                     const State& theState)
{
  try
  {
    // For applying directional corrections to positions
    time = theTime;

    // GeoEngine might be needed
    geonames = &theState.getGeoEngine();

    // Initialize clipping shapes

    const auto& gis = theState.getGisEngine();

    if (insidemap)
    {
      inshape = gis.getShape(theProjection.getCRS().get(), insidemap->options);
      if (!inshape)
        throw Spine::Exception(BCP, "Positions received empty inside-shape from database!");

      // This does not obey layer margins, hence we disable this speed optimization
      // inshape.reset(Fmi::OGR::polyclip(*inshape, theProjection.getBox()));
    }

    if (outsidemap)
    {
      outshape = gis.getShape(theProjection.getCRS().get(), outsidemap->options);

      // This does not obey layer margings, hence we disable this speed optimization
      // if (outshape)
      // outshape.reset(Fmi::OGR::polyclip(*outshape, theProjection.getBox()));
    }

    intersections.init(theProducer, theProjection, theTime, theState);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
                                       const boost::shared_ptr<OGRSpatialReference>& theCRS,
                                       const Fmi::Box& theBox,
                                       bool forecastMode) const
{
  try
  {
    switch (layout)
    {
      case Layout::Grid:
        return getGridPoints(theQ, theCRS, theBox, forecastMode);
      case Layout::Data:
        return getDataPoints(theQ, theCRS, theBox, forecastMode);
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
    return Points();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a grid of locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGridPoints(const Engine::Querydata::Q& theQ,
                                           const boost::shared_ptr<OGRSpatialReference>& theCRS,
                                           const Fmi::Box& theBox,
                                           bool forecastMode) const
{
  try
  {
    // Get the data projection

    auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err = wgs84->SetFromUserInput("WGS84");
    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP, "GDAL does not understand WGS84!");

    // Create the coordinate transformation from image world coordinates to latlon

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(theCRS.get(), wgs84.get()));
    if (transformation == nullptr)
      throw Spine::Exception(
          BCP, "Failed to create the needed coordinate transformation for position generation!");

    // Use defaults for this layout if nothing is specified

    if (!!dx && *dx <= 0)
      throw Spine::Exception(BCP, "Positions dx-setting must be nonnegative for grid-layouts!");

    if (!!dy && *dy <= 0)
      throw Spine::Exception(BCP, "Positions dy-setting must be nonnegative for grid-layouts!");

    int xstart = (!!x ? *x : 5);
    int ystart = (!!y ? *y : 5);
    int deltax = (!!dx ? *dx : 20);
    int deltay = (!!dy ? *dy : 20);
    int deltaxx = (!!ddx ? *ddx : 0);

    // Sanity check
    if (deltaxx >= deltax)
      throw Spine::Exception(BCP, "ddx must be smaller than dx when generating a grid layout!");

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

    for (int ypos = ystart; ypos < height + ymargin; ypos += deltay)
    {
      // Stagger every other row
      if (row++ % 2 == 0)
        xstart += deltaxx;
      else
        xstart -= deltaxx;

      for (int xpos = xstart; xpos < width + xmargin; xpos += deltax)
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

        if (transformation->Transform(1, &xcoord, &ycoord) == 0)
          continue;

        points.emplace_back(Point(xpos, ypos, NFmiPoint(xcoord, ycoord)));
      }
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate querydata locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getDataPoints(const Engine::Querydata::Q& theQ,
                                           const boost::shared_ptr<OGRSpatialReference>& theCRS,
                                           const Fmi::Box& theBox,
                                           bool forecastMode) const
{
  // Cannot generate any points without any querydata. If layout=data,
  // the layers will handle stations and flashes by themselves.

  if (!theQ)
    return {};

  try
  {
    // Get the data projection

    auto qcrs = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err;
    if (theQ->isArea())
      err = qcrs->SetFromUserInput(theQ->area().ProjStr().c_str());
    else
      err = qcrs->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP,
                             "GDAL does not understand this projection: " + theQ->area().ProjStr());

    // Create the coordinate transformation from image world coordinates
    // to querydata world coordinates

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(theCRS.get(), qcrs.get()));
    if (transformation == nullptr)
      throw Spine::Exception(
          BCP, "Failed to create the needed coordinate transformation for generating positions!");

    // Generate the grid coordinates

    Points points;

    auto shared_latlons = theQ->latLonCache();

    for (const auto& latlon : *shared_latlons)
    {
      // Convert latlon to world coordinate

      NFmiPoint xy;
      if (theCRS->IsGeographic() != 0)
        xy = latlon;
      else
        xy = theQ->area().LatLonToWorldXY(latlon);

      // World coordinate to pixel coordinate
      double xcoord = xy.X();
      double ycoord = xy.Y();
      theBox.transform(xcoord, ycoord);

      int deltax = 0;
      if (dx)
        deltax += *dx;
      int deltay = 0;
      if (dy)
        deltay += *dy;

      // Skip if not inside desired shapes
      if (inside(latlon.X(), latlon.Y(), forecastMode))
        points.emplace_back(Point(xcoord, ycoord, latlon, deltax, deltay));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGraticulePoints(
    const Engine::Querydata::Q& theQ,
    const boost::shared_ptr<OGRSpatialReference>& theCRS,
    const Fmi::Box& theBox,
    bool forecastMode) const
{
  try
  {
    // Set the graticule projection

    auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP,
                             "GDAL does not understand this projection: " + theQ->area().ProjStr());

    // Create the coordinate transformation from WGS84 to projection coordinates

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (transformation == nullptr)
      throw Spine::Exception(BCP,
                             "Failed to create the needed coordinate transformation for "
                             "generating graticule positions");

    // Generate the graticule coordinates.

    Points points;

    // This loop order makes it easier to handle the poles only once
    for (int lat = -90; lat <= 90; lat += step)
      for (int lon = -180; lon < 180; lon += step)
      {
        if (lon % size == 0 || lat % size == 0)
        {
          // latlon to world coordinate
          double xcoord = lon;
          double ycoord = lat;
          if (transformation->Transform(1, &xcoord, &ycoord) == 0)
            continue;

          // to pixel coordinate
          theBox.transform(xcoord, ycoord);

          int deltax = 0;
          if (dx)
            deltax += *dx;

          int deltay = 0;
          if (dy)
            deltay += *dy;

          // Skip if not inside desired areas
          if (inside(lon, lat, forecastMode))
            points.emplace_back(Point(xcoord, ycoord, NFmiPoint(lon, lat), deltax, deltay));
        }
        // Handle the poles only once
        if (lat == -90 || lat == 90)
          break;
      }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule fill locations
 *
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGraticuleFillPoints(
    const Engine::Querydata::Q& theQ,
    const boost::shared_ptr<OGRSpatialReference>& theCRS,
    const Fmi::Box& theBox,
    bool forecastMode) const
{
  try
  {
    // Set the graticule projection

    auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP,
                             "GDAL does not understand this projection: " + theQ->area().ProjStr());

    // Create the coordinate transformation from WGS84 to projection coordinates

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (transformation == nullptr)
      throw Spine::Exception(BCP,
                             "Failed to create the needed coordinate transformation for "
                             "generating graticule positions!");

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
        if (transformation->Transform(1, &x1, &y1) == 0)
          continue;
        if (transformation->Transform(1, &x2, &y2) == 0)
          continue;
        if (transformation->Transform(1, &x3, &y3) == 0)
          continue;
        if (transformation->Transform(1, &x4, &y4) == 0)
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

          if (transformation->Transform(1, &newx, &newy) == 0)
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
            points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));
        }

        // Bottom edge
        for (int i = 1; i < nbottom; i++)
        {
          double newlon = lon + 1.0 * i / nbottom * size;
          double newlat = lat;
          double newx = newlon;
          double newy = newlat;

          if (transformation->Transform(1, &newx, &newy) == 0)
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
            points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));

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

            if (transformation->Transform(1, &newx, &newy) == 0)
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
              points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));
          }
      }

    // All that remains is the north pole
    double newlon = 0;
    double newlat = 90;
    double newx = newlon;
    double newy = newlat;

    if (transformation->Transform(1, &newx, &newy) == 0)
    {
      int deltax = 0;
      if (dx)
        deltax += *dx;
      int deltay = 0;
      if (dy)
        deltay += *dy;

      // Skip if not inside desired areas
      if (inside(newlon, newlat, forecastMode))
        points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat), deltax, deltay));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate keyword locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getKeywordPoints(const Engine::Querydata::Q& theQ,
                                              const boost::shared_ptr<OGRSpatialReference>& theCRS,
                                              const Fmi::Box& theBox,
                                              bool forecastMode) const
{
  try
  {
    // Read the keyword

    if (keyword.empty())
      throw Spine::Exception(
          BCP, "No keyword given when trying to use a keyword for location definitions!");

    Locus::QueryOptions options;
    auto locations = geonames->keywordSearch(options, keyword);

    // Keyword locations are in WGS84

    auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP,
                             "GDAL does not understand this projection: " + theQ->area().ProjStr());

    // Create the coordinate transformation from WGS84 to projection coordinates

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (transformation == nullptr)
      throw Spine::Exception(BCP,
                             "Failed to create the needed coordinate transformation for "
                             "generating keyword positions!");

    Points points;

    // This loop order makes it easier to handle the poles only once
    for (const auto& location : locations)
    {
      // keyword location latlon
      double lon = location->longitude;
      double lat = location->latitude;

      // To world coordinate
      double xcoord = lon;
      double ycoord = lat;
      if (transformation->Transform(1, &xcoord, &ycoord) == 0)
        continue;

      // to pixel coordinate
      theBox.transform(xcoord, ycoord);

      int deltax = 0;
      if (dx)
        deltax += *dx;
      int deltay = 0;
      if (dy)
        deltay += *dy;

      // Skip if not inside desired areas
      if (inside(lon, lat, forecastMode))
        points.emplace_back(Point(xcoord, ycoord, NFmiPoint(lon, lat), deltax, deltay));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate explicit locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getLatLonPoints(const Engine::Querydata::Q& theQ,
                                             const boost::shared_ptr<OGRSpatialReference>& theCRS,
                                             const Fmi::Box& theBox,
                                             bool forecastMode) const
{
  try
  {
    Points points;

    if (locations.locations.empty())
      return points;

    // Keyword locations are in WGS84

    auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP,
                             "GDAL does not understand this projection: " + theQ->area().ProjStr());

    // Create the coordinate transformation from WGS84 to projection coordinates

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (transformation == nullptr)
      throw Spine::Exception(BCP,
                             "Failed to create the needed coordinate transformation for "
                             "generating latlon positions!");

    // This loop order makes it easier to handle the poles only once
    for (const auto& location : locations.locations)
    {
      // keyword location latlon
      if (!location.longitude || !location.latitude)
        throw Spine::Exception(BCP, "Incomplete location in the locations list!");

      double lon = *location.longitude;
      double lat = *location.latitude;

      // To world coordinate
      double xcoord = lon;
      double ycoord = lat;
      if (transformation->Transform(1, &xcoord, &ycoord) == 0)
        continue;

      // to pixel coordinate
      theBox.transform(xcoord, ycoord);

      // Global position adjustment
      int deltax = 0;
      if (dx)
        deltax += *dx;

      int deltay = 0;
      if (dy)
        deltay = *dy;

      // Individual adjustments
      if (location.dx)
        deltax += *location.dx;

      if (location.dy)
        deltay += *location.dy;

      // Skip if not inside desired areas
      if (inside(lon, lat, forecastMode))
        points.emplace_back(Point(xcoord, ycoord, NFmiPoint(lon, lat), deltax, deltay));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate station locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getStationPoints(const Engine::Querydata::Q& theQ,
                                              const boost::shared_ptr<OGRSpatialReference>& theCRS,
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

    // Station coordinates are in WGS84

    auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP,
                             "GDAL does not understand this projection: " + theQ->area().ProjStr());

    // Create the coordinate transformation from WGS84 to projection coordinates

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (transformation == nullptr)
      throw Spine::Exception(BCP,
                             "Failed to create the needed coordinate transformation for "
                             "generating station positions!");

    Locus::QueryOptions options;
    auto locations = geonames->keywordSearch(options, keyword);

    // This loop order makes it easier to handle the poles only once
    for (const auto& station : stations.stations)
    {
      Locus::QueryOptions options;
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
        throw Spine::Exception(
            BCP, "Use latlon instead of station layout for forecast data when listing coordinates");
      }
      else
        throw Spine::Exception(BCP, "Station ID not set in the configuration");

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
      if (transformation->Transform(1, &xcoord, &ycoord) == 0)
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    auto hash = Dali::hash_value(static_cast<int>(layout));

    Dali::hash_combine(hash, Dali::hash_value(x));
    Dali::hash_combine(hash, Dali::hash_value(y));
    Dali::hash_combine(hash, Dali::hash_value(dx));
    Dali::hash_combine(hash, Dali::hash_value(dy));
    Dali::hash_combine(hash, Dali::hash_value(ddx));

    Dali::hash_combine(hash, Dali::hash_value(size));

    Dali::hash_combine(hash, Dali::hash_value(step));

    Dali::hash_combine(hash, Dali::hash_value(mindistance));

    Dali::hash_combine(hash, Dali::hash_value(keyword));

    Dali::hash_combine(hash, Dali::hash_value(locations, theState));
    Dali::hash_combine(hash, Dali::hash_value(stations, theState));

    Dali::hash_combine(hash, Dali::hash_value(directionoffset));
    Dali::hash_combine(hash, Dali::hash_value(rotate));
    Dali::hash_combine(hash, Dali::hash_value(direction));
    Dali::hash_combine(hash, Dali::hash_value(u));
    Dali::hash_combine(hash, Dali::hash_value(v));

    Dali::hash_combine(hash, Dali::hash_value(outsidemap, theState));
    Dali::hash_combine(hash, Dali::hash_value(insidemap, theState));
    Dali::hash_combine(hash, Dali::hash_value(intersections, theState));

    Dali::hash_combine(hash, Dali::hash_value(xmargin));
    Dali::hash_combine(hash, Dali::hash_value(ymargin));

    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
