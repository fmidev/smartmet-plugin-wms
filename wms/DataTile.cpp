// ======================================================================

#include "DataTile.h"
#include "Layer.h"
#include "State.h"
#include <cmath>
#include <cstring>
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/grid/Typedefs.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <png.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// ------------------------------------------------------------------
// libpng in-memory write callback
// ------------------------------------------------------------------

struct PngBuffer
{
  std::string data;
};

void pngWriteCallback(png_structp png, png_bytep buf, png_size_t len)
{
  auto* out = static_cast<PngBuffer*>(png_get_io_ptr(png));
  out->data.append(reinterpret_cast<const char*>(buf), len);
}

void pngFlushCallback(png_structp /*png*/) {}

// ------------------------------------------------------------------
// Write an RGBA pixel buffer as PNG with optional tEXt metadata
// ------------------------------------------------------------------

struct TextEntry
{
  std::string key;
  std::string value;
};

std::string writePng(int width,
                     int height,
                     const std::vector<uint8_t>& pixels,
                     const std::vector<TextEntry>& text)
{
  PngBuffer buf;

  auto* png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png)
    throw Fmi::Exception(BCP, "png_create_write_struct failed");

  auto* info = png_create_info_struct(png);
  if (!info)
  {
    png_destroy_write_struct(&png, nullptr);
    throw Fmi::Exception(BCP, "png_create_info_struct failed");
  }

  if (setjmp(png_jmpbuf(png)))
  {
    png_destroy_write_struct(&png, &info);
    throw Fmi::Exception(BCP, "libpng error during write");
  }

  png_set_write_fn(png, &buf, pngWriteCallback, pngFlushCallback);

  png_set_IHDR(png,
               info,
               width,
               height,
               8,
               PNG_COLOR_TYPE_RGBA,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_set_compression_level(png, 6);

  // Add tEXt metadata chunks
  if (!text.empty())
  {
    std::vector<png_text> chunks(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
      std::memset(&chunks[i], 0, sizeof(png_text));
      chunks[i].compression = PNG_TEXT_COMPRESSION_NONE;
      chunks[i].key = const_cast<char*>(text[i].key.c_str());
      chunks[i].text = const_cast<char*>(text[i].value.c_str());
      chunks[i].text_length = text[i].value.size();
    }
    png_set_text(png, info, chunks.data(), static_cast<int>(chunks.size()));
  }

  png_write_info(png, info);

  // Write row by row
  for (int y = 0; y < height; ++y)
  {
    auto* row = const_cast<uint8_t*>(pixels.data() + y * width * 4);
    png_write_row(png, row);
  }

  png_write_end(png, info);
  png_destroy_write_struct(&png, &info);

  return std::move(buf.data);
}

// Missing-value sentinel from grid-files
const float nodata = static_cast<float>(ParamValueMissing);

bool isMissing(float v)
{
  return v == nodata || std::isnan(v);
}

}  // anonymous namespace

// ======================================================================
// Single-band datatile
// ======================================================================

std::string writeSingleBandDataTile(int width,
                                     int height,
                                     const std::vector<float>& values)
{
  try
  {
    const int sz = width * height;

    // Find min/max of valid values
    float vmin = std::numeric_limits<float>::max();
    float vmax = std::numeric_limits<float>::lowest();
    for (int i = 0; i < sz; ++i)
    {
      if (!isMissing(values[i]))
      {
        vmin = std::min(vmin, values[i]);
        vmax = std::max(vmax, values[i]);
      }
    }

    // Handle edge case: all missing or constant field
    if (vmin > vmax)
    {
      vmin = 0;
      vmax = 1;
    }
    else if (vmin == vmax)
    {
      vmax = vmin + 1;
    }

    const double range = vmax - vmin;

    // Encode pixels: R=high, G=low, B=0, A=255 (valid) or A=0 (missing)
    std::vector<uint8_t> pixels(sz * 4);
    for (int i = 0; i < sz; ++i)
    {
      const int p = i * 4;
      if (isMissing(values[i]))
      {
        pixels[p + 0] = 0;
        pixels[p + 1] = 0;
        pixels[p + 2] = 0;
        pixels[p + 3] = 0;
      }
      else
      {
        double norm = (values[i] - vmin) / range;
        norm = std::max(0.0, std::min(1.0, norm));
        auto q = static_cast<unsigned int>(std::round(norm * 65535.0));
        pixels[p + 0] = static_cast<uint8_t>(q >> 8);
        pixels[p + 1] = static_cast<uint8_t>(q & 0xFF);
        pixels[p + 2] = 0;
        pixels[p + 3] = 255;
      }
    }

    std::vector<TextEntry> text = {{"datatile:bands", "1"},
                                   {"datatile:min", fmt::format("{:.8g}", vmin)},
                                   {"datatile:max", fmt::format("{:.8g}", vmax)},
                                   {"datatile:encoding", "uint16"}};

    return writePng(width, height, pixels, text);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "writeSingleBandDataTile failed!");
  }
}

