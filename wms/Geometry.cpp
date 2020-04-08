#include "Geometry.h"
#include <boost/move/make_unique.hpp>
#include <gis/CoordinateTransformation.h>
#include <gis/SpatialReference.h>
#include <spine/Exception.h>
#include <ogr_geometry.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Geometry
{
// ----------------------------------------------------------------------
/*!
 * \brief Get the GeoJSON specific geometry name
 */
// ----------------------------------------------------------------------

std::string name_geojson(const OGRGeometry& theGeom)
{
  auto id = theGeom.getGeometryType();
  switch (id)
  {
    case wkbPoint:
    case wkbPoint25D:
      return "Point";
    case wkbLineString:
    case wkbLineString25D:
    case wkbLinearRing:
      return "LineString";
    case wkbPolygon:
    case wkbPolygon25D:
      return "Polygon";
    case wkbMultiPoint:
    case wkbMultiPoint25D:
      return "MultiPoint";
    case wkbMultiLineString:
    case wkbMultiLineString25D:
      return "MultiLineString";
    case wkbMultiPolygon:
    case wkbMultiPolygon25D:
      return "MultiPolygon";
    case wkbGeometryCollection:
    case wkbGeometryCollection25D:
      return "GeometryCollection";
    default:
      throw Spine::Exception(
          BCP, "Encountered an unknown geometry component when extracting geometry name");
  }

  throw Spine::Exception(BCP, "Unknown geometry type when extracting geometry name");
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the geometry name for the given format
 */
// ----------------------------------------------------------------------

std::string name(const OGRGeometry& theGeom, const std::string& theType)
{
  if (theType == "geojson")
    return name_geojson(theGeom);

  // By default we use WKT names
  return theGeom.getGeometryName();
}

// ----------------------------------------------------------------------
/*!
 * \brief Export the coordinates to GeoJSON format
 */
// ----------------------------------------------------------------------

std::string toGeoJSON(const OGRGeometry& theGeom,
                      const Fmi::Box& /* theBox */,
                      const Fmi::SpatialReference& theSRS)
{
  // Reproject to WGS84. TODO: Optimize if theSRS == WGS84.

  Fmi::CoordinateTransformation transformation(theSRS, "WGS84");

  // Reproject a clone
  boost::movelib::unique_ptr<OGRGeometry> geom(theGeom.clone());
  auto err = geom->transform(transformation.get());
  if (err != OGRERR_NONE)
    throw Spine::Exception(BCP, "Failed to project geometry to WGS84 GeoJSON");

  // Fix winding rule to be CCW for shells
  boost::movelib::unique_ptr<OGRGeometry> geom2(Fmi::OGR::reverseWindingOrder(*geom));

  char* tmp = geom2->exportToJson();
  std::string ret = tmp;
  OGRFree(tmp);

  // Extract the coordinates

  std::size_t pos1 = ret.find('[');
  std::size_t pos2 = ret.rfind(']');

  if (pos1 == std::string::npos || pos2 == std::string::npos)
    throw Spine::Exception(BCP, "Failed to extract GeoJSON from GDAL output");

  return ret.substr(pos1, pos2 - pos1 + 1);
}

// ----------------------------------------------------------------------
/*!
 * \brief Export the coordinates to KML format
 */
// ----------------------------------------------------------------------

std::string toKML(const OGRGeometry& theGeom,
                  const Fmi::Box& /* theBox */,
                  const Fmi::SpatialReference& theSRS)
{
  // Reproject to WGS84. TODO: Optimize if theSRS == WGS84.

  Fmi::CoordinateTransformation transformation(theSRS, "WGS84");

  // Reproject a clone
  boost::movelib::unique_ptr<OGRGeometry> geom(theGeom.clone());
  auto err = geom->transform(transformation.get());
  if (err != OGRERR_NONE)
    throw Spine::Exception(BCP, "Failed to project geometry to WGS84 KML");

  char* tmp = geom->exportToKML();
  std::string ret = tmp;
  OGRFree(tmp);

  // Extract the coordinates

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Export the coordinates to the given format
 */
// ----------------------------------------------------------------------

std::string toString(const OGRGeometry& theGeom,
                     const std::string& theType,
                     const Fmi::Box& theBox,
                     const Fmi::SpatialReference& theSRS,
                     double thePrecision)
{
  if (theType == "geojson")
    return toGeoJSON(theGeom, theBox, theSRS);

  if (theType == "kml")
    return toKML(theGeom, theBox, theSRS);

  return Fmi::OGR::exportToSvg(theGeom, theBox, thePrecision);
}

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
