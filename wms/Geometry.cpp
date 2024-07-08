#include "Geometry.h"
#include "Hash.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/CoordinateTransformation.h>
#include <gis/SpatialReference.h>
#include <macgyver/Exception.h>
#include <ogr_api.h>
#include <ogr_geometry.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Geometry
{
using CoordinateCounter = std::map<unsigned long long, unsigned long long>;

struct GInfo
{
  int precision = 0;
  double mp = 0;
  int arcCounter = 0;
  int coordinateCounter = 0;
  int globalArcCounter = 0;
  std::map<std::size_t, uint>* arcHashMap{};
  std::ostringstream arcNumbers;
  std::ostringstream arcCoordinates;
  std::ostringstream pointCoordinates;
};

FILE* file = nullptr;

// ----------------------------------------------------------------------
/*!
 * \brief Get the GeoJSON specific geometry name
 */
// ----------------------------------------------------------------------

std::string name_geojson(const OGRGeometry& theGeom)
{
  try
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
        throw Fmi::Exception(
            BCP, "Encountered an unknown geometry component when extracting geometry name");
    }

    throw Fmi::Exception(BCP, "Unknown geometry type when extracting geometry name");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the geometry name for the given format
 */
// ----------------------------------------------------------------------

std::string name(const OGRGeometry& theGeom, const std::string& theType)
{
  try
  {
    if (theType == "geojson")
      return name_geojson(theGeom);

    if (theType == "topojson")
      return name_geojson(theGeom);

    // By default we use WKT names
    return theGeom.getGeometryName();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

extern void extractGeometry(const OGRGeometry* theGeom, uint mode, GInfo& info);

void extractGeometry_point(const OGRPoint* theGeom, uint /* mode */, GInfo& info)
{
  try
  {
    double xx = theGeom->getX();
    double yy = theGeom->getY();

    long long xxi = (unsigned long long)(xx * info.mp);
    long long yyi = (unsigned long long)(yy * info.mp);

    info.pointCoordinates << "[" << xxi << "," << yyi << "]";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry_lineString(const OGRLineString* lineString, uint /* mode */, GInfo& info)
{
  try
  {
    std::ostringstream arcCoordinates;
    std::ostringstream reverseArcsCoordinates;

    int len = lineString->getNumPoints();
    if (len < 2)
      return;

    double xx = lineString->getX(0);
    double yy = lineString->getY(0);

    long long xxi = static_cast<unsigned long long>(xx * info.mp);
    long long yyi = static_cast<unsigned long long>(yy * info.mp);

    arcCoordinates << "[";
    arcCoordinates << "[" << xxi << "," << yyi << "]";
    // arcCoordinates << "[" << xx << "," << yy << "]";

    for (int t = 1; t < len; t++)
    {
      double x = lineString->getX(t);
      double y = lineString->getY(t);

      long long xi = static_cast<unsigned long long>(x * info.mp);
      long long yi = static_cast<unsigned long long>(y * info.mp);

      long long nx = (xi - xxi);
      long long ny = (yi - yyi);

      if (nx || ny)
      {
        arcCoordinates << ",[" << (xi - xxi) << "," << (yi - yyi) << "]";
        // arcCoordinates << ",[" << (x-xx) << "," << (y-yy) << "]";

        xxi = xi;
        yyi = yi;
      }
    }
    arcCoordinates << "]";

    xx = lineString->getX(len - 1);
    yy = lineString->getY(len - 1);

    xxi = static_cast<long long>(xx * info.mp);
    yyi = static_cast<long long>(yy * info.mp);

    reverseArcsCoordinates << "[";
    reverseArcsCoordinates << "[" << xxi << "," << yyi << "]";
    // reverseArcsCoordinates << "[" << xx << "," << yy << "]";

    for (int t = len - 2; t >= 0; t--)
    {
      double x = lineString->getX(t);
      double y = lineString->getY(t);

      long long xi = static_cast<unsigned long long>(x * info.mp);
      long long yi = static_cast<unsigned long long>(y * info.mp);

      long long nx = (xi - xxi);
      long long ny = (yi - yyi);

      if (nx || ny)
      {
        reverseArcsCoordinates << ",[" << (xi - xxi) << "," << (yi - yyi) << "]";
        // reverseArcsCoordinates << ",[" << (x-xx) << "," << (y-yy) << "]";

        xxi = xi;
        yyi = yi;
      }
    }
    reverseArcsCoordinates << "]";

    int archNumber = info.globalArcCounter;

    std::string str = arcCoordinates.str();
    auto hash = Fmi::hash_value(str);

    std::string reverseStr = reverseArcsCoordinates.str();
    auto reverseHash = Fmi::hash_value(reverseStr);

    auto it = info.arcHashMap->find(hash);
    if (it != info.arcHashMap->end())
    {
      // printf("HASH FOUND %u\n",it->second);
      archNumber = it->second;
    }
    else
    {
      it = info.arcHashMap->find(reverseHash);
      if (it != info.arcHashMap->end())
      {
        // printf("REVERSE HASH FOUND %u\n",it->second);
        archNumber = -(it->second + 1);
      }
      else
      {
        info.arcHashMap->insert(std::pair<std::size_t, int>(hash, info.globalArcCounter));
        // arcCoordinateCache.insert(std::pair<std::string,int>(str,info.globalArcCounter));

        if (info.coordinateCounter > 0)
          info.arcCoordinates << ",\n";

        info.arcCoordinates << str;
        info.coordinateCounter++;
        info.globalArcCounter++;
      }
    }

    if (info.arcCounter > 0)
      info.arcNumbers << ",";

    info.arcNumbers << "[" << archNumber << "]";

    info.arcCounter++;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry_linearRing(const OGRLinearRing* linearRing, uint mode, GInfo& info)
{
  try
  {
    extractGeometry_lineString(linearRing, mode, info);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry_polygon(const OGRPolygon* polygon, uint mode, GInfo& info)
{
  try
  {
    if (!polygon)
      return;

    const OGRLinearRing* ring = polygon->getExteriorRing();

    extractGeometry_linearRing(ring, mode, info);

    int len = polygon->getNumInteriorRings();
    for (int t = 0; t < len; t++)
    {
      ring = polygon->getInteriorRing(t);
      extractGeometry_linearRing(ring, mode, info);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry_multiPoint(const OGRMultiPoint* multiPoint, uint mode, GInfo& info)
{
  try
  {
    int len = multiPoint->getNumGeometries();
    for (int t = 0; t < len; t++)
    {
      const OGRGeometry* geom = multiPoint->getGeometryRef(t);
      const OGRPoint* point = geom->toPoint();
      extractGeometry_point(point, mode, info);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry_multiLineString(const OGRMultiLineString* multiLineString,
                                     uint mode,
                                     GInfo& info)
{
  try
  {
    int len = multiLineString->getNumGeometries();
    for (int t = 0; t < len; t++)
    {
      const OGRGeometry* geom = multiLineString->getGeometryRef(t);
      const OGRLineString* lineString = geom->toLineString();
      extractGeometry_lineString(lineString, mode, info);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry_multiPolygon(const OGRMultiPolygon* multiPolygon, uint mode, GInfo& info)
{
  try
  {
    if (info.arcCounter > 0)
      info.arcNumbers << ",";

    info.arcNumbers << "[";
    int len = multiPolygon->getNumGeometries();
    for (int t = 0; t < len; t++)
    {
      const OGRGeometry* geom = multiPolygon->getGeometryRef(t);
      const OGRPolygon* polygon = geom->toPolygon();
      extractGeometry_polygon(polygon, mode, info);
    }
    info.arcNumbers << "]";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry_geometryCollection(const OGRGeometryCollection* geometryCollection,
                                        uint mode,
                                        GInfo& info)
{
  try
  {
    int len = geometryCollection->getNumGeometries();
    for (int t = 0; t < len; t++)
    {
      const OGRGeometry* geom = geometryCollection->getGeometryRef(t);
      extractGeometry(geom, mode, info);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void extractGeometry(const OGRGeometry* geometry, uint mode, GInfo& info)
{
  try
  {
    auto id = geometry->getGeometryType();
    switch (id)
    {
      case wkbPoint:
        extractGeometry_point(dynamic_cast<const OGRPoint*>(geometry), mode, info);
        break;

      case wkbLineString:
        extractGeometry_lineString(dynamic_cast<const OGRLineString*>(geometry), mode, info);
        break;

      case wkbLinearRing:
        extractGeometry_linearRing(dynamic_cast<const OGRLinearRing*>(geometry), mode, info);
        break;

      case wkbPolygon:
        extractGeometry_polygon(dynamic_cast<const OGRPolygon*>(geometry), mode, info);
        break;

      case wkbMultiPoint:
        extractGeometry_multiPoint(dynamic_cast<const OGRMultiPoint*>(geometry), mode, info);
        break;

      case wkbMultiLineString:
        extractGeometry_multiLineString(
            dynamic_cast<const OGRMultiLineString*>(geometry), mode, info);
        break;

      case wkbMultiPolygon:
        extractGeometry_multiPolygon(dynamic_cast<const OGRMultiPolygon*>(geometry), mode, info);
        break;

      case wkbGeometryCollection:
        extractGeometry_geometryCollection(
            dynamic_cast<const OGRGeometryCollection*>(geometry), mode, info);
        break;

      default:
        throw Fmi::Exception(BCP, "Encountered an unknown geometry component!");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Export the coordinates to TopoJSON format
 */
// ----------------------------------------------------------------------

std::string toTopoJSON(const OGRGeometry& theGeom,
                       const Fmi::Box& /* theBox */,
                       const Fmi::SpatialReference& /* theSRS */,
                       double thePrecision,
                       std::map<std::size_t, uint>& arcHashMap,
                       uint& arcCounter,
                       std::string& arcNumbers,
                       std::string& arcCoordinates)
{
  try
  {
    GInfo info;
    info.arcHashMap = &arcHashMap;
    info.precision = (int)(thePrecision);
    if (thePrecision >= 1.0)
      info.mp = pow(10.0, info.precision);

    info.arcCounter = 0;
    info.coordinateCounter = 0;
    info.globalArcCounter = arcCounter;
    info.arcNumbers << "[";
    extractGeometry(&theGeom, 0, info);
    arcCounter = info.globalArcCounter;
    info.arcNumbers << "]";
    arcCoordinates = info.arcCoordinates.str();
    arcNumbers = info.arcNumbers.str();
    return info.pointCoordinates.str();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Export the coordinates to GeoJSON format
 */
// ----------------------------------------------------------------------

std::string toGeoJSON(const OGRGeometry& theGeom,
                      const Fmi::Box& /* theBox */,
                      const Fmi::SpatialReference& theSRS,
                      double thePrecision)
{
  try
  {
#if 0    
    if (!theGeom.IsValid())
      throw Fmi::Exception(BCP, "Input geometry for conversion to GeoJSON is not valid");
#endif

    // Reproject to WGS84. TODO: Optimize if theSRS == WGS84.

    Fmi::CoordinateTransformation transformation(theSRS, "WGS84");

    // Reproject a clone
    std::unique_ptr<OGRGeometry> geom(theGeom.clone());
    auto err = geom->transform(transformation.get());
    if (err != OGRERR_NONE)
      throw Fmi::Exception(BCP, "Failed to project geometry to WGS84 GeoJSON");

    // Fix winding rule to be CCW for shells
    std::unique_ptr<OGRGeometry> geom2(Fmi::OGR::reverseWindingOrder(*geom));

    // No C++ API with precision yet, must use C API

    auto prec = fmt::format("COORDINATE_PRECISION={}", static_cast<int>(thePrecision));
    char* options[] = {const_cast<char*>(prec.c_str()), nullptr};  // NOLINT
    auto* geom3 = const_cast<OGRGeometry*>(geom2.get());
    char* tmp = OGR_G_ExportToJsonEx(OGRGeometry::ToHandle(geom3), options);

    // GDAL does not allow empty geometries even though it is valid according to GeoJSON RFC
    if (tmp == nullptr)
      throw Fmi::Exception(BCP, "Failed to convert geometry to GeoJSON");

    std::string ret = tmp;
    CPLFree(tmp);

    // Extract the coordinates

    std::size_t pos1 = ret.find('[');
    std::size_t pos2 = ret.rfind(']');

    if (pos1 == std::string::npos || pos2 == std::string::npos)
      throw Fmi::Exception(BCP, "Failed to extract GeoJSON from GDAL output");

    return ret.substr(pos1, pos2 - pos1 + 1);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
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
  try
  {
    // Reproject to WGS84. TODO: Optimize if theSRS == WGS84.

    Fmi::CoordinateTransformation transformation(theSRS, "WGS84");

    // Reproject a clone
    std::unique_ptr<OGRGeometry> geom(theGeom.clone());
    auto err = geom->transform(transformation.get());
    if (err != OGRERR_NONE)
      throw Fmi::Exception(BCP, "Failed to project geometry to WGS84 KML");

    char* tmp = geom->exportToKML();
    std::string ret = tmp;
    CPLFree(tmp);

    // Extract the coordinates

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
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
  try
  {
    if (theType == "geojson")
      return toGeoJSON(theGeom, theBox, theSRS, thePrecision);

    if (theType == "kml")
      return toKML(theGeom, theBox, theSRS);

    return Fmi::OGR::exportToSvg(theGeom, theBox, thePrecision);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
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
                     double thePrecision,
                     std::map<std::size_t, uint>& arcHashMap,
                     uint& arcCounter,
                     std::string& arcNumbers,
                     std::string& arcCoordinates)
{
  try
  {
    if (theType == "topojson")
      return toTopoJSON(theGeom,
                        theBox,
                        theSRS,
                        thePrecision,
                        arcHashMap,
                        arcCounter,
                        arcNumbers,
                        arcCoordinates);

    if (theType == "geojson")
      return toGeoJSON(theGeom, theBox, theSRS, thePrecision);

    if (theType == "kml")
      return toKML(theGeom, theBox, theSRS);

    return Fmi::OGR::exportToSvg(theGeom, theBox, thePrecision);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Geometry
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
