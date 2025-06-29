// ======================================================================

#include "Projection.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>
#include <cmath>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <stdexcept>

#include <gis/OGR.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize the projection from JSON
 */
// ----------------------------------------------------------------------

void Projection::init(Json::Value& theJson, const State& theState, const Config& /* theConfig */)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Projection JSON is not a JSON object (name-value pairs)");

    // For later use
    gisengine = &theState.getGisEngine();

    // Extract all the members

    const auto& request = theState.getRequest();
    std::optional<std::string> v;

    // No longer needed here, querystring has been processed
    std::string qid;
    JsonTools::remove_string(qid, theJson, "qid");

    JsonTools::remove_string(crs, theJson, "crs");
    JsonTools::remove_string(bboxcrs, theJson, "bboxcrs");

    if (crs && *crs == "data")
    {
      JsonTools::remove_double(size, theJson, "size");

      v = request.getParameter("size");
      if (v)
        size = toDouble(*v);
    }

    if (!size || *size <= 0)
    {
      JsonTools::remove_int(xsize, theJson, "xsize");
      JsonTools::remove_int(ysize, theJson, "ysize");

      v = request.getParameter("xsize");
      if (v)
        xsize = toInt32(*v);

      v = request.getParameter("ysize");
      if (v)
        ysize = toInt32(*v);
    }

    if ((xsize && *xsize < 2) || (ysize && *ysize < 2))
      throw Fmi::Exception(BCP, "Image size must be atleast 2x2");

    JsonTools::remove_double(x1, theJson, "x1");
    JsonTools::remove_double(y1, theJson, "y1");
    JsonTools::remove_double(x2, theJson, "x2");
    JsonTools::remove_double(y2, theJson, "y2");
    JsonTools::remove_double(cx, theJson, "cx");
    JsonTools::remove_double(cy, theJson, "cy");
    JsonTools::remove_double(resolution, theJson, "resolution");

    // Note: intentionally after cx,cy parsing to enable name overrides

    auto json = JsonTools::remove(theJson, "geoid");
    if (!json.isNull())
    {
      const auto& engine = theState.getGeoEngine();
      // TODO(mheiskan): Make the language configurable
      int id = json.asInt();
      auto loc = engine.idSearch(id, "fi");
      if (!loc)
        throw Fmi::Exception(BCP,
                             "Unable to find coordinates for geoid '" + Fmi::to_string(id) + "'");
      latlon_center = true;
      cx = loc->longitude;
      cy = loc->latitude;
    }

    json = JsonTools::remove(theJson, "place");
    if (!json.isNull())
    {
      const auto& engine = theState.getGeoEngine();
      // TODO(mheiskan): Make the language configurable
      std::string name = json.asString();
      auto loc = engine.nameSearch(name, "fi");
      if (!loc)
        throw Fmi::Exception(BCP, "Unable to find coordinates for location '" + name + "'");

      latlon_center = true;
      cx = loc->longitude;
      cy = loc->latitude;
    }

    // Note: this must be handled after the CRS variables!

    json = JsonTools::remove(theJson, "bbox");
    if (!json.isNull())
    {
      std::string bbox = json.asString();
      std::vector<std::string> parts;
      boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));
      if (parts.size() != 4)
        throw Fmi::Exception(BCP, "bbox should contain 4 comma separated doubles");

      // for example in EPSG:4326 order of coordinates is changed in WMS
      // for more info: https://trac.osgeo.org/gdal/wiki/rfc20_srs_axes

      OGRSpatialReference spatref;
      if (bboxcrs)
      {
        // SetFromUserInput wants 'EPSGA' - code in order to understand axis ordering
        if (boost::algorithm::starts_with(*bboxcrs, "EPSG:"))
          boost::algorithm::replace_first(*bboxcrs, "EPSG:", "EPSGA:");

        spatref.SetFromUserInput(bboxcrs->c_str());
        spatref.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
      }
      else
      {
        // SetFromUserInput wants 'EPSGA' - code in order to understand axis ordering
        if (boost::algorithm::starts_with(*crs, "EPSG:"))
          boost::algorithm::replace_first(*crs, "EPSG:", "EPSGA:");

        spatref.SetFromUserInput(crs->c_str());
      }

      bool latLonOrder = spatref.EPSGTreatsAsLatLong() != 0;

      if (latLonOrder)
      {
        y1 = Fmi::stod(parts[0]);
        x1 = Fmi::stod(parts[1]);
        y2 = Fmi::stod(parts[2]);
        x2 = Fmi::stod(parts[3]);
      }
      else
      {
        x1 = Fmi::stod(parts[0]);
        y1 = Fmi::stod(parts[1]);
        x2 = Fmi::stod(parts[2]);
        y2 = Fmi::stod(parts[3]);
      }
    }

    if (!theJson.empty())
    {
      auto names = theJson.getMemberNames();
      auto namelist = boost::algorithm::join(names, ",");
      throw Fmi::Exception(BCP, "Unknown projection settings: " + namelist);
    }
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