// ======================================================================
// Dual-band datatile
// ======================================================================

std::string writeDualBandDataTile(int width,
                                   int height,
                                   const std::vector<float>& values1,
                                   const std::vector<float>& values2)
{
  try
  {
    const int sz = width * height;

    // Find min/max for each band independently
    float min1 = std::numeric_limits<float>::max();
    float max1 = std::numeric_limits<float>::lowest();
    float min2 = std::numeric_limits<float>::max();
    float max2 = std::numeric_limits<float>::lowest();

    for (int i = 0; i < sz; ++i)
    {
      if (!isMissing(values1[i]) && !isMissing(values2[i]))
      {
        min1 = std::min(min1, values1[i]);
        max1 = std::max(max1, values1[i]);
        min2 = std::min(min2, values2[i]);
        max2 = std::max(max2, values2[i]);
      }
    }

    if (min1 > max1)
    {
      min1 = 0;
      max1 = 1;
    }
    else if (min1 == max1)
      max1 = min1 + 1;

    if (min2 > max2)
    {
      min2 = 0;
      max2 = 1;
    }
    else if (min2 == max2)
      max2 = min2 + 1;

    const double range1 = max1 - min1;
    const double range2 = max2 - min2;

    // Encode: R=high(band1), G=low(band1), B=high(band2), A=low(band2)
    // [1..65535] for valid, 0 = missing sentinel
    std::vector<uint8_t> pixels(sz * 4);
    for (int i = 0; i < sz; ++i)
    {
      const int p = i * 4;
      if (isMissing(values1[i]) || isMissing(values2[i]))
      {
        pixels[p + 0] = 0;
        pixels[p + 1] = 0;
        pixels[p + 2] = 0;
        pixels[p + 3] = 0;
      }
      else
      {
        double n1 = (values1[i] - min1) / range1;
        n1 = std::max(0.0, std::min(1.0, n1));
        auto q1 = static_cast<unsigned int>(1 + std::round(n1 * 65534.0));

        double n2 = (values2[i] - min2) / range2;
        n2 = std::max(0.0, std::min(1.0, n2));
        auto q2 = static_cast<unsigned int>(1 + std::round(n2 * 65534.0));

        pixels[p + 0] = static_cast<uint8_t>(q1 >> 8);
        pixels[p + 1] = static_cast<uint8_t>(q1 & 0xFF);
        pixels[p + 2] = static_cast<uint8_t>(q2 >> 8);
        pixels[p + 3] = static_cast<uint8_t>(q2 & 0xFF);
      }
    }

    std::vector<TextEntry> text = {{"datatile:bands", "2"},
                                   {"datatile:min1", fmt::format("{:.8g}", min1)},
                                   {"datatile:max1", fmt::format("{:.8g}", max1)},
                                   {"datatile:min2", fmt::format("{:.8g}", min2)},
                                   {"datatile:max2", fmt::format("{:.8g}", max2)},
                                   {"datatile:encoding", "uint16"}};

    return writePng(width, height, pixels, text);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "writeDualBandDataTile failed!");
  }
}

// ======================================================================
// Query grid engine for a single scalar parameter and return datatile
// PNG bytes.  This mirrors gridDataGeoTiff() in GridDataGeoTiff.cpp.
// ======================================================================

