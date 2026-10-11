#include "ApiDefinition.h"
#include "Config.h"
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <algorithm>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{
namespace
{
namespace OA = Spine::OpenAPI;

const char* const TAG_CAPABILITIES = "Capabilities";
const char* const TAG_TILES = "Tiles";

// Tile file name extensions and their media types, see extensionToMimeType in Handler.cpp
const std::vector<std::pair<std::string, std::string>> tile_formats{
    {"png", "image/png"},
    {"webp", "image/webp"},
    {"svg", "image/svg+xml"},
    {"tiff", "image/tiff"},
    {"tif", "image/tiff"},
    {"mvt", "application/vnd.mapbox-vector-tile"},
    {"pbf", "application/vnd.mapbox-vector-tile"},
    {"datatile", "application/x-datatile+png"}};

Json::Value integer_schema(int minimum)
{
  Json::Value ret;
  ret["type"] = "integer";
  ret["minimum"] = minimum;
  return ret;
}

Json::Value fmi_extension(Json::Value parameter)
{
  parameter["x-fmi-extension"] = true;
  return parameter;
}

Json::Value exception_response(const std::string& description)
{
  Json::Value ret;
  ret["description"] = description + " An OWS ExceptionReport.";
  ret["content"]["application/xml"]["schema"] = OA::stringSchema();
  return ret;
}

void add_components(OA::Document& doc,
                    const Config& config,
                    const std::vector<OGC::OpenApiLayer>& layers,
                    std::size_t maxEnum)
{
  using OA::parameter;
  using OA::stringSchema;

  std::vector<std::string> tms_ids;
  std::set<std::string> tm_ids;
  for (const auto& tms : config.tileMatrixSets())
  {
    tms_ids.push_back(tms.identifier);
    for (const auto& tm : tms.tile_matrices)
      tm_ids.insert(tm.identifier);
  }

  std::set<std::string> styles;
  for (const auto& layer : layers)
    styles.insert(layer.styles.begin(), layer.styles.end());

  doc.addComponent("parameters",
                   "Style",
                   parameter("Style",
                             "path",
                             "Style of the layer, see the capabilities",
                             OA::enumSchema({styles.begin(), styles.end()}, maxEnum)));
  doc.addComponent("parameters",
                   "TileMatrixSet",
                   parameter("TileMatrixSet",
                             "path",
                             "Identifier of a tile matrix set",
                             OA::enumSchema(tms_ids, maxEnum)));
  doc.addComponent("parameters",
                   "TileMatrix",
                   parameter("TileMatrix",
                             "path",
                             "Identifier of a tile matrix (zoom level) of the tile matrix set",
                             OA::enumSchema({tm_ids.begin(), tm_ids.end()}, maxEnum)));
  doc.addComponent("parameters",
                   "TileRow",
                   parameter("TileRow", "path", "Row index of the tile", integer_schema(0)));
  doc.addComponent("parameters",
                   "TileCol",
                   parameter("TileCol", "path", "Column index of the tile", integer_schema(0)));
  doc.addComponent(
      "parameters", "J", parameter("J", "path", "Pixel row within the tile", integer_schema(0)));
  doc.addComponent(
      "parameters", "I", parameter("I", "path", "Pixel column within the tile", integer_schema(0)));

  std::vector<std::string> extensions;
  for (const auto& format : tile_formats)
    extensions.push_back(format.first);
  doc.addComponent("parameters",
                   "format",
                   parameter("format", "path", "Tile format", OA::enumSchema(extensions, maxEnum)));
  doc.addComponent("parameters",
                   "infoFormat",
                   parameter("infoFormat",
                             "path",
                             "Format of the feature info",
                             OA::enumSchema({"json", "html"}, maxEnum)));

  doc.addComponent("parameters",
                   "TIME",
                   fmi_extension(parameter("TIME",
                                           "query",
                                           "Time of the data when the URL has no Time segment, "
                                           "or 'current'",
                                           stringSchema())));
  doc.addComponent("parameters",
                   "WIDTH",
                   fmi_extension(parameter("WIDTH",
                                           "query",
                                           "Width of the tile in pixels, the tile matrix tile "
                                           "width by default",
                                           integer_schema(1))));
  doc.addComponent("parameters",
                   "HEIGHT",
                   fmi_extension(parameter("HEIGHT",
                                           "query",
                                           "Height of the tile in pixels, the tile matrix tile "
                                           "height by default",
                                           integer_schema(1))));
  doc.addComponent(
      "parameters",
      "LANGUAGE",
      fmi_extension(
          parameter("LANGUAGE", "query", "Language of the titles and abstracts", stringSchema())));

  Json::Value tile;
  tile["description"] = "The tile";
  std::set<std::string> media_types;
  for (const auto& format : tile_formats)
    media_types.insert(format.second);
  for (const auto& media_type : media_types)
  {
    Json::Value binary = stringSchema();
    if (media_type != "image/svg+xml")
      binary["format"] = "binary";
    tile["content"][media_type]["schema"] = binary;
  }
  doc.addComponent("responses", "Tile", tile);

  Json::Value info;
  info["description"] = "Values of the data at the pixel";
  info["content"]["application/json"]["schema"]["type"] = "object";
  info["content"]["text/html"]["schema"] = stringSchema();
  doc.addComponent("responses", "FeatureInfo", info);

  doc.addComponent("responses", "Exception", exception_response("The request failed."));
}

void add_errors(Json::Value& op)
{
  op["responses"]["400"] = OA::ref("responses", "Exception");
  op["responses"]["404"] = OA::ref("responses", "Exception");
  op["responses"]["500"] = OA::ref("responses", "Exception");
}

}  // namespace

Spine::OpenAPI::Document apiDefinition(const Config& config,
                                       const std::vector<OGC::OpenApiLayer>& layers,
                                       const Spine::OpenAPI::Settings& settings)
{
  try
  {
    const auto max_enum = settings.max_enum;

    OA::Document doc("SmartMet WMTS",
                     "The RESTful interface of the Web Map Tile Service (WMTS 1.0.0) of the WMS "
                     "layers. The capabilities document lists the layers, styles, tile matrix "
                     "sets and dimensions.",
                     "1.0.0",
                     settings);
    doc.addTag(TAG_CAPABILITIES, "The capabilities and the API definition");
    doc.addTag(TAG_TILES, "Tiles and feature info");
    doc.json()["externalDocs"]["description"] = "OGC Web Map Tile Service 1.0.0 (07-057r7)";
    doc.json()["externalDocs"]["url"] = "https://www.ogc.org/standard/wmts/";

    doc.addCommonComponents();
    add_components(doc, config, layers, max_enum);

    auto& caps = doc.operation("/1.0.0/WMTSCapabilities.xml", "get");
    caps["tags"].append(TAG_CAPABILITIES);
    caps["summary"] = "Capabilities";
    caps["operationId"] = "getCapabilities";
    caps["parameters"].append(OA::ref("parameters", "LANGUAGE"));
    caps["responses"]["200"]["description"] = "The WMTS capabilities document";
    caps["responses"]["200"]["content"]["application/xml"]["schema"] = OA::stringSchema();
    caps["externalDocs"]["description"] = "WMTS capabilities schema";
    caps["externalDocs"]["url"] =
        "http://schemas.opengis.net/wmts/1.0/wmtsGetCapabilities_response.xsd";
    add_errors(caps);

    doc.addApiOperations(TAG_CAPABILITIES);

    // The dimension values follow the style in the order of the ResourceURL template of the
    // layer. Trailing dimensions may be left out, so there is a path for each number of
    // dimension segments. The paths would differ only by their parameter names if the
    // dimensions were named, which OpenAPI does not allow, hence the positional names.
    std::size_t max_dims = 0;
    for (const auto& layer : layers)
      max_dims = std::max(max_dims, layer.dimensions.size());

    for (std::size_t n = 0; n <= max_dims; n++)
    {
      // The layers with at least n dimensions, and the dimensions at each position
      std::vector<std::string> ids;
      std::vector<std::set<std::string>> dimension_names(n);
      for (const auto& layer : layers)
        if (layer.dimensions.size() >= n)
        {
          ids.push_back(layer.id);
          for (std::size_t i = 0; i < n; i++)
            dimension_names[i].insert(layer.dimensions[i]);
        }

      Json::Value params(Json::arrayValue);
      params.append(OA::parameter(
          "layer",
          "path",
          n == 0 ? "Identifier of the layer"
                 : fmt::format("Identifier of a layer with at least {} dimensions", n),
          OA::enumSchema(ids, max_enum)));
      params.append(OA::ref("parameters", "Style"));

      std::string dim_path;
      for (std::size_t i = 0; i < n; i++)
      {
        const auto name = fmt::format("dim{}", i + 1);
        dim_path += "/{" + name + "}";
        std::string names;
        for (const auto& dim : dimension_names[i])
          names += (names.empty() ? "" : " or ") + dim;
        params.append(OA::parameter(
            name,
            "path",
            fmt::format("Value of dimension {} of the layer ({}), in the order of the "
                        "ResourceURL template of the capabilities",
                        i + 1,
                        names),
            OA::stringSchema()));
      }

      const std::string base =
          "/1.0.0/{layer}/{Style}" + dim_path + "/{TileMatrixSet}/{TileMatrix}/{TileRow}";
      const std::string suffix = (n == 0 ? "" : fmt::format("_{}", n));

      auto tile_params = params;
      for (const char* name : {"TileMatrixSet", "TileMatrix", "TileRow", "TileCol", "format"})
        tile_params.append(OA::ref("parameters", name));
      for (const char* name : {"TIME", "WIDTH", "HEIGHT"})
        tile_params.append(OA::ref("parameters", name));

      auto& tile = doc.operation(base + "/{TileCol}.{format}", "get");
      tile["tags"].append(TAG_TILES);
      tile["summary"] = n == 0 ? "A tile" : fmt::format("A tile, {} dimension values", n);
      tile["operationId"] = "getTile" + suffix;
      tile["parameters"] = tile_params;
      tile["responses"]["200"] = OA::ref("responses", "Tile");
      add_errors(tile);

      auto info_params = params;
      for (const char* name :
           {"TileMatrixSet", "TileMatrix", "TileRow", "TileCol", "J", "I", "infoFormat"})
        info_params.append(OA::ref("parameters", name));
      info_params.append(OA::ref("parameters", "TIME"));

      auto& info = doc.operation(base + "/{TileCol}/{J}/{I}.{infoFormat}", "get");
      info["tags"].append(TAG_TILES);
      info["summary"] =
          n == 0 ? "Feature info" : fmt::format("Feature info, {} dimension values", n);
      info["operationId"] = "getFeatureInfo" + suffix;
      info["parameters"] = info_params;
      info["responses"]["200"] = OA::ref("responses", "FeatureInfo");
      add_errors(info);
    }

    return doc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate the WMTS definition");
  }
}

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
