// ======================================================================
/*!
 * \brief WMTS REST handler implementation
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
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{

using Dali::demimetype;
using Dali::mimeType;
using OGC::useStyle;

namespace
{

// WMTS REST URL structure after /wmts/:
//   1.0.0/WMTSCapabilities.xml
//   1.0.0/{layer}/{style}/{tileMatrixSet}/{tileMatrix}/{tileRow}/{tileCol}.{ext}
//
// Returns path segments after stripping the leading '/wmts/' prefix.
std::vector<std::string> splitWmtsPath(const std::string& resource)
{
  const std::string prefix = "/wmts/";
  if (resource.size() <= prefix.size())
    return {};

  std::string path = resource.substr(prefix.size());
  std::vector<std::string> parts;
  boost::algorithm::split(parts, path, boost::is_any_of("/"));
  return parts;
}

// Split "col.ext" into tile column number and format extension.
// Example: "15.png" → col=15, ext="png"
bool parseColAndFormat(const std::string& s, unsigned& col, std::string& ext)
{
  auto dot = s.rfind('.');
  if (dot == std::string::npos)
    return false;

  ext = s.substr(dot + 1);
  try
  {
    col = Fmi::stoul(s.substr(0, dot));
    return true;
  }
  catch (...)
  {
    return false;
  }
}

// Map URL format extension to MIME type
std::string extensionToMimeType(const std::string& ext)
{
  if (ext == "png")  return "image/png";
  if (ext == "webp") return "image/webp";
  if (ext == "svg")  return "image/svg+xml";
  if (ext == "tiff" || ext == "tif") return "image/tiff";
  if (ext == "mvt" || ext == "pbf") return "application/vnd.mapbox-vector-tile";
  return {};
}

}  // namespace

Handler::Handler(const Dali::Config& daliConfig) : itsDaliConfig(daliConfig) {}

void Handler::init(std::unique_ptr<Config> wmtsConfig)
{
  itsWMTSConfig = std::move(wmtsConfig);
}

void Handler::shutdown()
{
  // Nothing to shut down — rendering pipeline is stateless per request.
}

// -----------------------------------------------------------------------
/*!
 * \brief Main WMTS query entry point — parses REST path and routes request
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
    auto parts = splitWmtsPath(resource);

    // Expect at least: version + (WMTSCapabilities.xml OR 6 more segments)
    if (parts.empty())
    {
      sendException("InvalidParameterValue", "Invalid WMTS URL", theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    // parts[0] is the version (e.g. "1.0.0"); we accept any version prefix
    // GetCapabilities: version/WMTSCapabilities.xml
    if (parts.size() == 2 && parts[1] == "WMTSCapabilities.xml")
      return handleGetCapabilities(theState, theRequest, theResponse);

    // GetTile: version/layer/style/TileMatrixSet/TileMatrix/TileRow/TileCol.ext
    if (parts.size() == 7)
    {
      const std::string& layer  = parts[1];
      const std::string& style  = parts[2];
      const std::string& tms_id = parts[3];
      const std::string& tm_id  = parts[4];

      unsigned tile_row = 0;
      unsigned tile_col = 0;
      std::string ext;

      try { tile_row = Fmi::stoul(parts[5]); }
      catch (...)
      {
        sendException("InvalidParameterValue", "Invalid TileRow: " + parts[5],
                      theState, theRequest, theResponse);
        return QueryStatus::OK;
      }

      if (!parseColAndFormat(parts[6], tile_col, ext))
      {
        sendException("InvalidParameterValue",
                      "Invalid TileCol or format: " + parts[6],
                      theState, theRequest, theResponse);
        return QueryStatus::OK;
      }

      std::string mime_type = extensionToMimeType(ext);
      if (mime_type.empty())
      {
        sendException("InvalidParameterValue",
                      "Unsupported tile format: " + ext,
                      theState, theRequest, theResponse);
        return QueryStatus::OK;
      }

      return handleGetTile(theState, theRequest, theResponse,
                           layer, style, tms_id, tm_id,
                           tile_row, tile_col, mime_type);
    }

    sendException("OperationNotSupported",
                  "Unrecognized WMTS URL pattern",
                  theState, theRequest, theResponse);
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WMTS query failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief Produce WMTS GetCapabilities XML response via CTPP2 template
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleGetCapabilities(Dali::State& theState,
                                           const Spine::HTTP::Request& theRequest,
                                           Spine::HTTP::Response& theResponse)
{
  try
  {
    auto tmpl = theState.getPlugin().getTemplate("wmts_get_capabilities");
    const auto& wmsConfig = itsWMTSConfig->wmsConfig();

    CTPP::CDT hash(CTPP::CDT::HASH_VAL);
    hash["version"] = "1.0.0";
    hash["service"]["title"] = "WMTS";
    hash["service"]["abstract"] = "";

    // Build WMTS base URL for ResourceURL elements
    auto host_protocol = theRequest.getProtocol();
    std::string protocol = host_protocol ? (*host_protocol + "://") : "http://";
    auto host_header = theRequest.getHeader("Host");
    std::string host = host_header ? *host_header : "localhost";
    if (host.size() >= 4 && host.substr(host.size() - 4) == "/wms")
      host = host.substr(0, host.size() - 4);

    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);
    std::string apikey_path;
    if (apikey)
      apikey_path = "/fmi-apikey/" + *apikey;

    hash["wmts_url"] = protocol + host + apikey_path + "/wmts";

    // Supported output formats
    std::size_t fi = 0;
    for (const auto& fmt : itsWMTSConfig->supportedFormats())
      hash["formats"][fi++] = fmt;

    // --- Layers ---
    // getCapabilities() returns the CDT that represents the WMS capabilities
    // layer list (flat hierarchy = array of layer CDTs).
    const auto& dali = itsWMTSConfig->getDaliConfig();
    auto language = dali.defaultLanguage();
    auto query_lang = theRequest.getParameter("LANGUAGE");
    if (query_lang)
      language = *query_lang;

    CTPP::CDT layer_list = wmsConfig.getCapabilities(
        apikey,
        language,
        {},  // starttime
        {},  // endtime
        {},  // reference_time
        {},  // wms_namespace
        OGC::LayerHierarchy::HierarchyType::flat,
        false,   // show_hidden
        false);  // multiple_intervals

    CTPP::CDT layers_cdt(CTPP::CDT::ARRAY_VAL);
    for (std::size_t i = 0; i < layer_list.Size(); ++i)
    {
      // CTPP2 CDT::At() has no const overload — use non-const access
      CTPP::CDT& wl = layer_list[i];
      CTPP::CDT layer(CTPP::CDT::HASH_VAL);

      if (wl.Exists("name"))     layer["identifier"] = wl.At("name");
      if (wl.Exists("title"))    layer["title"]      = wl.At("title");
      if (wl.Exists("abstract")) layer["abstract"]   = wl.At("abstract");

      // WGS84 bounding box
      if (wl.Exists("ex_geographic_bounding_box"))
      {
        CTPP::CDT& bb = wl.At("ex_geographic_bounding_box");
        if (bb.Exists("west_bound_longitude"))  layer["bbox_min_x"] = bb.At("west_bound_longitude");
        if (bb.Exists("east_bound_longitude"))  layer["bbox_max_x"] = bb.At("east_bound_longitude");
        if (bb.Exists("south_bound_latitude"))  layer["bbox_min_y"] = bb.At("south_bound_latitude");
        if (bb.Exists("north_bound_latitude"))  layer["bbox_max_y"] = bb.At("north_bound_latitude");
      }

      // Styles — carry over from WMS layer, ensuring at least 'default' exists
      if (wl.Exists("style"))
      {
        layer["styles"] = wl.At("style");
      }
      else
      {
        CTPP::CDT default_style(CTPP::CDT::HASH_VAL);
        default_style["identifier"] = "default";
        default_style["title"] = "Default";
        layer["styles"][0] = default_style;
      }

      // Supported tile formats
      std::size_t fmti = 0;
      for (const auto& fmt : itsWMTSConfig->supportedFormats())
        layer["formats"][fmti++] = fmt;

      // TileMatrixSet links — all standard TMS
      std::size_t tmsi = 0;
      for (const auto& tms : itsWMTSConfig->tileMatrixSets())
        layer["tile_matrix_set_links"][tmsi++] = tms.identifier;

      layers_cdt.PushBack(layer);
    }
    hash["layers"] = layers_cdt;

    // --- TileMatrixSets ---
    std::size_t tms_i = 0;
    for (const auto& tms : itsWMTSConfig->tileMatrixSets())
    {
      CTPP::CDT tms_cdt(CTPP::CDT::HASH_VAL);
      tms_cdt["identifier"] = tms.identifier;
      tms_cdt["crs"]        = tms.crs;
      if (!tms.well_known_scale_set.empty())
        tms_cdt["well_known_scale_set"] = tms.well_known_scale_set;

      for (std::size_t tm_i = 0; tm_i < tms.tile_matrices.size(); ++tm_i)
      {
        const auto& tm = tms.tile_matrices[tm_i];
        CTPP::CDT tm_cdt(CTPP::CDT::HASH_VAL);
        tm_cdt["identifier"]       = tm.identifier;
        tm_cdt["scale_denominator"] = fmt::format("{:.10g}", tm.scale_denominator);
        tm_cdt["top_left_corner"]  = fmt::format("{} {}", tm.top_left_corner_x,
                                                           tm.top_left_corner_y);
        tm_cdt["tile_width"]   = tm.tile_width;
        tm_cdt["tile_height"]  = tm.tile_height;
        tm_cdt["matrix_width"] = tm.matrix_width;
        tm_cdt["matrix_height"] = tm.matrix_height;
        tms_cdt["tile_matrices"][tm_i] = tm_cdt;
      }
      hash["tile_matrix_sets"][tms_i++] = tms_cdt;
    }

    const bool print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);
    if (print_hash)
      std::cout << fmt::format("WMTS GetCapabilities CDT:\n{}\n", hash.RecursiveDump());

    std::string output;
    std::string log;
    tmpl->process(hash, output, log);

    // Replace __hostname__ and __apikey__ placeholders that come from WMS layer LegendURL values
    boost::replace_all(output, "__hostname__", protocol + host);
    {
      std::string apirepl;
      if (apikey)
      {
        auto omit = theRequest.getHeader("omit-fmi-apikey");
        if (!omit || omit == std::string("0") || omit == std::string("false"))
          apirepl = "/fmi-apikey/" + *apikey;
      }
      boost::replace_all(output, "__apikey__", apirepl);
    }

    theResponse.setHeader("Content-Type", "application/xml; charset=UTF-8");
    theResponse.setContent(output);
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WMTS GetCapabilities failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief Validate parameters and serve a single map tile
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleGetTile(Dali::State& theState,
                                   const Spine::HTTP::Request& theRequest,
                                   Spine::HTTP::Response& theResponse,
                                   const std::string& layer,
                                   const std::string& style,
                                   const std::string& tms_id,
                                   const std::string& tm_id,
                                   unsigned tile_row,
                                   unsigned tile_col,
                                   const std::string& format)
{
  try
  {
    if (!itsWMTSConfig->isValidLayer(layer))
    {
      sendException("InvalidParameterValue", "Layer not found: " + layer,
                    theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    if (!itsWMTSConfig->isValidStyle(layer, style))
    {
      sendException("InvalidParameterValue",
                    "Style '" + style + "' not supported for layer: " + layer,
                    theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    if (!itsWMTSConfig->isValidFormat(format))
    {
      sendException("InvalidParameterValue", "Unsupported format: " + format,
                    theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    const TileMatrixSet* tms = itsWMTSConfig->findTileMatrixSet(tms_id);
    if (tms == nullptr)
    {
      sendException("InvalidParameterValue", "TileMatrixSet not found: " + tms_id,
                    theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    const TileMatrix* tm = itsWMTSConfig->findTileMatrix(*tms, tm_id);
    if (tm == nullptr)
    {
      sendException("InvalidParameterValue",
                    "TileMatrix '" + tm_id + "' not found in: " + tms_id,
                    theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    if (tile_row >= tm->matrix_height || tile_col >= tm->matrix_width)
    {
      sendException("TileOutOfRange",
                    fmt::format("Tile ({},{}) out of range ({}x{})",
                                tile_col, tile_row, tm->matrix_width, tm->matrix_height),
                    theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    // Compute tile bounding box in CRS coordinates
    TileBBox bbox = computeTileBBox(*tms, *tm, tile_row, tile_col);

    const auto& wmsConfig = itsWMTSConfig->wmsConfig();

    // Build a mutable request copy with Dali-vocabulary parameters injected
    auto thisRequest = theRequest;
    theState.setType(demimetype(format));

    // For geographic CRS (EPSG:4326), Projection::init() expects bbox in lat,lon order
    // (EPSGTreatsAsLatLong() returns true, so it reads parts as y1,x1,y2,x2).
    // computeTileBBox() always returns min_x=longitude, min_y=latitude, so we must swap.
    std::string bbox_str = tms->is_geographic
        ? fmt::format("{},{},{},{}", bbox.min_y, bbox.min_x, bbox.max_y, bbox.max_x)
        : fmt::format("{},{},{},{}", bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y);
    thisRequest.addParameter("projection.bbox",  bbox_str);
    thisRequest.addParameter("projection.xsize", Fmi::to_string(tm->tile_width));
    thisRequest.addParameter("projection.ysize", Fmi::to_string(tm->tile_height));
    thisRequest.addParameter("projection.crs",   wmsConfig.getCRSDefinition(tms->crs));
    thisRequest.addParameter("type",             demimetype(format));
    thisRequest.addParameter("customer",         wmsConfig.layerCustomer(layer));

    // Time: accept TIME query parameter, otherwise use most current available time
    auto time_param = theRequest.getParameter("TIME");
    if (time_param && !time_param->empty())
    {
      thisRequest.addParameter("time", *time_param);
    }
    else if (wmsConfig.isTemporal(layer))
    {
      Fmi::DateTime current_time = wmsConfig.mostCurrentTime(layer, {});
      if (!current_time.is_not_a_date_time())
        thisRequest.addParameter("time", Fmi::to_iso_string(current_time));
    }

    // Load product JSON, preprocess json: references and query params, then apply style
    Json::Value json = wmsConfig.json(layer);
    {
      const std::string customer = wmsConfig.layerCustomer(layer);
      const std::string root = itsDaliConfig.rootDirectory(true);
      const std::string layers_root = root + "/customers/" + customer + "/layers/";
      Spine::JSON::preprocess(json, root, layers_root, wmsConfig.getJsonCache());
      Spine::JSON::dereference(json);
      auto params = Dali::Plugin::extractValidParameters(thisRequest.getParameterMap());
      Spine::JSON::expand(json, params, "", false);
    }
    useStyle(json, style);

    // Apply margin defaults if not already set in the product JSON
    if (!json.isMember("xmargin"))
      json["xmargin"] = wmsConfig.getMargin();
    if (!json.isMember("ymargin"))
      json["ymargin"] = wmsConfig.getMargin();

    theState.setName(layer);
    theState.setCustomer(wmsConfig.layerCustomer(layer));

    // Store tile z/x/y in State so PMTiles-backed OSMLayers can do direct passthrough.
    // tm_id is the zoom level identifier ("0"-"21"); tile_col=x, tile_row=y.
    try
    {
      const auto zoom = static_cast<uint8_t>(Fmi::stoul(tm_id));
      theState.setTileCoords(
          zoom, static_cast<uint32_t>(tile_col), static_cast<uint32_t>(tile_row));
    }
    catch (...)
    { /* non-numeric tm_id — no tile coords set, passthrough disabled */
    }

    Dali::Product product;
    product.init(json, theState, itsDaliConfig);
    if (product.type.empty())
      product.type = theState.getType();

    return generateTile(theState, thisRequest, theResponse, product);
  }
  catch (...)
  {
    Fmi::Exception ex(BCP, "WMTS GetTile failed!", nullptr);
    sendException("NoApplicableCode", ex.what(), theState, theRequest, theResponse);
    return QueryStatus::OK;
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief Run the Dali rendering pipeline and return the tile image.
 *
 * Mirrors WMS::Handler::wmsGenerateProduct but without animation support
 * (tiles are always single-frame) and with WMTS exception format.
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
    try { product_hash = theProduct.hash_value(theState); }
    catch (...) { /* non-fatal: hash failure disables caching */ }

    if (product_hash != Fmi::bad_hash)
      theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));

    // Return cached tile if available
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

    if (!theProduct.svg_tmpl)
      theProduct.svg_tmpl = itsDaliConfig.defaultTemplate(theProduct.type);

    auto tmpl = theState.getPlugin().getTemplate(*theProduct.svg_tmpl);

    CTPP::CDT hash(CTPP::CDT::HASH_VAL);
    theProduct.generate(hash, theState);

    std::string output;
    std::string log;
    tmpl->process(hash, output, log);

    theState.getPlugin().formatResponse(
        output, theProduct.type, theRequest, theResponse,
        theState.useTimer(), theProduct, product_hash);

    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WMTS tile generation failed!");
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief Send an OWS 1.1 ExceptionReport XML response
 */
// -----------------------------------------------------------------------
void Handler::sendException(const std::string& code,
                            const std::string& text,
                            Dali::State& theState,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse)
{
  try
  {
    auto tmpl = theState.getPlugin().getTemplate("wmts_exception");
    CTPP::CDT hash(CTPP::CDT::HASH_VAL);
    hash["exception_code"] = code;
    hash["exception_text"] = text;

    std::string output;
    std::string log;
    tmpl->process(hash, output, log);

    theResponse.setHeader("Content-Type", "application/xml; charset=UTF-8");
    theResponse.setStatus(Spine::HTTP::Status::bad_request);
    theResponse.setContent(output);
  }
  catch (...)
  {
    // Fallback if template processing itself fails
    theResponse.setHeader("Content-Type", "text/plain");
    theResponse.setStatus(Spine::HTTP::Status::internal_server_error);
    theResponse.setContent("WMTS error " + code + ": " + text);
  }
}

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
