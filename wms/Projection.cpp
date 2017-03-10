// ======================================================================

#include "Projection.h"
#include "Config.h"
#include "Hash.h"
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <gis/Box.h>
#include <gdal/ogr_geometry.h>
#include <gdal/ogr_spatialref.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <cmath>
#include <stdexcept>

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

void Projection::init(const Json::Value& theJson, const State& theState, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Projection JSON is not a JSON object (name-value pairs)");

    // Extract all the members

    Json::Value nulljson;

    auto json = theJson.get("crs", nulljson);
    if (!json.isNull())
      crs = json.asString();

    json = theJson.get("bboxcrs", nulljson);
    if (!json.isNull())
      bboxcrs = json.asString();

    json = theJson.get("xsize", nulljson);
    if (!json.isNull())
      xsize = json.asInt();

    json = theJson.get("ysize", nulljson);
    if (!json.isNull())
      ysize = json.asInt();

    json = theJson.get("x1", nulljson);
    if (!json.isNull())
      x1 = json.asDouble();

    json = theJson.get("y1", nulljson);
    if (!json.isNull())
      y1 = json.asDouble();

    json = theJson.get("x2", nulljson);
    if (!json.isNull())
      x2 = json.asDouble();

    json = theJson.get("y2", nulljson);
    if (!json.isNull())
      y2 = json.asDouble();

    json = theJson.get("cx", nulljson);
    if (!json.isNull())
      cx = json.asDouble();

    json = theJson.get("cy", nulljson);
    if (!json.isNull())
      cy = json.asDouble();

    json = theJson.get("resolution", nulljson);
    if (!json.isNull())
      resolution = json.asDouble();

    // Note: intentionally after cx,cy parsing to enable name overrides

    json = theJson.get("geoid", nulljson);
    if (!json.isNull())
    {
      const auto& engine = theState.getGeoEngine();
      // TODO: Make the language configurable
      int id = json.asInt();
      auto loc = engine.idSearch(id, "fi");
      if (!loc)
        throw Spine::Exception(BCP,
                               "Unable to find coordinates for geoid '" + Fmi::to_string(id) + "'");
      latlon_center = true;
      cx = loc->longitude;
      cy = loc->latitude;
    }

    json = theJson.get("place", nulljson);
    if (!json.isNull())
    {
      const auto& engine = theState.getGeoEngine();
      // TODO: Make the language configurable
      std::string name = json.asString();
      auto loc = engine.nameSearch(name, "fi");
      if (!loc)
        throw Spine::Exception(BCP, "Unable to find coordinates for location '" + name + "'");

      latlon_center = true;
      cx = loc->longitude;
      cy = loc->latitude;
    }

    // Note: this must be handled after the CRS variables!

    json = theJson.get("bbox", nulljson);
    if (!json.isNull())
    {
      std::string bbox = json.asString();
      std::vector<std::string> parts;
      boost::algorithm::split(parts, bbox, boost::algorithm::is_any_of(","));
      if (parts.size() != 4)
        throw Spine::Exception(BCP, "bbox should contain 4 comma separated doubles");

      // for example in EPSG:4326 order of coordinates is changed in WMS
      // for more info: https://trac.osgeo.org/gdal/wiki/rfc20_srs_axes

      OGRSpatialReference spatref;
      if (bboxcrs)
      {
        // SetFromUserInput wants 'EPSGA' - code in order to understand axis ordering
        if (boost::algorithm::starts_with(*bboxcrs, "EPSG"))
        {
          boost::algorithm::replace_first(*bboxcrs, "EPSG", "EPSGA");
        }

        spatref.SetFromUserInput(bboxcrs->c_str());
      }
      else
      {
        // SetFromUserInput wants 'EPSGA' - code in order to understand axis ordering
        if (boost::algorithm::starts_with(*crs, "EPSG"))
        {
          boost::algorithm::replace_first(*crs, "EPSG", "EPSGA");
        }

        spatref.SetFromUserInput(crs->c_str());
      }

      bool latLonOrder = spatref.EPSGTreatsAsLatLong();

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
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Projection::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(crs);
    boost::hash_combine(hash, Dali::hash_value(xsize));
    boost::hash_combine(hash, Dali::hash_value(ysize));
    boost::hash_combine(hash, Dali::hash_value(x1));
    boost::hash_combine(hash, Dali::hash_value(y1));
    boost::hash_combine(hash, Dali::hash_value(x2));
    boost::hash_combine(hash, Dali::hash_value(y2));
    boost::hash_combine(hash, Dali::hash_value(cx));
    boost::hash_combine(hash, Dali::hash_value(cy));
    boost::hash_combine(hash, Dali::hash_value(resolution));
    boost::hash_combine(hash, Dali::hash_value(bboxcrs));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      crs = theQ->area().WKT().c_str();

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

      ogr_crs.reset();
      box.reset();
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Return the spatial reference
 */
// ----------------------------------------------------------------------

boost::shared_ptr<OGRSpatialReference> Projection::getCRS() const
{
  try
  {
    prepareCRS();
    return ogr_crs;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      throw Spine::Exception(BCP, "CRS not set, unable to create projection");

    if (!xsize && !ysize)
      throw Spine::Exception(BCP, "CRS xsize and ysize are both missing");

    // Are subdefinitions complete?
    bool full_rect_bbox = (x1 && y1 && x2 && y2);
    bool full_center_bbox = (cx && cy && resolution);

    // Are they partial?
    bool partial_rect_bbox = !full_rect_bbox && (x1 || y1 || x2 || y2);
    bool partial_center_bbox = !full_center_bbox && (cx || cy || resolution);

    // Disallow partial definitions
    if (partial_rect_bbox)
      throw Spine::Exception(BCP, "Partial CRS bounding box given: x1,y2,x2,y2 are needed");
    if (partial_center_bbox)
      throw Spine::Exception(BCP, "Partial CRS bounding box given: x1,y2,x2,y2 are needed");

    // bbox definition missing completely?
    if (!full_rect_bbox && !full_center_bbox)
      throw Spine::Exception(
          BCP, "CRS bounding box missing: x1,y2,x2,y2 or cx,cy,resolution are needed");

// two conflicting definitions is OK, since we create the corners from centered bbox
#if 0
        if(full_rect_bbox && full_center_bbox)
      throw Spine::Exception(BCP,"CRS bounding box overdefined: use only x1,y2,x2,y2 or cx,cy,resolution");
#endif

    // must give both width and height if centered bbox is given
    if (!full_rect_bbox && full_center_bbox && (!xsize || !ysize))
      throw Spine::Exception(
          BCP, "CRS xsize and ysize are required when a centered bounding box is used");

    // Create the CRS
    ogr_crs = boost::make_shared<OGRSpatialReference>();
    OGRErr err = ogr_crs->SetFromUserInput(crs->c_str());
    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP, "Unknown CRS: '" + *crs + "'");

    if (xsize && *xsize <= 0)
      throw Spine::Exception(BCP, "Projection xsize must be positive");

    if (ysize && *ysize <= 0)
      throw Spine::Exception(BCP, "Projection ysize must be positive");

    // World XY bounding box will be calculated during the process

    double XMIN, YMIN, XMAX, YMAX;

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
        OGRSpatialReference ogr_crs2;
        err = ogr_crs2.SetFromUserInput(bboxcrs->c_str());
        if (err != OGRERR_NONE)
          throw Spine::Exception(BCP, "Unknown CRS: '" + *bboxcrs + "'");

        boost::shared_ptr<OGRCoordinateTransformation> transformation(
            OGRCreateCoordinateTransformation(&ogr_crs2, ogr_crs.get()));
        transformation->Transform(1, &XMIN, &YMIN);
        transformation->Transform(1, &XMAX, &YMAX);
      }

      if (XMIN == XMAX || YMIN == YMAX)
        throw Spine::Exception(BCP, "Bounding box size is zero!");

      if (!xsize)
      {
        // Preserve aspect by calculating xsize
        int w = boost::numeric_cast<int>((*ysize) * (XMAX - XMIN) / (YMAX - YMIN));
        box = boost::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, w, *ysize);
      }
      else if (!ysize)
      {
        // Preserve aspect by calculating ysize
        int h = boost::numeric_cast<int>((*xsize) * (YMAX - YMIN) / (XMAX - XMIN));
        box = boost::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, h);
      }
      else
      {
        box = boost::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, *ysize);
      }

      if (ogr_crs->IsGeographic())
      {
        // Substract equations 5 and 4, subsititute equation 3 and solve resolution
        double pi = boost::math::constants::pi<double>();
        double circumference = 2 * pi * 6371.220;
        resolution = (YMAX - YMIN) * circumference / (360 * (*ysize));
        ;
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
        throw Spine::Exception(BCP,
                               "xsize and ysize are required when a centered bounding box is used");

      double CX = *cx, CY = *cy;

      if (latlon_center || (bboxcrs && *crs != *bboxcrs))
      {
        // Reproject center coordinates from latlon/bboxcrs to crs
        OGRSpatialReference ogr_crs2;
        if (latlon_center)
          err = ogr_crs2.SetFromUserInput("WGS84");
        else
          err = ogr_crs2.SetFromUserInput(bboxcrs->c_str());

        if (err != OGRERR_NONE)
          throw Spine::Exception(BCP, "Unknown CRS: '" + *bboxcrs + "'");

        boost::shared_ptr<OGRCoordinateTransformation> transformation(
            OGRCreateCoordinateTransformation(&ogr_crs2, ogr_crs.get()));
        transformation->Transform(1, &CX, &CY);
      }

      if (ogr_crs->IsGeographic())
      {
        double pi = boost::math::constants::pi<double>();
        double circumference = 2 * pi * 6371.220;

        // This will work well, since we move along an isocircle
        double dy = 360 * (*ysize) / 2 * (*resolution) / circumference;  // Equation 3.
        YMIN = CY - dy;                                                  // Equation 4.
        YMAX = CY + dy;                                                  // Equation 5

        // Distances will become distorted the further away we are from the equator
        double dx = 360 * (*xsize) / 2 * (*resolution) / circumference / cos(CX * pi / 180);
        XMIN = CX - dx;
        XMAX = CX + dx;

        box = boost::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, *ysize);
      }
      else
      {
        XMIN = CX - (*xsize) / 2 * (*resolution) * 1000;  // Equation 1.
        XMAX = CX + (*xsize) / 2 * (*resolution) * 1000;  // Equation 2.
        YMIN = CY - (*ysize) / 2 * (*resolution) * 1000;
        YMAX = CY + (*ysize) / 2 * (*resolution) * 1000;
        box = boost::make_shared<Fmi::Box>(XMIN, YMIN, XMAX, YMAX, *xsize, *ysize);
      }
    }

    // newbase corners calculated from world xy coordinates

    const char* fmiwkt = R"xxx(GEOGCS["FMI_Sphere",DATUM["FMI_2007",SPHEROID["FMI_Sphere",)xxx"
                         R"xxx(6371220,0]],PRIMEM["Greenwich",0],UNIT["Degree",0.)xxx"
                         R"xxx(0174532925199433]])xxx";
    OGRSpatialReference fmi;
    err = fmi.SetFromUserInput(fmiwkt);

    if (err != OGRERR_NONE)
      throw Spine::Exception(BCP, "Unable to parse FMI WKT in Dali::Projection");

    boost::shared_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(ogr_crs.get(), &fmi));

    // Calculate bottom left and top right coordinates

    transformation->Transform(1, &XMIN, &YMIN);
    itsBottomLeft = NFmiPoint(XMIN, YMIN);

    transformation->Transform(1, &XMAX, &YMAX);
    itsTopRight = NFmiPoint(XMAX, YMAX);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