std::size_t Projection::hash_value(const State& /* theState */) const
{
  try
  {
    auto hash = Fmi::hash_value(crs);
    Fmi::hash_combine(hash, Fmi::hash_value(size));
    Fmi::hash_combine(hash, Fmi::hash_value(xsize));
    Fmi::hash_combine(hash, Fmi::hash_value(ysize));
    Fmi::hash_combine(hash, Fmi::hash_value(x1));
    Fmi::hash_combine(hash, Fmi::hash_value(y1));
    Fmi::hash_combine(hash, Fmi::hash_value(x2));
    Fmi::hash_combine(hash, Fmi::hash_value(y2));
    Fmi::hash_combine(hash, Fmi::hash_value(cx));
    Fmi::hash_combine(hash, Fmi::hash_value(cy));
    Fmi::hash_combine(hash, Fmi::hash_value(resolution));
    Fmi::hash_combine(hash, Fmi::hash_value(bboxcrs));
    Fmi::hash_combine(hash, Fmi::hash_value(size));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Update area with querydata if necessary
 */
// ----------------------------------------------------------------------

void Projection::update(const Engine::Querydata::Q& theQ)
{
  if (!theQ)
    return;

  try
  {
    if (crs && *crs == "data")
    {
      crs = theQ->SpatialReference().projStr();
      bool no_bbox = (!x1 && !y1 && !x2 && !y2 && !cx && !cy && !resolution && !bboxcrs);

      if (no_bbox)
      {
        auto world1 = theQ->area().XYToWorldXY(theQ->area().BottomLeft());
        auto world2 = theQ->area().XYToWorldXY(theQ->area().TopRight());
        x1 = world1.X();
        y1 = world1.Y();
        x2 = world2.X();
        y2 = world2.Y();
      }

      if (size)
      {
        xsize = C_INT(C_DOUBLE(theQ->info()->GridXNumber()) * (*size));
        ysize = C_INT(C_DOUBLE(theQ->info()->GridYNumber()) * (*size));
      }

      ogr_crs.reset();
      box.reset();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the spatial reference
 */
// ----------------------------------------------------------------------

const Fmi::SpatialReference& Projection::getCRS() const
{
  try
  {
    prepareCRS();
    return *ogr_crs;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the spatial area
 */
// ----------------------------------------------------------------------

const Fmi::Box& Projection::getBox() const
{
  try
  {
    prepareCRS();
    return *box;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Create OGR spatial reference from the settings
 */
// ----------------------------------------------------------------------

void Projection::prepareCRS() const
{
  try
  {
    // Done already?
    if (ogr_crs)
      return;

    if (!crs)
      throw Fmi::Exception(BCP, "CRS not set, unable to create projection");

    if (!xsize && !ysize)
    {
      if (size)
        return;

      throw Fmi::Exception(BCP, "CRS xsize and ysize are both missing");
    }

    // Are subdefinitions complete?
    bool full_rect_bbox = (x1 && y1 && x2 && y2);
    bool full_center_bbox = (cx && cy && resolution);

    // Are they partial?
    bool partial_rect_bbox = !full_rect_bbox && (x1 || y1 || x2 || y2);
    bool partial_center_bbox = !full_center_bbox && (cx || cy || resolution);

    // Disallow partial definitions
    if (partial_rect_bbox)
      throw Fmi::Exception(BCP, "Partial CRS bounding box given: x1,y1,x2,y2 are needed");
    if (partial_center_bbox)
      throw Fmi::Exception(BCP, "Partial CRS bounding box given: cx,cy,resolution are needed");

    // bbox definition missing completely?
    if (!full_rect_bbox && !full_center_bbox)
      throw Fmi::Exception(BCP,
                           "CRS bounding box missing: x1,y2,x2,y2 or cx,cy,resolution are needed");

// two conflicting definitions is OK, since we create the corners from centered bbox
#if 0
        if(full_rect_bbox && full_center_bbox)
      throw Fmi::Exception(BCP,"CRS bounding box overdefined: use only x1,y2,x2,y2 or cx,cy,resolution");
#endif

    // must give both width and height if centered bbox is given
    if (!full_rect_bbox && full_center_bbox && (!xsize || !ysize))
      throw Fmi::Exception(BCP,
                           "CRS xsize and ysize are required when a centered bounding box is used");

    // Create the CRS
    ogr_crs = std::make_shared<Fmi::SpatialReference>(*crs);

    if (xsize && *xsize <= 0)
      throw Fmi::Exception(BCP, "Projection xsize must be positive");

    if (ysize && *ysize <= 0)
      throw Fmi::Exception(BCP, "Projection ysize must be positive");

    // World XY bounding box will be calculated during the process

    double XMIN = 0;
    double YMIN = 0;
    double XMAX = 0;
    double YMAX = 0;

    // Create the Contour::Area object

    if (full_rect_bbox)
    {
      // rect bounding box

      XMIN = *x1;
      YMIN = *y1;
      XMAX = *x2;
      YMAX = *y2;

      if (bboxcrs)
      {
        // Reproject corners coordinates from bboxcrs to crs
        Fmi::CoordinateTransformation transformation(*bboxcrs, *ogr_crs);
        transformation.transform(XMIN, YMIN);
        transformation.transform(XMAX, YMAX);
      }

      if (XMIN == XMAX || YMIN == YMAX)
        throw Fmi::Exception(BCP, "Bounding box size is zero!");

      if (!xsize)
      {
        // Preserve aspect by calculating xsize
        int w = boost::numeric_cast<int>((*ysize) * (XMAX - XMIN) / (YMAX - YMIN));
        box = std::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, w, *ysize);
      }
      else if (!ysize)
      {
        // Preserve aspect by calculating ysize
        int h = boost::numeric_cast<int>((*xsize) * (YMAX - YMIN) / (XMAX - XMIN));
        box = std::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, h);
      }
      else
      {
        box = std::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, *ysize);
      }

      if (ogr_crs->isGeographic() != 0)
      {
        // Substract equations 5 and 4, subsititute equation 3 and solve resolution
        double pi = boost::math::constants::pi<double>();
        double circumference = 2 * pi * 6371.220;
        resolution = (YMAX - YMIN) * circumference / (360 * (*ysize));
      }
      else
      {
        // Substract equations 1 and 2 to eliminate and solve resolution
        resolution = (XMAX - XMIN) / ((*xsize) * 1000);
      }
    }

    else
    {
      // centered bounding box
      if (!xsize || !ysize)
        throw Fmi::Exception(BCP,
                             "xsize and ysize are required when a centered bounding box is used");

      double CX = *cx;
      double CY = *cy;

      if (latlon_center || (bboxcrs && *crs != *bboxcrs))
      {
        // Reproject center coordinates from latlon/bboxcrs to crs
        Fmi::CoordinateTransformation transformation(latlon_center ? "WGS84" : bboxcrs->c_str(),
                                                     *ogr_crs);
        transformation.transform(CX, CY);
      }

      if (ogr_crs->isGeographic() != 0)
      {
        double pi = boost::math::constants::pi<double>();
        double circumference = 2 * pi * 6371.220;

        // This will work well, since we move along an isocircle
        double dy = 360 * (*ysize) / 2.0 * (*resolution) / circumference;  // Equation 3.
        YMIN = CY - dy;                                                    // Equation 4.
        YMAX = CY + dy;                                                    // Equation 5

        // Distances will become distorted the further away we are from the equator
        double dx = 360 * (*xsize) / 2.0 * (*resolution) / circumference / cos(CX * pi / 180.0);
        XMIN = CX - dx;
        XMAX = CX + dx;

        box = std::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, *ysize);
      }
      else
      {
        XMIN = CX - (*xsize) / 2.0 * (*resolution) * 1000;  // Equation 1.
        XMAX = CX + (*xsize) / 2.0 * (*resolution) * 1000;  // Equation 2.
        YMIN = CY - (*ysize) / 2.0 * (*resolution) * 1000;
        YMAX = CY + (*ysize) / 2.0 * (*resolution) * 1000;
        box = std::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, *ysize);
      }
    }

#if 0
    if (*crs == "EPSG:3857" || *crs == "epsg:3857" || *crs == "EPSGA:3857" || *crs == "epsga:3857")
    {
      // EPSG:3857 (WebMercator) bbox according to https://epsg.io/3857 uses value 20037508.34
      // but PROJ produces a slightly larger value, hence we add 0.01
      auto epsg_max = 20037508.35;
      auto epsg_min = -epsg_max;
      auto lon_0 = 0;  // default central longitude

      // No support for multiple wraps atleast yet
      if (XMIN < epsg_min)
        lon_0 = -180;
      else if (XMAX > epsg_max)
        lon_0 = +180;

      if (lon_0 != 0)
      {
        auto new_crs = fmt::format("+init={} +lon_0={}", *crs, lon_0);
        ogr_crs = std::make_shared<Fmi::SpatialReference>(new_crs);

        std::cout << "PROJ = " << ogr_crs->projStr() << std::endl;
        std::cout << "WKT = " << ogr_crs->WKT() << std::endl;
      }
    }
#endif

    // WGS84 latlon corners calculated from world xy coordinates

    Fmi::CoordinateTransformation transformation(*ogr_crs, "WGS84");

    // Calculate bottom left and top right coordinates

    transformation.transform(XMIN, YMIN);
    itsBottomLeft = NFmiPoint(XMIN, YMIN);

    transformation.transform(XMAX, YMAX);
    itsTopRight = NFmiPoint(XMAX, YMAX);

#if 0
    std::cerr << fmt::format(
                     "\nProjection:\n\n\nxsize={}\nysize={}\nx1={}\ny1={}\nx2={}\ny2={}\ncx={}\ncy="
                     "{}\nres={}\nbl={},{}\ntr={},{}\nxmin={}\nxmax={}\nymin={}\nymax={}\nwidth={}"
                     "\nheight={}\n\n",
                     xsize ? *xsize : -1,
                     ysize ? *ysize : -1,
                     x1 ? *x1 : -1,
                     y1 ? *y1 : -1,
                     x2 ? *x2 : -1,
                     y2 ? *y2 : -1,
                     cx ? *cx : -1,
                     cy ? *cy : -1,
                     resolution ? *resolution : -1,
                     itsBottomLeft.X(),
                     itsBottomLeft.Y(),
                     itsTopRight.X(),
                     itsTopRight.Y(),
                     box->xmin(),
                     box->xmax(),
                     box->ymin(),
                     box->ymax(),
                     box->width(),
                     box->height())
              << std::flush;
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
