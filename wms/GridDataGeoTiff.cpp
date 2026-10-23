// ======================================================================

#include "GridDataGeoTiff.h"
#include "GridQueryTools.h"
#include "Layer.h"
#include "State.h"
#include <engines/grid/Engine.h>
#include <engines/querydata/Q.h>
#include <fmt/format.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/grid/Typedefs.h>
#include <macgyver/Exception.h>
#include <gis/CoordinateMatrix.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiMetTime.h>
#include <timeseries/ParameterFactory.h>
#include <atomic>
#include <cmath>
#include <cpl_vsi.h>
#include <gdal_priv.h>
#include <ogr_spatialref.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// ----------------------------------------------------------------------
/*!
 * \brief Write pre-computed north-up float bands as a deflate-compressed
 *        multi-band Float32 GeoTiff and return the raw bytes.
 */
// ----------------------------------------------------------------------

std::string writeGeoTiffBands(const Projection& projection,
                               const std::string& wkt,
                               const std::vector<std::vector<float>>& bands)
{
  try
  {
    if (bands.empty())
      throw Fmi::Exception(BCP, "writeGeoTiffBands: no bands provided");

    if (!projection.xsize || !projection.ysize)
      throw Fmi::Exception(BCP, "writeGeoTiffBands: projection dimensions not set");

    const int width = *projection.xsize;
    const int height = *projection.ysize;
    const int numBands = static_cast<int>(bands.size());

    for (int b = 0; b < numBands; ++b)
      if (static_cast<int>(bands[b].size()) != width * height)
        throw Fmi::Exception(BCP,
                             "writeGeoTiffBands: band " + Fmi::to_string(b + 1) +
                                 " size mismatch: got " + Fmi::to_string(bands[b].size()) +
                                 " expected " + Fmi::to_string(width * height));

    GDALAllRegister();

    // North-up GeoTransform
    const auto& box = projection.getBox();
    double gt[6] = {
        box.xmin(),
        (box.xmax() - box.xmin()) / width,
        0.0,
        box.ymax(),
        0.0,
        -(box.ymax() - box.ymin()) / height};

    auto* memDrv = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!memDrv)
      throw Fmi::Exception(BCP, "GDAL MEM driver not available");

    auto* memDs = memDrv->Create("", width, height, numBands, GDT_Float32, nullptr);
    if (!memDs)
      throw Fmi::Exception(BCP, "Failed to create GDAL MEM dataset");

    memDs->SetGeoTransform(gt);

    OGRSpatialReference oSRS;
    oSRS.importFromWkt(wkt.c_str());
    memDs->SetSpatialRef(&oSRS);

    const auto nodata = static_cast<double>(ParamValueMissing);

    for (int b = 0; b < numBands; ++b)
    {
      auto* band = memDs->GetRasterBand(b + 1);
      band->SetNoDataValue(nodata);
      // RasterIO requires non-const data pointer
      auto* data = const_cast<float*>(bands[b].data());
      if (band->RasterIO(GF_Write, 0, 0, width, height, data, width, height, GDT_Float32, 0, 0) !=
          CE_None)
      {
        GDALClose(memDs);
        throw Fmi::Exception(BCP,
                             "Failed to write band " + Fmi::to_string(b + 1) + " to GDAL MEM");
      }
    }

    auto* tiffDrv = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!tiffDrv)
    {
      GDALClose(memDs);
      throw Fmi::Exception(BCP, "GDAL GTiff driver not available");
    }

    static std::atomic<uint64_t> seq{0};
    const std::string vsimem_path = fmt::format("/vsimem/geotiff_{}.tif", ++seq);
    const char* opts[] = {"COMPRESS=DEFLATE", nullptr};
    auto* tiffDs = tiffDrv->CreateCopy(
        vsimem_path.c_str(), memDs, 0, const_cast<char**>(opts), nullptr, nullptr);
    GDALClose(memDs);

    if (!tiffDs)
    {
      VSIUnlink(vsimem_path.c_str());
      throw Fmi::Exception(BCP, "GDAL CreateCopy to GTiff failed");
    }
    GDALClose(tiffDs);

    vsi_l_offset file_size = 0;
    GByte* raw = VSIGetMemFileBuffer(vsimem_path.c_str(), &file_size, FALSE);
    if (!raw)
    {
      VSIUnlink(vsimem_path.c_str());
      throw Fmi::Exception(BCP, "Failed to read GeoTiff bytes from GDAL virtual filesystem");
    }

    std::string result(reinterpret_cast<char*>(raw), static_cast<size_t>(file_size));
    VSIUnlink(vsimem_path.c_str());
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "writeGeoTiffBands failed!");
  }
}

