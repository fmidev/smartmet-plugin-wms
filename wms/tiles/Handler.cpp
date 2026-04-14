// ======================================================================
/*!
 * \brief OGC API - Tiles handler implementation
 */
// ======================================================================

#include "Handler.h"
#include "../Hash.h"
#include "../Mime.h"
#include "../Plugin.h"
#include "../Product.h"
#include "../State.h"
#include "../ogc/LayerHierarchy.h"
#include "../ogc/StyleSelection.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <json/json.h>
#include <json/writer.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Tiles
{

using Dali::demimetype;
using Dali::mimeType;
using OGC::useStyle;

namespace
{

// -----------------------------------------------------------------------
// Path utilities
// -----------------------------------------------------------------------

// Split the resource path after the base /tiles prefix.
// Returns segments as a vector; empty vector means the landing page.
// Example: "/tiles/collections/rain/tiles/EPSG:3857/10/512/256"
//   → ["collections", "rain", "tiles", "EPSG:3857", "10", "512", "256"]
std::vector<std::string> splitTilesPath(const std::string& resource,
                                         const std::string& base_url)
{
  // Strip base_url prefix (e.g. "/tiles")
  if (resource.size() < base_url.size())
    return {};
  if (resource == base_url || resource == base_url + "/")
    return {};

  std::string rest = resource.substr(base_url.size());
  if (!rest.empty() && rest[0] == '/')
    rest = rest.substr(1);

  std::vector<std::string> parts;
  boost::algorithm::split(parts, rest, boost::is_any_of("/"));

  // Remove empty segments
  parts.erase(
      std::remove_if(parts.begin(), parts.end(), [](const std::string& s) { return s.empty(); }),
      parts.end());
  return parts;
}

// -----------------------------------------------------------------------
// JSON helpers
// -----------------------------------------------------------------------

Json::Value makeLink(const std::string& href,
                     const std::string& rel,
                     const std::string& type,
                     const std::string& title = {},
                     bool templated = false)
{
  Json::Value link;
  link["href"] = href;
  link["rel"] = rel;
  if (!type.empty())
    link["type"] = type;
  if (!title.empty())
    link["title"] = title;
  if (templated)
    link["templated"] = true;
  return link;
}

std::string toJson(const Json::Value& val)
{
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Json::writeString(builder, val);
}

// -----------------------------------------------------------------------
// Format negotiation
// -----------------------------------------------------------------------

std::string extensionOrParamToMime(const std::string& f)
{
  if (f == "png" || f == "image/png")
    return "image/png";
  if (f == "webp" || f == "image/webp")
    return "image/webp";
  if (f == "svg" || f == "image/svg+xml")
    return "image/svg+xml";
  if (f == "tiff" || f == "geotiff" || f == "image/tiff")
    return "image/tiff";
  if (f == "mvt" || f == "pbf" || f == "application/vnd.mapbox-vector-tile")
    return "application/vnd.mapbox-vector-tile";
  if (f == "datatile" || f == "application/x-datatile+png")
    return "application/x-datatile+png";
  return {};
}

}  // namespace

// -----------------------------------------------------------------------

Handler::Handler(const Dali::Config& daliConfig) : itsDaliConfig(daliConfig) {}

void Handler::init(std::unique_ptr<Config> tilesConfig)
{
  itsTilesConfig = std::move(tilesConfig);
}

void Handler::shutdown()
{
  // Stateless per request — nothing to tear down.
}

// -----------------------------------------------------------------------
/*!
 * \brief Main OGC API - Tiles entry point
 */
// -----------------------------------------------------------------------
QueryStatus Handler::query(Spine::Reactor& /* theReactor */,
                            Dali::State& theState,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
  try
  {
    const std::string& resource = theRequest.getResource();
    const std::string base = computeBaseUrl(theRequest);

    auto parts = splitTilesPath(resource, itsDaliConfig.tilesUrl());

    // --- Landing page ---
    if (parts.empty())
      return handleLandingPage(base, theResponse);

    // --- /conformance ---
    if (parts.size() == 1 && parts[0] == "conformance")
      return handleConformance(base, theResponse);

    // --- /tileMatrixSets ---
    if (parts[0] == "tileMatrixSets")
    {
      if (parts.size() == 1)
        return handleTileMatrixSets(base, theResponse);
      if (parts.size() == 2)
        return handleTileMatrixSet(base, parts[1], theResponse);
    }

    // --- /collections ---
    if (parts[0] == "collections")
    {
      if (parts.size() == 1)
        return handleCollections(base, theState, theRequest, theResponse);

      const std::string& collId = parts[1];

      if (parts.size() == 2)
        return handleCollection(base, collId, theState, theRequest, theResponse);

      if (parts.size() == 3 && parts[2] == "tiles")
        return handleCollectionTilesets(base, collId, theResponse);

      if (parts.size() == 4 && parts[2] == "tiles")
        return handleTilesetMetadata(base, collId, parts[3], theResponse);

      // /collections/{id}/tiles/{tmsId}/{tm}/{row}/{col}
      if (parts.size() == 7 && parts[2] == "tiles")
      {
        const std::string& tmsId = parts[3];
        const std::string& tmId = parts[4];

        unsigned row = 0;
        unsigned col = 0;
        try
        {
          row = Fmi::stoul(parts[5]);
          col = Fmi::stoul(parts[6]);
        }
        catch (...)
        {
          sendError(400, "Bad Request", "Invalid tile row or column: " + parts[5] + "/" + parts[6],
                    theResponse);
          return QueryStatus::OK;
        }

        std::string format = negotiateFormat(theRequest);
        if (!itsTilesConfig->isValidFormat(format))
        {
          sendError(400, "Bad Request", "Unsupported tile format: " + format, theResponse);
          return QueryStatus::OK;
        }

        return handleGetTile(
            theState, theRequest, theResponse, collId, tmsId, tmId, row, col, format);
      }
    }

    sendError(404, "Not Found", "No matching OGC API - Tiles endpoint for: " + resource,
              theResponse);
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC API - Tiles query failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles  — Landing page
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleLandingPage(const std::string& base, Spine::HTTP::Response& resp)
{
  try
  {
    Json::Value doc;
    doc["title"] = "OGC API - Tiles";
    doc["description"] = "Access map tiles via OGC API - Tiles 1.0";

    Json::Value links(Json::arrayValue);
    links.append(makeLink(base, "self", "application/json", "This document"));
    links.append(makeLink(base + "/conformance",
                          "http://www.opengis.net/def/rel/ogc/1.0/conformance",
                          "application/json",
                          "Conformance classes"));
    links.append(makeLink(base + "/collections", "data", "application/json", "Collections"));
    links.append(makeLink(base + "/tileMatrixSets",
                          "http://www.opengis.net/def/rel/ogc/1.0/tiling-schemes",
                          "application/json",
                          "Tile Matrix Sets"));
    doc["links"] = links;

    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles landing page failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/conformance  — Conformance classes
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleConformance(const std::string& /* base */, Spine::HTTP::Response& resp)
{
  try
  {
    Json::Value doc;
    Json::Value uris(Json::arrayValue);
    uris.append("http://www.opengis.net/spec/ogcapi-common-1/1.0/conf/core");
    uris.append("http://www.opengis.net/spec/ogcapi-common-2/0.1/conf/collections");
    uris.append("http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/core");
    uris.append("http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/tileset");
    uris.append("http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/tilesets-list");
    uris.append("http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/geodata-tilesets");
    uris.append("http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/png");
    uris.append("http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/jpeg");
    uris.append("http://www.opengis.net/spec/ogcapi-tiles-1/1.0/conf/webp");
    doc["conformsTo"] = uris;

    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles conformance failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/tileMatrixSets  — List all TileMatrixSets
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleTileMatrixSets(const std::string& base, Spine::HTTP::Response& resp)
{
  try
  {
    Json::Value doc;
    Json::Value list(Json::arrayValue);

    for (const auto& tms : itsTilesConfig->tileMatrixSets())
    {
      Json::Value entry;
      entry["id"] = tms.identifier;
      entry["supportedCRS"] = Config::crsUri(tms.crs);

      const std::string uri = Config::tmsDefinitionUri(tms.identifier);
      if (!uri.empty())
        entry["uri"] = uri;

      if (!tms.well_known_scale_set.empty())
        entry["wellKnownScaleSet"] = Config::wellKnownScaleSetUri(tms.well_known_scale_set);

      Json::Value links(Json::arrayValue);
      links.append(makeLink(base + "/tileMatrixSets/" + tms.identifier,
                            "self",
                            "application/json",
                            tms.identifier));
      entry["links"] = links;
      list.append(entry);
    }

    doc["tileMatrixSets"] = list;
    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles tileMatrixSets failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/tileMatrixSets/{tmsId}  — Single TileMatrixSet definition
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleTileMatrixSet(const std::string& base,
                                          const std::string& tmsId,
                                          Spine::HTTP::Response& resp)
{
  try
  {
    const auto* tms = itsTilesConfig->findTileMatrixSet(tmsId);
    if (tms == nullptr)
    {
      sendError(404, "Not Found", "TileMatrixSet not found: " + tmsId, resp);
      return QueryStatus::OK;
    }

    Json::Value doc;
    doc["id"] = tms->identifier;
    doc["supportedCRS"] = Config::crsUri(tms->crs);

    const std::string uri = Config::tmsDefinitionUri(tms->identifier);
    if (!uri.empty())
      doc["uri"] = uri;

    if (!tms->well_known_scale_set.empty())
      doc["wellKnownScaleSet"] = Config::wellKnownScaleSetUri(tms->well_known_scale_set);

    // Bounding box
    Json::Value bbox(Json::arrayValue);
    bbox.append(tms->bbox_min_x);
    bbox.append(tms->bbox_min_y);
    bbox.append(tms->bbox_max_x);
    bbox.append(tms->bbox_max_y);
    doc["boundingBox"]["type"] = "BoundingBoxType";
    doc["boundingBox"]["lowerLeft"] = Json::Value(Json::arrayValue);
    doc["boundingBox"]["lowerLeft"].append(tms->bbox_min_x);
    doc["boundingBox"]["lowerLeft"].append(tms->bbox_min_y);
    doc["boundingBox"]["upperRight"] = Json::Value(Json::arrayValue);
    doc["boundingBox"]["upperRight"].append(tms->bbox_max_x);
    doc["boundingBox"]["upperRight"].append(tms->bbox_max_y);
    doc["boundingBox"]["crs"] = "http://www.opengis.net/def/crs/OGC/1.3/CRS84";

    // TileMatrix entries
    Json::Value matrices(Json::arrayValue);
    for (const auto& tm : tms->tile_matrices)
    {
      Json::Value entry;
      entry["id"] = tm.identifier;
      entry["scaleDenominator"] = tm.scale_denominator;
      entry["cellSize"] = tm.scale_denominator * WMTS::OGC_PIXEL_SIZE;
      entry["cornerOfOrigin"] = "topLeft";
      // pointOfOrigin in native CRS
      Json::Value point;
      point["type"] = "Point";
      Json::Value coords(Json::arrayValue);
      coords.append(tm.top_left_corner_x);
      coords.append(tm.top_left_corner_y);
      point["coordinates"] = coords;
      entry["pointOfOrigin"] = point;
      entry["tileWidth"] = tm.tile_width;
      entry["tileHeight"] = tm.tile_height;
      entry["matrixWidth"] = tm.matrix_width;
      entry["matrixHeight"] = tm.matrix_height;
      matrices.append(entry);
    }
    doc["tileMatrices"] = matrices;

    Json::Value links(Json::arrayValue);
    links.append(makeLink(base + "/tileMatrixSets/" + tms->identifier, "self", "application/json"));
    doc["links"] = links;

    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles tileMatrixSet detail failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/collections  — List available collections
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleCollections(const std::string& base,
                                        Dali::State& theState,
                                        const Spine::HTTP::Request& theRequest,
                                        Spine::HTTP::Response& resp)
{
  try
  {
    const auto& wmsConfig = itsTilesConfig->wmsConfig();
    const auto& dali = itsTilesConfig->getDaliConfig();

    auto language = dali.defaultLanguage();
    auto query_lang = theRequest.getParameter("LANGUAGE");
    if (query_lang)
      language = *query_lang;

    const bool check_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);

    CTPP::CDT layer_list = wmsConfig.getCapabilities(apikey,
                                                      language,
                                                      {},
                                                      {},
                                                      {},
                                                      {},
                                                      OGC::LayerHierarchy::HierarchyType::flat,
                                                      false,
                                                      false);

    Json::Value doc;
    Json::Value collections(Json::arrayValue);

    for (std::size_t i = 0; i < layer_list.Size(); ++i)
    {
      CTPP::CDT& wl = layer_list[i];
      if (!wl.Exists("name"))
        continue;

      const std::string id = wl.At("name").GetString();
      const std::string coll_base = base + "/collections/" + id;

      Json::Value entry;
      entry["id"] = id;

      if (wl.Exists("title"))
        entry["title"] = wl.At("title").GetString();

      if (wl.Exists("abstract"))
        entry["description"] = wl.At("abstract").GetString();

      Json::Value links(Json::arrayValue);
      links.append(makeLink(coll_base, "self", "application/json", id));
      links.append(makeLink(coll_base + "/tiles",
                            "http://www.opengis.net/def/rel/ogc/1.0/tiling-schemes",
                            "application/json",
                            "Available tile sets"));
      entry["links"] = links;
      collections.append(entry);
    }

    doc["collections"] = collections;

    Json::Value links(Json::arrayValue);
    links.append(makeLink(base + "/collections", "self", "application/json", "Collections"));
    doc["links"] = links;

    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles collections list failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/collections/{id}  — Single collection description
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleCollection(const std::string& base,
                                       const std::string& collId,
                                       Dali::State& theState,
                                       const Spine::HTTP::Request& theRequest,
                                       Spine::HTTP::Response& resp)
{
  try
  {
    if (!itsTilesConfig->isValidCollection(collId))
    {
      sendError(404, "Not Found", "Collection not found: " + collId, resp);
      return QueryStatus::OK;
    }

    const auto& wmsConfig = itsTilesConfig->wmsConfig();
    const auto& dali = itsTilesConfig->getDaliConfig();

    auto language = dali.defaultLanguage();
    auto query_lang = theRequest.getParameter("LANGUAGE");
    if (query_lang)
      language = *query_lang;

    const bool check_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);

    // Find layer entry in capabilities CDT
    CTPP::CDT layer_list = wmsConfig.getCapabilities(apikey,
                                                      language,
                                                      {},
                                                      {},
                                                      {},
                                                      {},
                                                      OGC::LayerHierarchy::HierarchyType::flat,
                                                      false,
                                                      false);

    const std::string coll_base = base + "/collections/" + collId;
    Json::Value doc;
    doc["id"] = collId;

    for (std::size_t i = 0; i < layer_list.Size(); ++i)
    {
      CTPP::CDT& wl = layer_list[i];
      if (!wl.Exists("name") || wl.At("name").GetString() != collId)
        continue;

      if (wl.Exists("title"))
        doc["title"] = wl.At("title").GetString();
      if (wl.Exists("abstract"))
        doc["description"] = wl.At("abstract").GetString();

      // Spatial extent
      if (wl.Exists("ex_geographic_bounding_box"))
      {
        CTPP::CDT& bb = wl.At("ex_geographic_bounding_box");
        Json::Value spatial;
        Json::Value bboxArr(Json::arrayValue);
        Json::Value corners(Json::arrayValue);
        corners.append(bb.Exists("west_bound_longitude")
                           ? bb.At("west_bound_longitude").GetString()
                           : "-180");
        corners.append(bb.Exists("south_bound_latitude")
                           ? bb.At("south_bound_latitude").GetString()
                           : "-90");
        corners.append(bb.Exists("east_bound_longitude")
                           ? bb.At("east_bound_longitude").GetString()
                           : "180");
        corners.append(bb.Exists("north_bound_latitude")
                           ? bb.At("north_bound_latitude").GetString()
                           : "90");
        bboxArr.append(corners);
        spatial["bbox"] = bboxArr;
        spatial["crs"] = "http://www.opengis.net/def/crs/OGC/1.3/CRS84";
        doc["extent"]["spatial"] = spatial;
      }
      break;
    }

    Json::Value links(Json::arrayValue);
    links.append(makeLink(coll_base, "self", "application/json", collId));
    links.append(makeLink(coll_base + "/tiles",
                          "http://www.opengis.net/def/rel/ogc/1.0/tiling-schemes",
                          "application/json",
                          "Available tile sets"));
    doc["links"] = links;

    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles collection detail failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/collections/{id}/tiles  — Available tile sets for collection
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleCollectionTilesets(const std::string& base,
                                               const std::string& collId,
                                               Spine::HTTP::Response& resp)
{
  try
  {
    if (!itsTilesConfig->isValidCollection(collId))
    {
      sendError(404, "Not Found", "Collection not found: " + collId, resp);
      return QueryStatus::OK;
    }

    const std::string coll_base = base + "/collections/" + collId;

    Json::Value doc;
    Json::Value tilesets(Json::arrayValue);

    for (const auto& tms : itsTilesConfig->tileMatrixSets())
    {
      Json::Value ts;
      ts["tileMatrixSetId"] = tms.identifier;
      ts["dataType"] = "map";
      ts["crs"] = Config::crsUri(tms.crs);

      const std::string uri = Config::tmsDefinitionUri(tms.identifier);
      if (!uri.empty())
        ts["tileMatrixSetURI"] = uri;

      Json::Value links(Json::arrayValue);
      links.append(makeLink(coll_base + "/tiles/" + tms.identifier,
                            "self",
                            "application/json",
                            tms.identifier + " tile set"));
      ts["links"] = links;
      tilesets.append(ts);
    }

    doc["tilesets"] = tilesets;

    Json::Value links(Json::arrayValue);
    links.append(makeLink(coll_base + "/tiles", "self", "application/json"));
    doc["links"] = links;

    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles collection tilesets failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/collections/{id}/tiles/{tmsId}  — Tileset metadata
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleTilesetMetadata(const std::string& base,
                                            const std::string& collId,
                                            const std::string& tmsId,
                                            Spine::HTTP::Response& resp)
{
  try
  {
    if (!itsTilesConfig->isValidCollection(collId))
    {
      sendError(404, "Not Found", "Collection not found: " + collId, resp);
      return QueryStatus::OK;
    }

    const auto* tms = itsTilesConfig->findTileMatrixSet(tmsId);
    if (tms == nullptr)
    {
      sendError(404, "Not Found", "TileMatrixSet not found: " + tmsId, resp);
      return QueryStatus::OK;
    }

    const std::string ts_base = base + "/collections/" + collId + "/tiles/" + tmsId;
    // RFC 6570 URI template for tile access
    const std::string tile_template = ts_base + "/{tileMatrix}/{tileRow}/{tileCol}";

    Json::Value doc;
    doc["tileMatrixSetId"] = tmsId;
    doc["dataType"] = "map";
    doc["crs"] = Config::crsUri(tms->crs);

    const std::string uri = Config::tmsDefinitionUri(tmsId);
    if (!uri.empty())
      doc["tileMatrixSetURI"] = uri;

    // BoundingBox in CRS84
    doc["boundingBox"]["type"] = "BoundingBoxType";
    doc["boundingBox"]["lowerLeft"] = Json::Value(Json::arrayValue);
    doc["boundingBox"]["lowerLeft"].append(tms->bbox_min_x);
    doc["boundingBox"]["lowerLeft"].append(tms->bbox_min_y);
    doc["boundingBox"]["upperRight"] = Json::Value(Json::arrayValue);
    doc["boundingBox"]["upperRight"].append(tms->bbox_max_x);
    doc["boundingBox"]["upperRight"].append(tms->bbox_max_y);
    doc["boundingBox"]["crs"] = "http://www.opengis.net/def/crs/OGC/1.3/CRS84";

    // Links: self + one templated link per supported image format
    Json::Value links(Json::arrayValue);
    links.append(makeLink(ts_base, "self", "application/json", tmsId + " tileset metadata"));

    for (const auto& fmt : itsTilesConfig->supportedFormats())
    {
      links.append(makeLink(tile_template + "?f=" + fmt,
                            "http://www.opengis.net/def/rel/ogc/1.0/map",
                            fmt,
                            collId + " tiles as " + fmt,
                            true));
    }
    doc["links"] = links;

    setJsonResponse(resp, toJson(doc));
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles tileset metadata failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /tiles/collections/{id}/tiles/{tmsId}/{tm}/{row}/{col}  — Tile image
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleGetTile(Dali::State& theState,
                                    const Spine::HTTP::Request& theRequest,
                                    Spine::HTTP::Response& theResponse,
                                    const std::string& collId,
                                    const std::string& tmsId,
                                    const std::string& tmId,
                                    unsigned row,
                                    unsigned col,
                                    const std::string& format)
{
  try
  {
    if (!itsTilesConfig->isValidCollection(collId))
    {
      sendError(404, "Not Found", "Collection not found: " + collId, theResponse);
      return QueryStatus::OK;
    }

    const auto* tms = itsTilesConfig->findTileMatrixSet(tmsId);
    if (tms == nullptr)
    {
      sendError(404, "Not Found", "TileMatrixSet not found: " + tmsId, theResponse);
      return QueryStatus::OK;
    }

    const auto* tm = itsTilesConfig->findTileMatrix(*tms, tmId);
    if (tm == nullptr)
    {
      sendError(404, "Not Found", "TileMatrix '" + tmId + "' not found in: " + tmsId, theResponse);
      return QueryStatus::OK;
    }

    if (row >= tm->matrix_height || col >= tm->matrix_width)
    {
      sendError(400,
                "Bad Request",
                fmt::format("Tile ({},{}) out of range ({}x{})", col, row, tm->matrix_width,
                            tm->matrix_height),
                theResponse);
      return QueryStatus::OK;
    }

    // Compute tile bounding box
    WMTS::TileBBox bbox = WMTS::computeTileBBox(*tms, *tm, row, col);

    const auto& wmsConfig = itsTilesConfig->wmsConfig();

    // Build request with Dali projection parameters
    auto thisRequest = theRequest;
    theState.setType(demimetype(format));

    // For geographic CRS (EPSG:4326), Projection::init() expects bbox in lat,lon order
    // (EPSGTreatsAsLatLong() returns true, so it reads parts as y1,x1,y2,x2).
    // computeTileBBox() always returns min_x=longitude, min_y=latitude, so we must swap.
    std::string bbox_str = tms->is_geographic
        ? fmt::format("{},{},{},{}", bbox.min_y, bbox.min_x, bbox.max_y, bbox.max_x)
        : fmt::format("{},{},{},{}", bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y);
    thisRequest.addParameter("projection.bbox", bbox_str);
    thisRequest.addParameter("projection.xsize", Fmi::to_string(tm->tile_width));
    thisRequest.addParameter("projection.ysize", Fmi::to_string(tm->tile_height));
    thisRequest.addParameter("projection.crs", wmsConfig.getCRSDefinition(tms->crs));
    thisRequest.addParameter("type", demimetype(format));
    thisRequest.addParameter("customer", wmsConfig.layerCustomer(collId));

    // Accept TIME query parameter or use most current time for temporal layers
    auto time_param = theRequest.getParameter("TIME");
    if (time_param && !time_param->empty())
    {
      thisRequest.addParameter("time", *time_param);
    }
    else if (wmsConfig.isTemporal(collId))
    {
      Fmi::DateTime current_time = wmsConfig.mostCurrentTime(collId, {});
      if (!current_time.is_not_a_date_time())
        thisRequest.addParameter("time", Fmi::to_iso_string(current_time));
    }

    // Use default style (OGC API Tiles does not require a style in the URL)
    const std::string style =
        Spine::optional_string(theRequest.getParameter("style"), "default");

    Json::Value json = wmsConfig.json(collId);
    {
      const std::string customer = wmsConfig.layerCustomer(collId);
      const std::string root = itsDaliConfig.rootDirectory(true);
      const std::string layers_root = root + "/customers/" + customer + "/layers/";
      Spine::JSON::preprocess(json, root, layers_root, wmsConfig.getJsonCache());
      Spine::JSON::dereference(json);
      auto params = Dali::Plugin::extractValidParameters(thisRequest.getParameterMap());
      Spine::JSON::expand(json, params, "", false);
    }
    useStyle(json, style);

    if (!json.isMember("xmargin"))
      json["xmargin"] = wmsConfig.getMargin();
    if (!json.isMember("ymargin"))
      json["ymargin"] = wmsConfig.getMargin();

    theState.setName(collId);
    theState.setCustomer(wmsConfig.layerCustomer(collId));

    // Store tile z/x/y in State so PMTiles-backed OSMLayers can do direct passthrough.
    // tmId is the zoom level identifier ("0"-"21"); col=x, row=y in tile coordinates.
    try
    {
      const auto zoom = static_cast<uint8_t>(Fmi::stoul(tmId));
      theState.setTileCoords(zoom, static_cast<uint32_t>(col), static_cast<uint32_t>(row));
    }
    catch (...)
    { /* non-numeric tmId — no tile coords set, passthrough disabled */
    }

    Dali::Product product;
    product.init(json, theState, itsDaliConfig);
    if (product.type.empty())
      product.type = theState.getType();

    return generateTile(theState, thisRequest, theResponse, product);
  }
  catch (...)
  {
    Fmi::Exception ex(BCP, "OGC Tiles GetTile failed!", nullptr);
    sendError(500, "Internal Server Error", ex.what(), theResponse);
    return QueryStatus::OK;
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief Run the Dali rendering pipeline and return the tile image.
 * Mirrors WMTS::Handler::generateTile.
 */
// -----------------------------------------------------------------------
QueryStatus Handler::generateTile(Dali::State& theState,
                                   const Spine::HTTP::Request& theRequest,
                                   Spine::HTTP::Response& theResponse,
                                   Dali::Product& theProduct)
{
  try
  {
    std::size_t product_hash = 0;
    try
    {
      product_hash = theProduct.hash_value(theState);
    }
    catch (...)
    { /* non-fatal: hash failure disables caching */
    }

    if (product_hash != Fmi::bad_hash)
      theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));

    auto cached = theState.getPlugin().findInImageCache(product_hash);
    if (cached)
    {
      theResponse.setHeader("Content-Type", mimeType(theProduct.type));
      theResponse.setContent(cached);
      return QueryStatus::OK;
    }

    // GeoTiff: bypass CTPP/SVG pipeline entirely and return raw grid data
    if (theProduct.type == "geotiff")
    {
      auto bytes = theProduct.generateGeoTiff(theState);
      auto buffer = std::make_shared<std::string>(std::move(bytes));
      theState.getPlugin().insertInImageCache(product_hash, buffer);
      theResponse.setHeader("Content-Type", mimeType("geotiff"));
      theResponse.setContent(buffer);
      return QueryStatus::OK;
    }

    // MVT: bypass CTPP/SVG pipeline and return protobuf-encoded vector tile
    if (theProduct.type == "mvt")
    {
      auto bytes = theProduct.generateMVT(theState);
      auto buffer = std::make_shared<std::string>(std::move(bytes));
      theState.getPlugin().insertInImageCache(product_hash, buffer);
      theResponse.setHeader("Content-Type", mimeType("mvt"));
      theResponse.setContent(buffer);
      return QueryStatus::OK;
    }

    // DataTile: bypass CTPP/SVG pipeline and return RGBA-encoded float data PNG
    if (theProduct.type == "datatile")
    {
      auto bytes = theProduct.generateDataTile(theState);
      auto buffer = std::make_shared<std::string>(std::move(bytes));
      theState.getPlugin().insertInImageCache(product_hash, buffer);
      theResponse.setHeader("Content-Type", mimeType("datatile"));
      theResponse.setContent(buffer);
      return QueryStatus::OK;
    }

    if (!theProduct.svg_tmpl)
      theProduct.svg_tmpl = itsDaliConfig.defaultTemplate(theProduct.type);

    auto tmpl = theState.getPlugin().getTemplate(*theProduct.svg_tmpl);

    CTPP::CDT hash(CTPP::CDT::HASH_VAL);
    theProduct.generate(hash, theState);

    std::string output;
    std::string log;
    tmpl->process(hash, output, log);

    theState.getPlugin().formatResponse(
        output, theProduct.type, theRequest, theResponse, theState.useTimer(), theProduct,
        product_hash);

    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "OGC Tiles tile generation failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief Send an RFC 7807 problem+json error response
 */
// -----------------------------------------------------------------------
void Handler::sendError(int status,
                         const std::string& title,
                         const std::string& detail,
                         Spine::HTTP::Response& resp)
{
  try
  {
    Json::Value doc;
    doc["type"] = "https://www.rfc-editor.org/rfc/rfc9110";
    doc["title"] = title;
    doc["status"] = status;
    doc["detail"] = detail;

    resp.setHeader("Content-Type", "application/problem+json; charset=UTF-8");
    resp.setStatus(static_cast<Spine::HTTP::Status>(status));
    resp.setContent(toJson(doc));
  }
  catch (...)
  {
    resp.setHeader("Content-Type", "text/plain");
    resp.setStatus(Spine::HTTP::Status::internal_server_error);
    resp.setContent(title + ": " + detail);
  }
}

// -----------------------------------------------------------------------

void Handler::setJsonResponse(Spine::HTTP::Response& resp, const std::string& body)
{
  resp.setHeader("Content-Type", "application/json; charset=UTF-8");
  resp.setContent(body);
}

// -----------------------------------------------------------------------

std::string Handler::computeBaseUrl(const Spine::HTTP::Request& req) const
{
  auto host_protocol = req.getProtocol();
  std::string protocol = host_protocol ? (*host_protocol + "://") : "http://";
  auto host_header = req.getHeader("Host");
  std::string host = host_header ? *host_header : "localhost";

  const bool check_token = true;
  auto apikey = Spine::FmiApiKey::getFmiApiKey(req, check_token);
  std::string apikey_path = apikey ? ("/fmi-apikey/" + *apikey) : "";

  return protocol + host + apikey_path + itsDaliConfig.tilesUrl();
}

std::string Handler::negotiateFormat(const Spine::HTTP::Request& req) const
{
  // 1. 'f' query parameter
  auto f_param = req.getParameter("f");
  if (f_param)
  {
    std::string m = extensionOrParamToMime(*f_param);
    if (!m.empty())
      return m;
  }

  // 2. Accept header
  auto accept = req.getHeader("Accept");
  if (accept)
  {
    if (accept->find("image/webp") != std::string::npos)
      return "image/webp";
    if (accept->find("image/svg+xml") != std::string::npos)
      return "image/svg+xml";
    if (accept->find("image/png") != std::string::npos)
      return "image/png";
  }

  // 3. Default
  return "image/png";
}

}  // namespace Tiles
}  // namespace Plugin
}  // namespace SmartMet
