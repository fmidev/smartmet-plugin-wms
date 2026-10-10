#include "ApiDefinition.h"
#include "Config.h"
#include <macgyver/Exception.h>
#include <algorithm>
#include <cctype>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace Tiles
{
namespace
{
namespace OA = Spine::OpenAPI;

const char* const TAG_CAPABILITIES = "Capabilities";
const char* const TAG_TMS = "Tiling schemes";
const char* const TAG_COLLECTIONS = "Collections";
const char* const TAG_TILES = "Tiles";
const char* const TAG_STYLES = "Styles";

// Values of the 'f' option of the tile resource and the media types they select. The
// same names are accepted as media types, see extensionOrParamToMime in Handler.cpp.
const std::vector<std::pair<std::string, std::string>> tile_formats{
    {"png", "image/png"},
    {"webp", "image/webp"},
    {"svg", "image/svg+xml"},
    {"tiff", "image/tiff"},
    {"geotiff", "image/tiff"},
    {"mvt", "application/vnd.mapbox-vector-tile"},
    {"pbf", "application/vnd.mapbox-vector-tile"},
    {"datatile", "application/x-datatile+png"}};

std::string component_name(const std::string& name)
{
  std::string ret = name;
  for (auto& c : ret)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-')
      c = '_';
  return ret;
}

Json::Value object_schema()
{
  Json::Value ret;
  ret["type"] = "object";
  return ret;
}

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

Json::Value json_response(const std::string& description, const Json::Value& schema)
{
  Json::Value ret;
  ret["description"] = description;
  ret["content"]["application/json"]["schema"] = schema;
  return ret;
}

void add_schemas(OA::Document& doc)
{
  using OA::arraySchema;
  using OA::ref;
  using OA::stringSchema;

  Json::Value number;
  number["type"] = "number";

  // OGC API - Common Part 2 collection description
  Json::Value extent = object_schema();
  extent["properties"]["spatial"] = object_schema();
  extent["properties"]["spatial"]["properties"]["bbox"] = arraySchema(arraySchema(number));
  extent["properties"]["spatial"]["properties"]["crs"] = stringSchema();
  extent["properties"]["temporal"] = object_schema();
  extent["properties"]["temporal"]["properties"]["interval"] =
      arraySchema(arraySchema(stringSchema()));
  extent["properties"]["temporal"]["properties"]["values"] = arraySchema(stringSchema());
  extent["properties"]["vertical"] = object_schema();
  extent["properties"]["vertical"]["properties"]["interval"] =
      arraySchema(arraySchema(stringSchema()));
  extent["properties"]["vertical"]["properties"]["values"] = arraySchema(stringSchema());
  extent["properties"]["vertical"]["properties"]["vrs"] = stringSchema();

  Json::Value collection = object_schema();
  collection["required"].append("id");
  collection["required"].append("links");
  collection["properties"]["id"] = stringSchema();
  collection["properties"]["title"] = stringSchema();
  collection["properties"]["description"] = stringSchema();
  collection["properties"]["extent"] = extent;
  collection["properties"]["links"] = arraySchema(ref("schemas", "link"));
  doc.addComponent("schemas", "collectionDesc", collection);

  Json::Value collections = object_schema();
  collections["required"].append("links");
  collections["required"].append("collections");
  collections["properties"]["links"] = arraySchema(ref("schemas", "link"));
  collections["properties"]["collections"] = arraySchema(ref("schemas", "collectionDesc"));
  doc.addComponent("schemas", "collections", collections);

  // OGC Two Dimensional Tile Matrix Set 2.0
  Json::Value tile_matrix = object_schema();
  for (const char* name : {"id",
                           "scaleDenominator",
                           "cellSize",
                           "pointOfOrigin",
                           "tileWidth",
                           "tileHeight",
                           "matrixWidth",
                           "matrixHeight"})
    tile_matrix["required"].append(name);
  tile_matrix["properties"]["id"] = stringSchema();
  tile_matrix["properties"]["scaleDenominator"] = number;
  tile_matrix["properties"]["cellSize"] = number;
  tile_matrix["properties"]["pointOfOrigin"] = arraySchema(number);
  for (const char* name : {"tileWidth", "tileHeight", "matrixWidth", "matrixHeight"})
    tile_matrix["properties"][name] = integer_schema(1);

  Json::Value tms = object_schema();
  tms["required"].append("crs");
  tms["required"].append("tileMatrices");
  tms["properties"]["id"] = stringSchema();
  tms["properties"]["title"] = stringSchema();
  tms["properties"]["uri"] = stringSchema();
  tms["properties"]["crs"] = stringSchema();
  tms["properties"]["tileMatrices"] = arraySchema(tile_matrix);
  doc.addComponent("schemas", "tileMatrixSet", tms);

  Json::Value tms_item = object_schema();
  tms_item["required"].append("links");
  tms_item["properties"]["id"] = stringSchema();
  tms_item["properties"]["title"] = stringSchema();
  tms_item["properties"]["uri"] = stringSchema();
  tms_item["properties"]["crs"] = stringSchema();
  tms_item["properties"]["links"] = arraySchema(ref("schemas", "link"));
  Json::Value tms_list = object_schema();
  tms_list["required"].append("tileMatrixSets");
  tms_list["properties"]["tileMatrixSets"] = arraySchema(tms_item);
  doc.addComponent("schemas", "tileMatrixSets", tms_list);

  // OGC API - Tiles tileset metadata
  Json::Value tileset = object_schema();
  tileset["required"].append("dataType");
  tileset["required"].append("crs");
  tileset["required"].append("links");
  tileset["properties"]["title"] = stringSchema();
  tileset["properties"]["dataType"] = OA::enumSchema({"map", "vector", "coverage"}, 10);
  tileset["properties"]["crs"] = stringSchema();
  tileset["properties"]["tileMatrixSetURI"] = stringSchema();
  tileset["properties"]["tileMatrixSetLimits"] = arraySchema(object_schema());
  tileset["properties"]["links"] = arraySchema(ref("schemas", "link"));
  doc.addComponent("schemas", "tileSet", tileset);

  Json::Value tilesets = object_schema();
  tilesets["required"].append("tilesets");
  tilesets["properties"]["tilesets"] = arraySchema(ref("schemas", "tileSet"));
  tilesets["properties"]["links"] = arraySchema(ref("schemas", "link"));
  doc.addComponent("schemas", "tileSets", tilesets);

  // OGC API - Styles
  Json::Value style = object_schema();
  style["required"].append("id");
  style["properties"]["id"] = stringSchema();
  style["properties"]["title"] = stringSchema();
  style["properties"]["links"] = arraySchema(ref("schemas", "link"));
  Json::Value styles = object_schema();
  styles["required"].append("styles");
  styles["properties"]["styles"] = arraySchema(style);
  styles["properties"]["links"] = arraySchema(ref("schemas", "link"));
  doc.addComponent("schemas", "styleSet", styles);

  Json::Value mapbox = object_schema();
  mapbox["required"].append("version");
  mapbox["required"].append("layers");
  mapbox["properties"]["version"]["type"] = "integer";
  mapbox["properties"]["sources"] = object_schema();
  mapbox["properties"]["layers"] = arraySchema(object_schema());
  mapbox["description"] = "Mapbox GL style";
  doc.addComponent("schemas", "mapboxStyle", mapbox);

  // Tile responses
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
  info["content"]["application/json"]["schema"] = object_schema();
  info["content"]["text/html"]["schema"] = stringSchema();
  doc.addComponent("responses", "FeatureInfo", info);
}

void add_parameters(OA::Document& doc,
                    const Config& config,
                    const std::vector<OGC::OpenApiLayer>& layers,
                    std::size_t maxEnum)
{
  using OA::parameter;
  using OA::stringSchema;

  std::vector<std::string> ids;
  for (const auto& layer : layers)
    ids.push_back(layer.id);

  std::vector<std::string> tms_ids;
  std::set<std::string> tm_ids;
  for (const auto& tms : config.tileMatrixSets())
  {
    tms_ids.push_back(tms.identifier);
    for (const auto& tm : tms.tile_matrices)
      tm_ids.insert(tm.identifier);
  }

  doc.addComponent("parameters",
                   "collectionId",
                   parameter("collectionId",
                             "path",
                             "Identifier of a collection (layer), see /collections",
                             OA::enumSchema(ids, maxEnum, "collections")));
  doc.addComponent("parameters",
                   "tileMatrixSetId",
                   parameter("tileMatrixSetId",
                             "path",
                             "Identifier of a tile matrix set, see /tileMatrixSets",
                             OA::enumSchema(tms_ids, maxEnum, "tileMatrixSets")));
  doc.addComponent("parameters",
                   "tileMatrix",
                   parameter("tileMatrix",
                             "path",
                             "Identifier of a tile matrix (zoom level) of the tile matrix set",
                             OA::enumSchema({tm_ids.begin(), tm_ids.end()}, maxEnum)));
  doc.addComponent("parameters",
                   "tileRow",
                   parameter("tileRow", "path", "Row index of the tile", integer_schema(0)));
  doc.addComponent("parameters",
                   "tileCol",
                   parameter("tileCol", "path", "Column index of the tile", integer_schema(0)));
  doc.addComponent(
      "parameters", "i", parameter("i", "path", "Pixel column within the tile", integer_schema(0)));
  doc.addComponent(
      "parameters", "j", parameter("j", "path", "Pixel row within the tile", integer_schema(0)));
  doc.addComponent("parameters",
                   "styleId",
                   parameter("styleId",
                             "path",
                             "Identifier of a style, see /collections/{collectionId}/styles",
                             stringSchema()));

  std::vector<std::string> formats;
  for (const auto& format : tile_formats)
    formats.push_back(format.first);
  std::set<std::string> media_types;
  for (const auto& format : tile_formats)
    media_types.insert(format.second);
  formats.insert(formats.end(), media_types.begin(), media_types.end());

  doc.addComponent(
      "parameters",
      "f.tile",
      parameter("f",
                "query",
                "Tile format as a name or a media type. Without f the format is selected with the "
                "Accept header, png by default.",
                OA::enumSchema(formats, maxEnum)));
  doc.addComponent(
      "parameters",
      "f.info",
      parameter("f",
                "query",
                "Format of the feature info, json by default",
                OA::enumSchema({"json", "html", "application/json", "text/html"}, maxEnum)));
  doc.addComponent("parameters",
                   "f.json",
                   parameter("f", "query", "Format of the response", OA::enumSchema({"json"}, 1)));
  doc.addComponent(
      "parameters",
      "f.style",
      parameter("f", "query", "Style encoding, Mapbox GL style", OA::enumSchema({"mapbox"}, 1)));

  doc.addComponent("parameters",
                   "datetime",
                   parameter("datetime",
                             "query",
                             "Time of the data (RFC 3339), see the temporal extent of the "
                             "collection. The default is the time closest to the current time.",
                             stringSchema()));
  doc.addComponent("parameters",
                   "TIME",
                   fmi_extension(parameter(
                       "TIME", "query", "Alias of datetime, as in WMS and WMTS", stringSchema())));
  doc.addComponent("parameters",
                   "elevation",
                   parameter("elevation",
                             "query",
                             "Vertical level, see the vertical extent of the collection",
                             stringSchema()));
  doc.addComponent("parameters",
                   "reference_time",
                   parameter("reference_time",
                             "query",
                             "Origin time of the model run, the latest by default",
                             stringSchema()));
  doc.addComponent("parameters",
                   "style",
                   parameter("style",
                             "query",
                             "Style of the tile, see /collections/{collectionId}/styles",
                             stringSchema()));
  doc.addComponent("parameters",
                   "width",
                   fmi_extension(parameter("width",
                                           "query",
                                           "Width of the tile in pixels, the tile matrix "
                                           "tile width by default",
                                           integer_schema(1))));
  doc.addComponent("parameters",
                   "height",
                   fmi_extension(parameter("height",
                                           "query",
                                           "Height of the tile in pixels, the tile matrix "
                                           "tile height by default",
                                           integer_schema(1))));
  doc.addComponent(
      "parameters",
      "language",
      fmi_extension(parameter(
          "language", "query", "Language of the titles and descriptions", stringSchema())));
}

Json::Value refs(std::initializer_list<const char*> names)
{
  Json::Value ret(Json::arrayValue);
  for (const auto* name : names)
    ret.append(OA::ref("parameters", name));
  return ret;
}

Json::Value& add_operation(OA::Document& doc,
                           const std::string& path,
                           const std::string& tag,
                           const std::string& summary,
                           const std::string& operationId,
                           const Json::Value& parameters)
{
  auto& op = doc.operation(path, "get");
  op["tags"].append(tag);
  op["summary"] = summary;
  op["operationId"] = operationId;
  if (!parameters.empty())
    op["parameters"] = parameters;
  return op;
}

void add_json_operation(OA::Document& doc,
                        const std::string& path,
                        const std::string& tag,
                        const std::string& summary,
                        const std::string& operationId,
                        Json::Value parameters,
                        const std::string& schema)
{
  parameters.append(OA::ref("parameters", "f.json"));
  parameters.append(OA::ref("parameters", "language"));
  auto& op = add_operation(doc, path, tag, summary, operationId, parameters);
  op["responses"]["200"] = json_response(summary, OA::ref("schemas", schema));
  OA::Document::addErrorResponses(op);
}

// The collection specific operations below 'base' ("/collections/{collectionId}" or a
// literal collection path). 'collection' holds the path parameters of 'base'.
void add_collection_operations(OA::Document& doc,
                               const std::string& base,
                               const std::string& idSuffix,
                               const Json::Value& collection,
                               const Json::Value& styleParameter,
                               const Json::Value& elevationParameter,
                               const Json::Value& styleIdParameter)
{
  add_json_operation(doc,
                     base,
                     TAG_COLLECTIONS,
                     "Collection description",
                     "getCollection" + idSuffix,
                     collection,
                     "collectionDesc");

  auto tms = collection;
  tms.append(OA::ref("parameters", "tileMatrixSetId"));

  add_json_operation(doc,
                     base + "/tiles",
                     TAG_TILES,
                     "Tile sets of the collection",
                     "getCollectionTileSets" + idSuffix,
                     collection,
                     "tileSets");
  add_json_operation(doc,
                     base + "/tiles/{tileMatrixSetId}",
                     TAG_TILES,
                     "Tile set metadata",
                     "getCollectionTileSet" + idSuffix,
                     tms,
                     "tileSet");

  Json::Value dimensions(Json::arrayValue);
  dimensions.append(OA::ref("parameters", "datetime"));
  dimensions.append(OA::ref("parameters", "TIME"));
  dimensions.append(elevationParameter);
  dimensions.append(OA::ref("parameters", "reference_time"));
  dimensions.append(styleParameter);

  auto tile_params = tms;
  for (const char* name : {"tileMatrix", "tileRow", "tileCol"})
    tile_params.append(OA::ref("parameters", name));
  auto info_params = tile_params;

  tile_params.append(OA::ref("parameters", "f.tile"));
  for (const auto& p : dimensions)
    tile_params.append(p);
  tile_params.append(OA::ref("parameters", "width"));
  tile_params.append(OA::ref("parameters", "height"));

  auto& tile = add_operation(doc,
                             base + "/tiles/{tileMatrixSetId}/{tileMatrix}/{tileRow}/{tileCol}",
                             TAG_TILES,
                             "A tile of the collection",
                             "getCollectionTile" + idSuffix,
                             tile_params);
  tile["responses"]["200"] = OA::ref("responses", "Tile");
  OA::Document::addErrorResponses(tile);

  info_params.append(OA::ref("parameters", "j"));
  info_params.append(OA::ref("parameters", "i"));
  info_params.append(OA::ref("parameters", "f.info"));
  for (const auto& p : dimensions)
    info_params.append(p);

  auto& info =
      add_operation(doc,
                    base + "/tiles/{tileMatrixSetId}/{tileMatrix}/{tileRow}/{tileCol}/{j}/{i}",
                    TAG_TILES,
                    "Data values at a pixel of a tile",
                    "getCollectionTileFeatureInfo" + idSuffix,
                    info_params);
  info["description"] =
      "An extension of OGC API - Tiles, like the WMTS FeatureInfo resource and WMS "
      "GetFeatureInfo.";
  info["x-fmi-extension"] = true;
  info["responses"]["200"] = OA::ref("responses", "FeatureInfo");
  OA::Document::addErrorResponses(info);

  add_json_operation(doc,
                     base + "/styles",
                     TAG_STYLES,
                     "Styles of the collection",
                     "getCollectionStyles" + idSuffix,
                     collection,
                     "styleSet");

  auto style_params = collection;
  style_params.append(styleIdParameter);
  style_params.append(OA::ref("parameters", "f.style"));
  auto& style = add_operation(doc,
                              base + "/styles/{styleId}",
                              TAG_STYLES,
                              "A Mapbox GL style for the vector tiles of the collection",
                              "getCollectionStyle" + idSuffix,
                              style_params);
  style["responses"]["200"]["description"] = "The style";
  style["responses"]["200"]["content"]["application/vnd.mapbox.style+json"]["schema"] =
      OA::ref("schemas", "mapboxStyle");
  OA::Document::addErrorResponses(style);
}

}  // namespace

std::vector<std::string> conformanceClasses()
{
  std::vector<std::string> ret{
      "http://www.opengis.net/spec/ogcapi-common-1/1.0/conf/core",
      "http://www.opengis.net/spec/ogcapi-common-2/0.1/conf/collections",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/core",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/tileset",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/tilesets-list",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/geodata-tilesets",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/png",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/webp",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/mvt",
      "http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/geotiff",
      // OGC API - Styles (Part 1: Core) with the Mapbox style encoding
      "http://www.opengis.net/spec/ogcapi-styles-1/1.0/conf/core",
      "http://www.opengis.net/spec/ogcapi-styles-1/1.0/conf/mapbox-styles"};
  return ret;
}

Spine::OpenAPI::Document apiDefinition(const Config& config,
                                       const std::vector<OGC::OpenApiLayer>& layers,
                                       const Spine::OpenAPI::Settings& settings)
{
  try
  {
    const auto max_enum = settings.max_enum;

    OA::Document doc("SmartMet OGC API - Tiles",
                     "Map, vector and data tiles of the WMS layers following OGC API - Tiles, "
                     "with Mapbox GL styles following OGC API - Styles.",
                     "1.0",
                     settings);
    doc.addTag(TAG_CAPABILITIES, "The landing page, conformance declaration and API definition");
    doc.addTag(TAG_TMS, "Tile matrix sets");
    doc.addTag(TAG_COLLECTIONS, "Collections (layers)");
    doc.addTag(TAG_TILES, "Tile sets and tiles");
    doc.addTag(TAG_STYLES, "Mapbox GL styles of the vector tiles");
    doc.json()["externalDocs"]["description"] = "OGC API - Tiles";
    doc.json()["externalDocs"]["url"] = "https://ogcapi.ogc.org/tiles/";

    doc.addCommonComponents();
    add_schemas(doc);
    add_parameters(doc, config, layers, max_enum);

    doc.addCoreOperations(TAG_CAPABILITIES, OA::ref("parameters", "f.json"));

    add_json_operation(doc,
                       "/tileMatrixSets",
                       TAG_TMS,
                       "Tile matrix sets",
                       "getTileMatrixSets",
                       Json::Value(Json::arrayValue),
                       "tileMatrixSets");
    add_json_operation(doc,
                       "/tileMatrixSets/{tileMatrixSetId}",
                       TAG_TMS,
                       "Tile matrix set definition",
                       "getTileMatrixSet",
                       refs({"tileMatrixSetId"}),
                       "tileMatrixSet");
    add_json_operation(doc,
                       "/collections",
                       TAG_COLLECTIONS,
                       "Collections",
                       "getCollections",
                       Json::Value(Json::arrayValue),
                       "collections");
    add_json_operation(doc,
                       "/styles",
                       TAG_STYLES,
                       "Styles",
                       "getStyles",
                       Json::Value(Json::arrayValue),
                       "styleSet");

    add_collection_operations(doc,
                              "/collections/{collectionId}",
                              "",
                              refs({"collectionId"}),
                              OA::ref("parameters", "style"),
                              OA::ref("parameters", "elevation"),
                              OA::ref("parameters", "styleId"));

    if (settings.expand_collections && layers.size() <= max_enum)
    {
      for (const auto& layer : layers)
      {
        const auto name = component_name(layer.id);
        doc.addComponent(
            "parameters",
            name + ".style",
            OA::parameter(
                "style", "query", "Style of the tile", OA::enumSchema(layer.styles, max_enum)));
        doc.addComponent("parameters",
                         name + ".styleId",
                         OA::parameter("styleId",
                                       "path",
                                       "Identifier of a style",
                                       OA::enumSchema(layer.styles, max_enum)));
        Json::Value elevation = OA::stringSchema();
        if (!layer.elevations.empty())
          elevation = OA::enumSchema(layer.elevations, max_enum);
        doc.addComponent(
            "parameters",
            name + ".elevation",
            OA::parameter("elevation",
                          "query",
                          layer.elevations.empty() ? "Ignored, the layer has no vertical extent"
                                                   : "Vertical level",
                          elevation));

        add_collection_operations(doc,
                                  "/collections/" + layer.id,
                                  "_" + name,
                                  Json::Value(Json::arrayValue),
                                  OA::ref("parameters", name + ".style"),
                                  OA::ref("parameters", name + ".elevation"),
                                  OA::ref("parameters", name + ".styleId"));
      }
    }

    return doc;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate the OGC API - Tiles definition");
  }
}

}  // namespace Tiles
}  // namespace Plugin
}  // namespace SmartMet