namespace
{
// ----------------------------------------------------------------------
/*!
 * \brief Interpolate a querydata parameter to the output pixels and write it as a
 *        single-band GeoTiff.
 *
 * The pixel centres are transformed to WGS84 and the data is interpolated at them, hence
 * any querydata grid can be written in any projection. Points outside the data become nodata.
 */
// ----------------------------------------------------------------------

std::string querydataGeoTiff(Layer& layer, const std::string& parameterName, const State& state)
{
  auto q = layer.getModel(state);
  if (!q)
    throw Fmi::Exception(BCP, "No querydata available for GeoTiff generation");
  if (!q->isGrid())
    throw Fmi::Exception(BCP, "GeoTiff output requires gridded querydata");

  const auto param = TS::ParameterFactory::instance().parse(parameterName);
  if (param.type() != Spine::Parameter::Type::Data)
    throw Fmi::Exception(BCP, "GeoTiff output supports only data parameters")
        .addParameter("Parameter", parameterName);
  if (!q->param(static_cast<FmiParameterName>(param.number())))
    throw Fmi::Exception(BCP, "Parameter not available in the querydata")
        .addParameter("Parameter", parameterName);

  if (layer.paraminfo.level)
  {
    if (!q->selectLevel(*layer.paraminfo.level))
      throw Fmi::Exception(BCP, "Level not available in the querydata")
          .addParameter("Level", Fmi::to_string(*layer.paraminfo.level));
  }
  else
    q->firstLevel();

  if (!layer.projection.xsize || !layer.projection.ysize)
    throw Fmi::Exception(BCP, "GeoTiff output requires the image size");
  const auto width = static_cast<std::size_t>(*layer.projection.xsize);
  const auto height = static_cast<std::size_t>(*layer.projection.ysize);

  // Pixel centres in north-up order: the first row is the top one
  const auto& box = layer.projection.getBox();
  const double dx = (box.xmax() - box.xmin()) / static_cast<double>(width);
  const double dy = (box.ymax() - box.ymin()) / static_cast<double>(height);
  Fmi::CoordinateMatrix coords(width,
                               height,
                               box.xmin() + 0.5 * dx,
                               box.ymax() - 0.5 * dy,
                               box.xmax() - 0.5 * dx,
                               box.ymin() + 0.5 * dy);

  const auto crs = layer.projection.getCRS();
  if (!crs.isGeographic())
  {
    Fmi::CoordinateTransformation transformation(crs, "WGS84");
    coords.transform(transformation);
  }

  const auto values = q->values(coords, NFmiMetTime(layer.getValidTime()));

  const double multiplier = layer.multiplier.value_or(1.0);
  const double offset = layer.offset.value_or(0.0);
  std::vector<float> band(width * height, static_cast<float>(ParamValueMissing));
  for (std::size_t j = 0; j < height; j++)
    for (std::size_t i = 0; i < width; i++)
    {
      const float v = values[i][j];
      if (v != kFloatMissing && std::isfinite(v))
        band[j * width + i] = static_cast<float>(multiplier * v + offset);
    }

  return writeGeoTiffBands(layer.projection, crs.WKT(), {band});
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Query a single grid parameter and write it as a single-band GeoTiff.
 *
 * Layers with the grid source use the grid engine, other layers querydata.
 */
// ----------------------------------------------------------------------

std::string gridDataGeoTiff(Layer& layer,
                             const std::string& parameterName,
                             const std::string& interpolation,
                            const State& state)
{
  try
  {
    if (parameterName.empty())
      throw Fmi::Exception(BCP, "Parameter not set for GeoTiff generation");

    if (!layer.projection.crs || *layer.projection.crs == "data")
      throw Fmi::Exception(BCP,
                           "GeoTiff output requires an explicit CRS (crs=data is not supported)");

    if (layer.paraminfo.source != std::string("grid"))
      return querydataGeoTiff(layer, parameterName, state);

    const auto* gridEngine = state.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "GeoTiff output requires the grid engine to be enabled");

    if (!layer.paraminfo.producer)
      throw Fmi::Exception(BCP, "Producer not set for GeoTiff generation");

    // ---- Build the grid query ----

    auto originalGridQuery = std::make_shared<QueryServer::Query>();
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*layer.paraminfo.producer);
    auto crs_ref = layer.projection.getCRS();
    const auto& box = layer.projection.getBox();
    std::string wkt = *layer.projection.crs;

    if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
      wkt = crs_ref.WKT();

    auto bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
    auto bl = layer.projection.bottomLeftLatLon();
    auto tr = layer.projection.topRightLatLon();

    if (layer.projection.x1 == bl.X() && layer.projection.y1 == bl.Y() &&
        layer.projection.x2 == tr.X() && layer.projection.y2 == tr.Y())
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

    originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);