std::string gridDataTile(Layer& layer,
                          const std::string& parameterName,
                          const std::string& interpolation,
                          State& state)
{
  try
  {
    const auto* gridEngine = state.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "Datatile output requires the grid engine to be enabled");

    if (parameterName.empty())
      throw Fmi::Exception(BCP, "Parameter not set for datatile generation");

    if (!layer.paraminfo.producer)
      throw Fmi::Exception(BCP, "Producer not set for datatile generation");

    if (!layer.projection.crs || *layer.projection.crs == "data")
      throw Fmi::Exception(
          BCP, "Datatile output requires an explicit CRS (crs=data is not supported)");

    // ---- Build the grid query (same as gridDataGeoTiff) ----

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
    {
      std::string pName = parameterName;
      auto pos = pName.find(".raw");
      if (pos != std::string::npos)
      {
        attributeList.addAttribute("grid.areaInterpolationMethod",
                                   Fmi::to_string(T::AreaInterpolationMethod::Nearest));
        pName.erase(pos, 4);
      }

      std::string param = gridEngine->getParameterString(producerName, pName);

      if (layer.multiplier && *layer.multiplier != 1.0)
        param = "MUL{" + param + ";" + std::to_string(*layer.multiplier) + "}";
      if (layer.offset && *layer.offset)
        param = "SUM{" + param + ";" + std::to_string(*layer.offset) + "}";

      attributeList.addAttribute("param", param);

      if (!layer.projection.projectionParameter)
        layer.projection.projectionParameter = param;

      if (param == parameterName && originalGridQuery->mProducerNameList.empty())
      {
        gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
        if (originalGridQuery->mProducerNameList.empty())
          originalGridQuery->mProducerNameList.push_back(producerName);
      }
    }

    // Time
    std::string forecastTime = Fmi::to_iso_string(layer.getValidTime());
    attributeList.addAttribute("startTime", forecastTime);
    attributeList.addAttribute("endTime", forecastTime);
    attributeList.addAttribute("timelist", forecastTime);
    attributeList.addAttribute("timezone", "UTC");
    if (layer.origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*layer.origintime));

    queryConfigurator.configure(*originalGridQuery, attributeList);

    for (auto& p : originalGridQuery->mQueryParameterList)
    {
      p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      p.mType = QueryServer::QueryParameter::Type::Vector;
      p.mFlags |= QueryServer::QueryParameter::Flags::ReturnCoordinates;

      if (interpolation == "nearest")
        p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Nearest;
      else if (interpolation == "linear" || interpolation == "transfer")
      {
        p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Linear;
        p.mTimeInterpolationMethod = T::TimeInterpolationMethod::Linear;
        p.mLevelInterpolationMethod = T::LevelInterpolationMethod::Linear;
      }

      if (layer.paraminfo.geometryId)
        p.mGeometryId = *layer.paraminfo.geometryId;
      if (layer.paraminfo.levelId)
        p.mParameterLevelId = *layer.paraminfo.levelId;
      if (layer.paraminfo.level)
        p.mParameterLevel = C_INT(*layer.paraminfo.level);
      else if (layer.paraminfo.pressure)
      {
        p.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
        p.mParameterLevel = C_INT(*layer.paraminfo.pressure);
      }
      if (layer.paraminfo.elevation_unit)
      {
        if (*layer.paraminfo.elevation_unit == "m")
          p.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;
        if (*layer.paraminfo.elevation_unit == "p")
          p.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
      }
      if (layer.paraminfo.forecastType)
        p.mForecastType = C_INT(*layer.paraminfo.forecastType);
      if (layer.paraminfo.forecastNumber)
        p.mForecastNumber = C_INT(*layer.paraminfo.forecastNumber);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    if (layer.projection.size && *layer.projection.size > 0)
      originalGridQuery->mAttributeList.addAttribute("grid.size",
                                                      Fmi::to_string(*layer.projection.size));
    else
    {
      if (layer.projection.xsize)
        originalGridQuery->mAttributeList.addAttribute("grid.width",
                                                        Fmi::to_string(*layer.projection.xsize));
      if (layer.projection.ysize)
        originalGridQuery->mAttributeList.addAttribute("grid.height",
                                                        Fmi::to_string(*layer.projection.ysize));
    }

    if (layer.projection.bboxcrs)
      originalGridQuery->mAttributeList.addAttribute("grid.bboxcrs", *layer.projection.bboxcrs);

    // Execute
    auto query = gridEngine->executeQuery(originalGridQuery);

    // Update projection dimensions from result if needed
    if ((layer.projection.size && *layer.projection.size > 0) ||
        (!layer.projection.xsize && !layer.projection.ysize))
    {
      const char* widthStr = query->mAttributeList.getAttributeValue("grid.width");
      const char* heightStr = query->mAttributeList.getAttributeValue("grid.height");
      if (widthStr)
        layer.projection.xsize = Fmi::stoi(widthStr);
      if (heightStr)
        layer.projection.ysize = Fmi::stoi(heightStr);
    }

    if (!layer.projection.xsize || !layer.projection.ysize)
      throw Fmi::Exception(BCP, "Grid size is unknown after query");

    // Extract the first non-empty value set
    std::shared_ptr<QueryServer::ParameterValues> pval;
    const T::Coordinate_vec* coordinates = nullptr;

    for (const auto& qp : query->mQueryParameterList)
    {
      for (const auto& val : qp.mValueList)
      {
        if (!val->mValueVector.empty())
        {
          pval = val;
          if (!qp.mCoordinates.empty())
            coordinates = &qp.mCoordinates;
          break;
        }
      }
      if (pval)
        break;
    }

    if (!pval || pval->mValueVector.empty())
      throw Fmi::Exception(BCP, "No data returned for datatile generation");

    const int width = *layer.projection.xsize;
    const int height = *layer.projection.ysize;
    const auto& values = pval->mValueVector;

    if (static_cast<int>(values.size()) != width * height)
      throw Fmi::Exception(BCP,
                           "Value vector size mismatch: got " + Fmi::to_string(values.size()) +
                               " expected " + Fmi::to_string(width * height));

    // Detect row order: if first-row Y < row-10 Y, data is south-up (needs flip)
    const bool south_up = (coordinates &&
                           static_cast<int>(coordinates->size()) > 10 * width &&
                           (*coordinates)[0].y() < (*coordinates)[10 * width].y());

    // Normalise to north-up row order
    std::vector<float> ordered;
    if (south_up)
    {
      ordered.resize(values.size());
      for (int row = 0; row < height; ++row)
        for (int col = 0; col < width; ++col)
          ordered[row * width + col] = values[(height - 1 - row) * width + col];
    }
    else
    {
      ordered = values;
    }

    return writeSingleBandDataTile(width, height, ordered);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "gridDataTile failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
