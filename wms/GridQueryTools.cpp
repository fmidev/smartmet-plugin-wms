#include "GridQueryTools.h"
#include "ParameterInfo.h"
#include "Projection.h"
#include "Smoother.h"
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <gis/SpatialReference.h>
#include <engines/grid/Engine.h>
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/StringConversion.h>
#include <cstring>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Add the grid-engine parameter to the query attributes
 */
// ----------------------------------------------------------------------

std::string add_grid_parameter(const Engine::Grid::Engine& gridEngine,
                               const std::string& producerName,
                               const std::string& parameterName,
                               const std::optional<double>& multiplier,
                               const std::optional<double>& offset,
                               Projection& projection,
                               T::AttributeList& attributeList,
                               QueryServer::Query& query)
{
  std::string pName = parameterName;
  auto pos = pName.find(".raw");
  if (pos != std::string::npos)
  {
    attributeList.addAttribute("grid.areaInterpolationMethod",
                               Fmi::to_string(T::AreaInterpolationMethod::Nearest));
    pName.erase(pos, 4);
  }

  std::string param = gridEngine.getParameterString(producerName, pName);

  if (multiplier && *multiplier != 1.0)
    param = "MUL{" + param + ";" + std::to_string(*multiplier) + "}";

  if (offset && *offset)
    param = "SUM{" + param + ";" + std::to_string(*offset) + "}";

  attributeList.addAttribute("param", param);

  if (!projection.projectionParameter)
    projection.projectionParameter = param;

  if (param == parameterName && query.mProducerNameList.empty())
  {
    gridEngine.getProducerNameList(producerName, query.mProducerNameList);
    if (query.mProducerNameList.empty())
      query.mProducerNameList.push_back(producerName);
  }
  return param;
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the time attributes to the query attributes
 */
// ----------------------------------------------------------------------

void add_time_attributes(T::AttributeList& attributeList,
                         const Fmi::DateTime& validTime,
                         const std::optional<Fmi::DateTime>& origintime)
{
  std::string forecastTime = Fmi::to_iso_string(validTime);
  attributeList.addAttribute("startTime", forecastTime);
  attributeList.addAttribute("endTime", forecastTime);
  attributeList.addAttribute("timelist", forecastTime);
  attributeList.addAttribute("timezone", "UTC");

  if (origintime)
    attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the geometry, level and forecast selections to a query parameter
 */
// ----------------------------------------------------------------------

void set_query_parameter_selections(QueryServer::QueryParameter& param,
                                    const ParameterInfo& paraminfo)
{
  if (paraminfo.geometryId)
    param.mGeometryId = *paraminfo.geometryId;

  if (paraminfo.levelId)
    param.mParameterLevelId = *paraminfo.levelId;

  if (paraminfo.level)
  {
    param.mParameterLevel = C_INT(*paraminfo.level);
  }
  else if (paraminfo.pressure)
  {
    param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
    param.mParameterLevel = C_INT(*paraminfo.pressure);
  }

  if (paraminfo.elevation_unit)
  {
    if (*paraminfo.elevation_unit == "m")
      param.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;

    if (*paraminfo.elevation_unit == "p")
      param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
  }

  if (paraminfo.forecastType)
    param.mForecastType = C_INT(*paraminfo.forecastType);

  if (paraminfo.forecastNumber)
    param.mForecastNumber = C_INT(*paraminfo.forecastNumber);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the grid size attributes of the projection to the query
 */
// ----------------------------------------------------------------------

void add_grid_size_attributes(QueryServer::Query& query, const Projection& projection)
{
  if (projection.size && *projection.size > 0)
  {
    query.mAttributeList.addAttribute("grid.size", Fmi::to_string(*projection.size));
  }
  else
  {
    if (projection.xsize)
      query.mAttributeList.addAttribute("grid.width", Fmi::to_string(*projection.xsize));

    if (projection.ysize)
      query.mAttributeList.addAttribute("grid.height", Fmi::to_string(*projection.ysize));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the clipping bounding box of the requested projection
 */
// ----------------------------------------------------------------------

void add_grid_bbox_attributes(QueryServer::Query& query,
                              const Projection& projection,
                              const Fmi::Box& clipbox,
                              bool animation_enabled)
{
  auto bbox =
      fmt::format("{},{},{},{}", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(), clipbox.ymax());
  auto bl = projection.bottomLeftLatLon();
  auto tr = projection.topRightLatLon();

  // # Testing if the target grid is defined as latlon:
  if (projection.x1 == bl.X() && projection.y1 == bl.Y() && projection.x2 == tr.X() &&
      projection.y2 == tr.Y())
    query.mAttributeList.addAttribute("grid.llbox", bbox);

  query.mAttributeList.addAttribute("grid.bbox", bbox);
  if (!animation_enabled)
    query.mAttributeList.addAttribute("grid.countSize", "1");
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the bboxcrs, cx, cy and resolution settings of the projection
 */
// ----------------------------------------------------------------------

void add_projection_attributes(QueryServer::Query& query, const Projection& projection)
{
  if (projection.bboxcrs)
    query.mAttributeList.addAttribute("grid.bboxcrs", *projection.bboxcrs);

  if (projection.cx)
    query.mAttributeList.addAttribute("grid.cx", Fmi::to_string(*projection.cx));

  if (projection.cy)
    query.mAttributeList.addAttribute("grid.cy", Fmi::to_string(*projection.cy));

  if (projection.resolution)
    query.mAttributeList.addAttribute("grid.resolution", Fmi::to_string(*projection.resolution));
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the bounding box given in the projection settings
 */
// ----------------------------------------------------------------------

void add_data_bbox_attribute(QueryServer::Query& query, const Projection& projection)
{
  if (projection.x1 && projection.y1 && projection.x2 && projection.y2)
  {
    auto bbox =
        fmt::format("{},{},{},{}", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
    query.mAttributeList.addAttribute("grid.bbox", bbox);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the contour smoothing, subdivision and thread count attributes
 */
// ----------------------------------------------------------------------

void add_contour_attributes(QueryServer::Query& query,
                            const Smoother& smoother,
                            int subdivide,
                            const std::optional<int>& contour_threads)
{
  if (smoother.size)
    query.mAttributeList.addAttribute("contour.smooth.size", Fmi::to_string(*smoother.size));

  if (smoother.degree)
    query.mAttributeList.addAttribute("contour.smooth.degree", Fmi::to_string(*smoother.degree));

  if (subdivide > 0)
    query.mAttributeList.addAttribute("contour.subdivide", Fmi::to_string(subdivide));

  if (contour_threads)
    query.mAttributeList.addAttribute("contour.threads", Fmi::to_string(*contour_threads));
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the contour extrapolation, multiplier, offset and coordinate type attributes
 */
// ----------------------------------------------------------------------

void add_contour_value_attributes(QueryServer::Query& query,
                                  int extrapolation,
                                  const std::optional<double>& multiplier,
                                  const std::optional<double>& offset)
{
  query.mAttributeList.addAttribute("contour.extrapolation", Fmi::to_string(extrapolation));

  if (extrapolation && multiplier)
    query.mAttributeList.addAttribute("contour.multiplier", Fmi::to_string(*multiplier));

  if (offset)
    query.mAttributeList.addAttribute("contour.offset", Fmi::to_string(*offset));

  query.mAttributeList.setAttribute(
      "contour.coordinateType",
      Fmi::to_string(static_cast<int>(T::CoordinateTypeValue::ORIGINAL_COORDINATES)));
  // query.mAttributeList.setAttribute("contour.coordinateType",Fmi::to_string(T::CoordinateTypeValue::LATLON_COORDINATES));
  // query.mAttributeList.setAttribute("contour.coordinateType",Fmi::to_string(T::CoordinateTypeValue::GRID_COORDINATES));
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the contour interpolation method name
 */
// ----------------------------------------------------------------------

std::optional<Trax::InterpolationType> parse_contour_interpolation(const std::string& interpolation)
{
  if (interpolation == "linear")
    return Trax::InterpolationType::Linear;
  if (interpolation == "nearest" || interpolation == "discrete" || interpolation == "midpoint")
    return Trax::InterpolationType::Midpoint;
  if (interpolation == "logarithmic")
    return Trax::InterpolationType::Logarithmic;
  return std::nullopt;
}

// ----------------------------------------------------------------------
/*!
 * \brief Convert the WKB contours returned by the query into OGRGeometry objects
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> contour_geometries(const QueryServer::Query& query,
                                               const std::optional<double>& minarea,
                                               const std::string& areaunit,
                                               const Fmi::Box& box,
                                               const Fmi::SpatialReference& crs)
{
  std::vector<OGRGeometryPtr> geoms;
  for (const auto& query_param : query.mQueryParameterList)
  {
    for (const auto& val : query_param.mValueList)
    {
      for (const auto& wkb : val->mValueData)
      {
        const auto* cwkb = reinterpret_cast<const unsigned char*>(wkb.data());
        OGRGeometry* geom = nullptr;
        OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb.size());
        auto geomPtr = OGRGeometryPtr(geom);
        geoms.push_back(despeckle_contour(geomPtr, minarea, areaunit, box, crs));
      }
    }
  }
  return geoms;
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove small areas from a contour if a minimum area is set
 */
// ----------------------------------------------------------------------

OGRGeometryPtr despeckle_contour(const OGRGeometryPtr& geomPtr,
                                 const std::optional<double>& minarea,
                                 const std::string& areaunit,
                                 const Fmi::Box& box,
                                 const Fmi::SpatialReference& crs)
{
  if (geomPtr && minarea && crs.get())
  {
    auto area = *minarea;
    if (areaunit == "px^2")
      area = box.areaFactor() * area;

    OGRGeometryPtr tmp(geomPtr);
    tmp->assignSpatialReference(crs.get());
    tmp.reset(Fmi::OGR::despeckle(*tmp, area));
    return tmp;
  }
  return geomPtr;
}

// ----------------------------------------------------------------------
/*!
 * \brief Update the projection size from the query result
 */
// ----------------------------------------------------------------------

void update_projection_size(Projection& projection, const QueryServer::Query& query)
{
  if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
  {
    const char* widthStr = query.mAttributeList.getAttributeValue("grid.width");
    const char* heightStr = query.mAttributeList.getAttributeValue("grid.height");

    if (widthStr != nullptr)
      projection.xsize = Fmi::stoi(widthStr);

    if (heightStr != nullptr)
      projection.ysize = Fmi::stoi(heightStr);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Update the projection from the query result if the data projection was requested
 */
// ----------------------------------------------------------------------

void update_data_projection(Projection& projection, const QueryServer::Query& query)
{
  if (*projection.crs != "data")
    return;

  const char* crsStr = query.mAttributeList.getAttributeValue("grid.crs");
  const char* proj4Str = query.mAttributeList.getAttributeValue("grid.proj4");
  if (proj4Str != nullptr && strstr(proj4Str, "+lon_wrap") != nullptr)
    crsStr = proj4Str;

  if (crsStr == nullptr)
    return;

  projection.crs = crsStr;
  if (projection.bboxcrs)
    return;

  const char* bboxStr = query.mAttributeList.getAttributeValue("grid.bbox");
  if (bboxStr != nullptr)
  {
    std::vector<double> partList;
    splitString(bboxStr, ',', partList);

    if (partList.size() == 4)
    {
      projection.x1 = partList[0];
      projection.y1 = partList[1];
      projection.x2 = partList[2];
      projection.y2 = partList[3];
    }
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