    // Parameter with optional unit conversion
    add_grid_parameter(*gridEngine,
                       producerName,
                       parameterName,
                       layer.multiplier,
                       layer.offset,
                       layer.projection,
                       attributeList,
                       *originalGridQuery);

    // Time
    add_time_attributes(attributeList, layer.getValidTime(), layer.origintime);

    queryConfigurator.configure(*originalGridQuery, attributeList);

    for (auto& p : originalGridQuery->mQueryParameterList)
    {
      p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      p.mType = QueryServer::QueryParameter::Type::Vector;
      p.mFlags |= QueryServer::QueryParameter::Flags::ReturnCoordinates;

      set_vector_interpolation(p, interpolation);
      set_query_parameter_selections(p, layer.paraminfo);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    add_grid_size_attributes(*originalGridQuery, layer.projection);

    if (layer.projection.bboxcrs)
      originalGridQuery->mAttributeList.addAttribute("grid.bboxcrs", *layer.projection.bboxcrs);

    // Execute
    auto query = gridEngine->executeQuery(originalGridQuery);

    // Update projection dimensions from result if needed
    update_projection_size(layer.projection, *query);

    if (!layer.projection.xsize || !layer.projection.ysize)
      throw Fmi::Exception(BCP, "Grid size is unknown after query");

    // Extract the first non-empty value set
    const T::Coordinate_vec* coordinates = nullptr;
    auto pval = first_value_vector(*query, coordinates);

    if (!pval || pval->mValueVector.empty())
      throw Fmi::Exception(BCP, "No data returned for GeoTiff generation");

    const int width = *layer.projection.xsize;
    const int height = *layer.projection.ysize;
    const auto& values = pval->mValueVector;

    if (static_cast<int>(values.size()) != width * height)
      throw Fmi::Exception(BCP,
                           "Value vector size mismatch: got " + Fmi::to_string(values.size()) +
                               " expected " + Fmi::to_string(width * height));

    // Normalise to north-up row order
    std::vector<float> ordered = north_up_values(values, coordinates, width, height);

    return writeGeoTiffBands(layer.projection, wkt, {ordered});
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "gridDataGeoTiff failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
