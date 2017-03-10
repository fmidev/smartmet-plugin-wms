#include "Geometry.h"
#include <spine/Exception.h>
#include <gdal/ogr_spatialref.h>

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
    case wkbUnknown:
      throw Spine::Exception(
          BCP, "Encountered an unknown geometry component when extracting geometry name");
    case wkbNone:
      throw Spine::Exception(BCP, "Encountered a 'none' geometry when extracting geometry name");
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
                      const Fmi::Box& theBox,
                      boost::shared_ptr<OGRSpatialReference> theSRS)
{
  // Reproject to WGS84. TODO: Optimize if theSRS == WGS84.

  std::unique_ptr<OGRSpatialReference> wgs84(new OGRSpatialReference);
  OGRErr err = wgs84->SetFromUserInput("WGS84");
  if (err != OGRERR_NONE)
    throw Spine::Exception(BCP, "GDAL does not understand WGS84");

  std::unique_ptr<OGRCoordinateTransformation> transformation(
      OGRCreateCoordinateTransformation(theSRS.get(), wgs84.get()));
  if (!transformation)
    throw Spine::Exception(BCP,
                           "Failed to create the coordinate transformation for producing GeoJSON");

  // Reproject a clone
  std::unique_ptr<OGRGeometry> geom(theGeom.clone());
  err = geom->transform(transformation.get());
  if (err != OGRERR_NONE)
    throw Spine::Exception(BCP, "Failed to project geometry to WGS84 GeoJSON");

  char* tmp = geom->exportToJson();
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
 * \brief Export the coordinates to the given format
 */
// ----------------------------------------------------------------------

std::string toString(const OGRGeometry& theGeom,
                     const std::string& theType,
                     const Fmi::Box& theBox,
                     boost::shared_ptr<OGRSpatialReference> theSRS)
{
  if (theType == "geojson")
    return toGeoJSON(theGeom, theBox, theSRS);

  // Default is SVG-style
  const int precision = 1;
  return Fmi::OGR::exportToSvg(theGeom, theBox, precision);
}

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
