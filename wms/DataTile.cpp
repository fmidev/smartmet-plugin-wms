// ======================================================================

#include "DataTile.h"
#include "Layer.h"
#include "State.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/grid/Typedefs.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <boost/crc.hpp>
#include <giza/Giza.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// ------------------------------------------------------------------
// Write an RGBA pixel buffer as PNG with optional tEXt metadata
// ------------------------------------------------------------------

struct TextEntry
{
  std::string key;
  std::string value;
};

// Append a PNG chunk: length, type, data, CRC-32 over type + data.
void appendChunk(std::string& out, const char* type, const std::string& data)
{
  auto be32 = [&out](std::uint32_t v)
  {
    out.push_back(static_cast<char>(v >> 24));
    out.push_back(static_cast<char>(v >> 16));
    out.push_back(static_cast<char>(v >> 8));
    out.push_back(static_cast<char>(v));
  };
  be32(static_cast<std::uint32_t>(data.size()));
  boost::crc_32_type crc;
  crc.process_bytes(type, 4);
  crc.process_bytes(data.data(), data.size());
  out.append(type, 4);
  out.append(data);
  be32(crc.checksum());
}

// The pixels are data, not a picture: they are written byte for byte (no
// colour reduction, straight alpha) by Giza::topng_argb, which compresses with
// libdeflate. libpng's streaming zlib at level 6 spent most of its time in
// deflate_slow: a 1024x1024 dual-band wind tile took 192 ms and 2.49 MB,
// libdeflate level 1 takes 39 ms and 2.25 MB (a single-band precipitation
// tile 59 ms -> 15 ms, 0.68 -> 0.77 MB).
std::string writePng(int width,
                     int height,
                     const std::vector<uint8_t>& pixels,
                     const std::vector<TextEntry>& text)
{
  const std::size_t n = static_cast<std::size_t>(width) * height;
  std::vector<std::uint32_t> argb(n);
  for (std::size_t i = 0; i < n; ++i)
  {
    const auto* p = &pixels[i * 4];
    argb[i] = (static_cast<std::uint32_t>(p[3]) << 24) | (static_cast<std::uint32_t>(p[0]) << 16) |
              (static_cast<std::uint32_t>(p[1]) << 8) | p[2];
  }

  std::string png = Giza::topng_argb(argb.data(), width, height, 1);

  if (text.empty())
    return png;

  // Splice the tEXt chunks in right after IHDR (8-byte signature + 25-byte
  // IHDR chunk), where readers expect ancillary metadata before the pixels.
  const std::size_t ihdr_end = 8 + 25;
  if (png.size() < ihdr_end || png.compare(12, 4, "IHDR") != 0)
    throw Fmi::Exception(BCP, "Unexpected PNG layout from Giza::topng_argb");

  std::string chunks;
  for (const auto& entry : text)
    appendChunk(chunks, "tEXt", entry.key + '\0' + entry.value);

  png.insert(ihdr_end, chunks);
  return png;
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
                                     const std::vector<float>& values,
                                     const std::string& parameter)
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
    if (!parameter.empty())
      text.push_back({"datatile:parameter", parameter});

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
                                   const std::vector<float>& values2,
                                   const std::string& components)
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
    if (!components.empty())
      text.push_back({"datatile:components", components});

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

    return writeSingleBandDataTile(width, height, ordered, parameterName);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "gridDataTile failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
