#include "Geometry.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <engines/gis/Engine.h>
#include <gdal/ogr_spatialref.h>
#include <macgyver/Exception.h>

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
      throw Fmi::Exception(
          BCP, "Encountered an unknown geometry component when extracting geometry name");
    case wkbNone:
      throw Fmi::Exception(BCP, "Encountered a 'none' geometry when extracting geometry name");
  }

  throw Fmi::Exception(BCP, "Unknown geometry type when extracting geometry name");
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the geometry name for the given format
 */
// ----------------------------------------------------------------------

std::string name(const OGRGeometry& theGeom, const State& theState)
{
  auto type = theState.getType();

  if (type == "geojson")
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
                      const std::shared_ptr<OGRSpatialReference>& theSRS,
                      const State& theState)
{
  // Reproject to WGS84. TODO: Optimize if theSRS == WGS84.

  auto wgs84 = theState.getGisEngine().getSpatialReference("WGS84");

  boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
      OGRCreateCoordinateTransformation(theSRS.get(), wgs84.get()));
  if (transformation == nullptr)
    throw Fmi::Exception(BCP,
                           "Failed to create the coordinate transformation for producing GeoJSON");

  // Reproject a clone
  boost::movelib::unique_ptr<OGRGeometry> geom(theGeom.clone());
  auto err = geom->transform(transformation.get());
  if (err != OGRERR_NONE)
    throw Fmi::Exception(BCP, "Failed to project geometry to WGS84 GeoJSON");

  // Fix winding rule to be CCW for shells
  boost::movelib::unique_ptr<OGRGeometry> geom2(Fmi::OGR::reverseWindingOrder(*geom));

  char* tmp = geom2->exportToJson();
  std::string ret = tmp;
  CPLFree(tmp);

  // Extract the coordinates

  std::size_t pos1 = ret.find('[');
  std::size_t pos2 = ret.rfind(']');

  if (pos1 == std::string::npos || pos2 == std::string::npos)
    throw Fmi::Exception(BCP, "Failed to extract GeoJSON from GDAL output");

  return ret.substr(pos1, pos2 - pos1 + 1);
}

// ----------------------------------------------------------------------
/*!
 * \brief Export the coordinates to KML format
 */
// ----------------------------------------------------------------------

std::string toKML(const OGRGeometry& theGeom,
                  const Fmi::Box& /* theBox */,
                  const std::shared_ptr<OGRSpatialReference>& theSRS,
                  const State& theState)
{
  // Reproject to WGS84. TODO: Optimize if theSRS == WGS84.

  auto wgs84 = theState.getGisEngine().getSpatialReference("WGS84");

  boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
      OGRCreateCoordinateTransformation(theSRS.get(), wgs84.get()));
  if (transformation == nullptr)
    throw Fmi::Exception(BCP, "Failed to create the coordinate transformation for producing KML");

  // Reproject a clone
  boost::movelib::unique_ptr<OGRGeometry> geom(theGeom.clone());
  auto err = geom->transform(transformation.get());
  if (err != OGRERR_NONE)
    throw Fmi::Exception(BCP, "Failed to project geometry to WGS84 KML");

  char* tmp = geom->exportToKML();
  std::string ret = tmp;
  CPLFree(tmp);

  // Extract the coordinates

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Export the coordinates to the given format
 */
// ----------------------------------------------------------------------

std::string toString(const OGRGeometry& theGeom,
                     const State& theState,
                     const Fmi::Box& theBox,
                     const std::shared_ptr<OGRSpatialReference>& theSRS,
                     double thePrecision)
{
  auto type = theState.getType();

  if (type == "geojson")
    return toGeoJSON(theGeom, theBox, theSRS, theState);

  if (type == "kml")
    return toKML(theGeom, theBox, theSRS, theState);

  return Fmi::OGR::exportToSvg(theGeom, theBox, thePrecision);
}

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
