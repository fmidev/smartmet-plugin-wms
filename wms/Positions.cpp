#include "Positions.h"
#include "Config.h"
#include "Hash.h"
#include "Projection.h"
#include <spine/ParameterFactory.h>
#include <spine/Exception.h>
#include <gis/OGR.h>
#include <boost/foreach.hpp>
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
  try
  {
    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
                             SmartMet::Engine::Querydata::Q theQ,
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
      auto param = SmartMet::Spine::ParameterFactory::instance().parse(theDirection);
      if (!q.param(param.number()))
        throw SmartMet::Spine::Exception(
            BCP, "Parameter '" + theDirection + "' is not available for position selection!");

      for (auto& point : thePoints)
      {
        double dir = q.interpolate(point.latlon, theTime, 180);  // TODO: magic constant
        if (dir != kFloatMissing)
        {
          point.x += theOffset * cos((dir + 90 + theRotation) * pi / 180);
          point.y += theOffset * sin((dir + 90 + theRotation) * pi / 180);
        }
      }
    }
    else
    {
      auto uparam = SmartMet::Spine::ParameterFactory::instance().parse(theU);
      if (!q.param(uparam.number()))
        throw SmartMet::Spine::Exception(
            BCP, "Parameter '" + theU + "' is not available for position selection!");

      auto vparam = SmartMet::Spine::ParameterFactory::instance().parse(theV);
      if (!q.param(vparam.number()))
        throw SmartMet::Spine::Exception(
            BCP, "Parameter '" + theV + "' is not available for position selection!");

      for (auto& point : thePoints)
      {
        q.param(uparam.number());
        double uspd = q.interpolate(point.latlon, theTime, 180);  // TODO: magic constant
        q.param(vparam.number());
        double vspd = q.interpolate(point.latlon, theTime, 180);  // TODO: magic constant
        if (uspd != kFloatMissing && vspd != kFloatMissing && (uspd != 0 || vspd != 0))
        {
          double dir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
          point.x += theOffset * cos((dir + 90 + theRotation) * pi / 180);
          point.y += theOffset * sin((dir + 90 + theRotation) * pi / 180);
        }
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
      throw SmartMet::Spine::Exception(BCP, "Positions JSON is not a JSON object!");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
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
        else
          throw SmartMet::Spine::Exception(BCP,
                                           "Unknown layout type for positions: '" + tmp + "'!");
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
        throw SmartMet::Spine::Exception(BCP,
                                         "Positions does not have a setting named '" + name + "'!");
    }

    if (size <= 0)
      throw SmartMet::Spine::Exception(BCP, "Positions size-setting must be nonnegative!");

    if (step <= 0)
      throw SmartMet::Spine::Exception(BCP, "Positions step-setting must be nonnegative!");

    if (step > size)
      throw SmartMet::Spine::Exception(
          BCP, "Positions step-setting must be smaller than the size-setting!");

    if (180 % step != 0)
      throw SmartMet::Spine::Exception(BCP, "Positions step-size must divide 180 degrees evenly");

    // we cannot allow stuff like 1e-6, that'll generate a huge amounts of points
    if (mindistance < 2)
      throw SmartMet::Spine::Exception(
          BCP, "Minimum distance between positions must be at least 2 pixels!");

    if (!direction.empty() && (!u.empty() || !v.empty()))
      throw SmartMet::Spine::Exception(
          BCP, "Cannot specify position offsets using both direction and the u- and v-components!");

    if ((!u.empty() && v.empty()) || (!v.empty() && u.empty()))
      throw SmartMet::Spine::Exception(
          BCP, "Cannot specify only one of u- and v-components for position offsets!");

    if (directionoffset != 0 && direction.empty() && u.empty() && v.empty())
      throw SmartMet::Spine::Exception(
          BCP, "Must specify direction parameter for direction offset of positions!");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the geometries to be used for intersections
 */
// ----------------------------------------------------------------------

void Positions::init(SmartMet::Engine::Querydata::Q q,
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
        throw SmartMet::Spine::Exception(BCP,
                                         "Positions received empty inside-shape from database!");

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

    if (q)
      intersections.init(q, theProjection, theTime, theState);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the locations to be rendered
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getPoints(SmartMet::Engine::Querydata::Q theQ,
                                       boost::shared_ptr<OGRSpatialReference> theCRS,
                                       const Fmi::Box& theBox) const
{
  try
  {
    switch (layout)
    {
      case Layout::Grid:
        return getGridPoints(theQ, theCRS, theBox);
      case Layout::Data:
        return getDataPoints(theQ, theCRS, theBox);
      case Layout::Graticule:
        return getGraticulePoints(theQ, theCRS, theBox);
      case Layout::GraticuleFill:
        return getGraticuleFillPoints(theQ, theCRS, theBox);
      case Layout::Keyword:
        return getKeywordPoints(theQ, theCRS, theBox);
      case Layout::LatLon:
        return getLatLonPoints(theQ, theCRS, theBox);
    }
    // Dummy to prevent g++ from complaining
    return Points();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a grid of locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGridPoints(SmartMet::Engine::Querydata::Q theQ,
                                           boost::shared_ptr<OGRSpatialReference> theCRS,
                                           const Fmi::Box& theBox) const
{
  try
  {
    // Get the data projection

    std::unique_ptr<OGRSpatialReference> wgs84(new OGRSpatialReference);
    OGRErr err = wgs84->SetFromUserInput("WGS84");
    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP, "GDAL does not understand WGS84!");

    // Create the coordinate transformation from image world coordinates
    // to lat lon

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(theCRS.get(), wgs84.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(
          BCP, "Failed to create the needed coordinate transformation for position generation!");

    // Use defaults for this layout if nothing is specified

    if (!!dx && *dx <= 0)
      throw SmartMet::Spine::Exception(
          BCP, "Positions dx-setting must be nonnegative for grid-layouts!");

    if (!!dy && *dy <= 0)
      throw SmartMet::Spine::Exception(
          BCP, "Positions dy-setting must be nonnegative for grid-layouts!");

    int xstart = (!!x ? *x : 5);
    int ystart = (!!y ? *y : 5);
    int deltax = (!!dx ? *dx : 20);
    int deltay = (!!dy ? *dy : 20);
    int deltaxx = (!!ddx ? *ddx : 0);

    // Sanity check
    if (deltaxx >= deltax)
      throw SmartMet::Spine::Exception(
          BCP, "ddx must be smaller than dx when generating a grid layout!");

    // Generate the grid coordinates

    Points points;

    // // this initial stagger will be cancelled in the first iteration
    xstart += deltaxx;

    // TODO: Must alter these if there is a margin
    const int height = theBox.height();
    const int width = theBox.width();

    for (int ypos = ystart; ypos < height; ypos += deltay)
    {
      // Stagger left when possible, the fix back right
      if (xstart < deltaxx)
        xstart += deltax;
      xstart -= deltaxx;

      for (int xpos = xstart; xpos < width; xpos += deltax)
      {
        // Convert pixel coordinate to world coordinate (or latlon for geographic spatial
        // references)
        double xcoord = static_cast<double>(xpos);
        double ycoord = static_cast<double>(ypos);

        theBox.itransform(xcoord, ycoord);

        // Skip if the coordinate is outside the desired shapes

        if (!insideShapes(xcoord, ycoord))
          continue;

        // Convert world coordinate to latlon, skipping points which cannot be handled

        if (!transformation->Transform(1, &xcoord, &ycoord))
          continue;

        points.emplace_back(Point(xpos, ypos, NFmiPoint(xcoord, ycoord)));
      }
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate querydata locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getDataPoints(SmartMet::Engine::Querydata::Q theQ,
                                           boost::shared_ptr<OGRSpatialReference> theCRS,
                                           const Fmi::Box& theBox) const
{
  // Cannot generate any points without any querydata. If layout=data,
  // the layers will handle stations and flashes by themselves.

  if (!theQ)
    return {};

  try
  {
    // Get the data projection

    std::unique_ptr<OGRSpatialReference> qcrs(new OGRSpatialReference);
    OGRErr err;
    if (theQ->isArea())
      err = qcrs->SetFromUserInput(theQ->area().WKT().c_str());
    else
      err = qcrs->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(
          BCP, "GDAL does not understand this FMI WKT: " + theQ->area().WKT());

    // Create the coordinate transformation from image world coordinates
    // to querydata world coordinates

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(theCRS.get(), qcrs.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(
          BCP, "Failed to create the needed coordinate transformation for generating positions!");

    // Generate the grid coordinates

    Points points;

    auto shared_latlons = theQ->latLonCache();

    for (const auto& latlon : *shared_latlons)
    {
      // Convert latlon to world coordinate

      NFmiPoint xy;
      if (theCRS->IsGeographic())
        xy = latlon;
      else
        xy = theQ->area().LatLonToWorldXY(latlon);

      // World coordinate to pixel coordinate
      double xcoord = xy.X();
      double ycoord = xy.Y();
      theBox.transform(xcoord, ycoord);

      if (dx)
        xcoord += *dx;
      if (dy)
        ycoord += *dy;

      // Skip if not inside desired shapes
      if (insideShapes(latlon.X(), latlon.Y()))
        points.emplace_back(Point(xcoord, ycoord, latlon));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGraticulePoints(SmartMet::Engine::Querydata::Q theQ,
                                                boost::shared_ptr<OGRSpatialReference> theCRS,
                                                const Fmi::Box& theBox) const
{
  try
  {
    // Set the graticule projection

    std::unique_ptr<OGRSpatialReference> wgs84(new OGRSpatialReference);
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP,
                                       "GDAL does not understand this WKT: " + theQ->area().WKT());

    // Create the coordinate transformation from WGS84 to projection coordinates

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(BCP,
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
          if (!transformation->Transform(1, &xcoord, &ycoord))
            continue;

          // to pixel coordinate
          theBox.transform(xcoord, ycoord);

          if (dx)
            xcoord += *dx;
          if (dy)
            ycoord += *dy;

          // Skip if not inside desired shapes
          if (insideShapes(lon, lat))
            points.emplace_back(Point(xcoord, ycoord, NFmiPoint(lon, lat)));
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate graticule fill locations
 *
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getGraticuleFillPoints(SmartMet::Engine::Querydata::Q theQ,
                                                    boost::shared_ptr<OGRSpatialReference> theCRS,
                                                    const Fmi::Box& theBox) const
{
  try
  {
    // Set the graticule projection

    std::unique_ptr<OGRSpatialReference> wgs84(new OGRSpatialReference);
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP,
                                       "GDAL does not understand this WKT: " + theQ->area().WKT());

    // Create the coordinate transformation from WGS84 to projection coordinates

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(BCP,
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
        if (!transformation->Transform(1, &x1, &y1))
          continue;
        if (!transformation->Transform(1, &x2, &y2))
          continue;
        if (!transformation->Transform(1, &x3, &y3))
          continue;
        if (!transformation->Transform(1, &x4, &y4))
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

          if (!transformation->Transform(1, &newx, &newy))
            continue;
          theBox.transform(newx, newy);

          if (dx)
            newx += *dx;
          if (dy)
            newy += *dy;

          // Skip if not inside desired area
          if (insideShapes(newlon, newlat))
            points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat)));
        }

        // Bottom edge
        for (int i = 1; i < nbottom; i++)
        {
          double newlon = lon + 1.0 * i / nbottom * size;
          double newlat = lat;
          double newx = newlon;
          double newy = newlat;

          if (!transformation->Transform(1, &newx, &newy))
            continue;
          theBox.transform(newx, newy);

          if (dx)
            newx += *dx;
          if (dy)
            newy += *dy;

          // Skip if not inside desired shapes
          if (insideShapes(newlon, newlat))
            points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat)));

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

            if (!transformation->Transform(1, &newx, &newy))
              continue;
            theBox.transform(newx, newy);

            if (dx)
              newx += *dx;
            if (dy)
              newy += *dy;

            // Skip if not inside desired area
            if (insideShapes(newlon, newlat))
              points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat)));
          }
      }

    // All that remains is the north pole
    double newlon = 0;
    double newlat = 90;
    double newx = newlon;
    double newy = newlat;

    if (!transformation->Transform(1, &newx, &newy))
    {
      if (dx)
        newx += *dx;
      if (dy)
        newy += *dy;

      // Skip if not inside desired area
      if (insideShapes(newlon, newlat))
        points.emplace_back(Point(newx, newy, NFmiPoint(newlon, newlat)));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate keyword locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getKeywordPoints(SmartMet::Engine::Querydata::Q theQ,
                                              boost::shared_ptr<OGRSpatialReference> theCRS,
                                              const Fmi::Box& theBox) const
{
  try
  {
    // Read the keyword

    if (keyword.empty())
      throw SmartMet::Spine::Exception(
          BCP, "No keyword given when trying to use a keyword for location definitions!");

    Locus::QueryOptions options;
    auto locations = geonames->keywordSearch(options, keyword);

    // Keyword locations are in WGS84

    std::unique_ptr<OGRSpatialReference> wgs84(new OGRSpatialReference);
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP,
                                       "GDAL does not understand this WKT: " + theQ->area().WKT());

    // Create the coordinate transformation from WGS84 to projection coordinates

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(BCP,
                                       "Failed to create the needed coordinate transformation for "
                                       "generating graticule positions!");

    // Generate the graticule coordinates.

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
      if (!transformation->Transform(1, &xcoord, &ycoord))
        continue;

      // to pixel coordinate
      theBox.transform(xcoord, ycoord);

      if (dx)
        xcoord += *dx;
      if (dy)
        ycoord += *dy;

      // Skip if not inside desired shapes
      if (insideShapes(lon, lat))
        points.emplace_back(Point(xcoord, ycoord, NFmiPoint(lon, lat)));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate explicit locations
 */
// ----------------------------------------------------------------------

Positions::Points Positions::getLatLonPoints(SmartMet::Engine::Querydata::Q theQ,
                                             boost::shared_ptr<OGRSpatialReference> theCRS,
                                             const Fmi::Box& theBox) const
{
  try
  {
    Points points;

    if (locations.locations.empty())
      return points;

    // Keyword locations are in WGS84

    std::unique_ptr<OGRSpatialReference> wgs84(new OGRSpatialReference);
    OGRErr err = wgs84->SetFromUserInput("WGS84");

    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP,
                                       "GDAL does not understand this WKT: " + theQ->area().WKT());

    // Create the coordinate transformation from WGS84 to projection coordinates

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), theCRS.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(BCP,
                                       "Failed to create the needed coordinate transformation for "
                                       "generating graticule positions!");

    // Generate the graticule coordinates.

    // This loop order makes it easier to handle the poles only once
    for (const auto& location : locations.locations)
    {
      // keyword location latlon
      if (!location.longitude || !location.latitude)
        throw SmartMet::Spine::Exception(BCP, "Incomplete location in the locations list!");

      double lon = *location.longitude;
      double lat = *location.latitude;

      // To world coordinate
      double xcoord = lon;
      double ycoord = lat;
      if (!transformation->Transform(1, &xcoord, &ycoord))
        continue;

      // to pixel coordinate
      theBox.transform(xcoord, ycoord);

      if (dx)
        xcoord += *dx;
      if (dy)
        ycoord += *dy;

      // Skip if not inside desired area
      if (insideShapes(lon, lat))
        points.emplace_back(Point(xcoord, ycoord, NFmiPoint(lon, lat)));
    }

    apply_direction_offsets(points, theQ, time, directionoffset, rotate, direction, u, v);

    return points;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given point is to be rendered
 */
// ----------------------------------------------------------------------

bool Positions::inside(double theX, double theY) const
{
  try
  {
    return insideShapes(theX, theY) && intersections.inside(theX, theY);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

    boost::hash_combine(hash, Dali::hash_value(x));
    boost::hash_combine(hash, Dali::hash_value(y));
    boost::hash_combine(hash, Dali::hash_value(dx));
    boost::hash_combine(hash, Dali::hash_value(dy));
    boost::hash_combine(hash, Dali::hash_value(ddx));

    boost::hash_combine(hash, Dali::hash_value(size));

    boost::hash_combine(hash, Dali::hash_value(step));

    boost::hash_combine(hash, Dali::hash_value(mindistance));

    boost::hash_combine(hash, Dali::hash_value(keyword));

    boost::hash_combine(hash, Dali::hash_value(locations, theState));

    boost::hash_combine(hash, Dali::hash_value(directionoffset));
    boost::hash_combine(hash, Dali::hash_value(rotate));
    boost::hash_combine(hash, Dali::hash_value(direction));
    boost::hash_combine(hash, Dali::hash_value(u));
    boost::hash_combine(hash, Dali::hash_value(v));

    boost::hash_combine(hash, Dali::hash_value(outsidemap, theState));
    boost::hash_combine(hash, Dali::hash_value(insidemap, theState));
    boost::hash_combine(hash, Dali::hash_value(intersections, theState));
    return hash;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
