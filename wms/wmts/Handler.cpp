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
#include <cctype>
#include <optional>
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
  if (ext == "datatile") return "application/x-datatile+png";
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

    // GetTile (RESTful):
    //   version/layer/style[/dim…]/TileMatrixSet/TileMatrix/TileRow/TileCol.ext
    // Temporal/elevation layers carry extra dimension segments between the style
    // and the TileMatrixSet (Time, Reference_time, Elevation — see the
    // GetCapabilities ResourceURL). The fixed head is layer+style, the fixed tail
    // is the last four segments; anything in between is a dimension value, in the
    // same order the capabilities advertised. With no dimension segments (n == 7)
    // this reduces to the original fixed layout.
    if (parts.size() >= 7)
    {
      const std::size_t n = parts.size();
      const std::string& layer   = parts[1];
      const std::string& style   = parts[2];
      const std::string& tms_id  = parts[n - 4];
      const std::string& tm_id   = parts[n - 3];
      const std::string& row_str = parts[n - 2];
      const std::string& col_str = parts[n - 1];

      // Dimension values sit between the style (index 2) and the 4-segment tail.
      std::vector<std::string> dimensionValues(parts.begin() + 3, parts.begin() + (n - 4));

      unsigned tile_row = 0;
      unsigned tile_col = 0;
      std::string ext;

      try { tile_row = Fmi::stoul(row_str); }
      catch (...)
      {
        sendException("InvalidParameterValue", "Invalid TileRow: " + row_str,
                      theState, theRequest, theResponse);
        return QueryStatus::OK;
      }

      if (!parseColAndFormat(col_str, tile_col, ext))
      {
        sendException("InvalidParameterValue",
                      "Invalid TileCol or format: " + col_str,
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
                           tile_row, tile_col, mime_type, dimensionValues);
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

      // Time / elevation / reference-time dimensions.
      //
      // The WMS layer CDT already carries the temporal and elevation dimensions
      // (time_dimension[]/elevation_dimension[]); the WMTS capabilities used to
      // drop them, leaving clients unable to request anything but the latest
      // time. Reshape them into WMTS <Dimension> elements and build the matching
      // RESTful ResourceURL placeholders. Identifiers are capitalized
      // (time -> Time, elevation -> Elevation, reference_time -> Reference_time)
      // to match what the GeoWeb OpenLayers client substitutes into the tile
      // URL template (WMTSDimensionsFromDimensions upper-cases the first letter).
      CTPP::CDT dims(CTPP::CDT::ARRAY_VAL);
      std::string dim_path;
      // Reshape one WMS dimension entry (a HASH with name/default/value) into a
      // WMTS <Dimension> and its RESTful ResourceURL placeholder.
      auto add_dimension = [&](CTPP::CDT& e) {
        if (!e.Exists("name"))
          return;
        std::string name = e.At("name").GetString();
        if (name.empty())
          return;
        std::string identifier = name;
        identifier[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(identifier[0])));
        for (std::size_t k = 1; k < identifier.size(); ++k)
          identifier[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(identifier[k])));

        CTPP::CDT dim(CTPP::CDT::HASH_VAL);
        dim["identifier"] = identifier;
        if (e.Exists("units") && !e.At("units").GetString().empty())
          dim["uom"] = e.At("units").GetString();
        if (e.Exists("unit_symbol") && !e.At("unit_symbol").GetString().empty())
          dim["unit_symbol"] = e.At("unit_symbol").GetString();
        if (e.Exists("default"))
          dim["default"] = e.At("default");
        if (e.Exists("value"))
          dim["value"] = e.At("value");
        dims.PushBack(dim);
        dim_path += "/{" + identifier + "}";
      };
      // time_dimension is a list ([time, reference_time]); elevation_dimension may
      // be either a list or a single dimension hash — handle both shapes.
      for (const char* src : {"time_dimension", "elevation_dimension"})
      {
        if (!wl.Exists(src))
          continue;
        CTPP::CDT& node = wl.At(src);
        if (node.GetType() == CTPP::CDT::ARRAY_VAL)
        {
          for (std::size_t d = 0; d < node.Size(); ++d)
            add_dimension(node[d]);
        }
        else if (node.GetType() == CTPP::CDT::HASH_VAL)
        {
          add_dimension(node);
        }
      }
      if (dims.Size() > 0)
        layer["dimensions"] = dims;
      // Always defined so the ResourceURL template can interpolate it
      // unconditionally (empty for non-temporal layers).
      layer["dim_path"] = dim_path;

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
// -----------------------------------------------------------------------
/*!
 * \brief Ordered RESTful dimension identifiers for a layer, cached.
 *
 * Returns the dimension names in the same order GetCapabilities advertises
 * them in the ResourceURL template (time, reference_time, elevation). The set
 * of dimensions a layer exposes is fixed by configuration and does not change
 * between model runs — only their values do — so the list is cached to avoid a
 * per-tile Querydata lookup during animation.
 */
// -----------------------------------------------------------------------
const std::vector<std::string>& Handler::orderedDimensionNames(const std::string& layer) const
{
  {
    std::lock_guard<std::mutex> lock(itsDimNamesMutex);
    auto it = itsDimNamesCache.find(layer);
    if (it != itsDimNamesCache.end())
      return it->second;
  }

  std::vector<std::string> names;
  try
  {
    const auto& wmsConfig = itsWMTSConfig->wmsConfig();
    auto layer_obj = wmsConfig.getLayer(layer);
    if (layer_obj)
    {
      auto ti = layer_obj->getTimeDimensionInfo(false, {}, {}, {});
      if (ti && ti->Exists("time_dimension"))
      {
        CTPP::CDT& a = (*ti)["time_dimension"];
        if (a.GetType() == CTPP::CDT::ARRAY_VAL)
          for (std::size_t d = 0; d < a.Size(); ++d)
            if (a[d].Exists("name"))
              names.push_back(a[d].At("name").GetString());
      }
      auto ei = layer_obj->getElevationDimensionInfo();
      if (ei && ei->Exists("elevation_dimension"))
      {
        CTPP::CDT& e = (*ei)["elevation_dimension"];
        if (e.GetType() == CTPP::CDT::ARRAY_VAL)
        {
          for (std::size_t d = 0; d < e.Size(); ++d)
            if (e[d].Exists("name"))
              names.push_back(e[d].At("name").GetString());
        }
        else if (e.GetType() == CTPP::CDT::HASH_VAL && e.Exists("name"))
        {
          names.push_back(e.At("name").GetString());
        }
      }
    }
  }
  catch (...)
  {
    // Leave empty; caller falls back to query-parameter / default handling.
  }

  std::lock_guard<std::mutex> lock(itsDimNamesMutex);
  // try_emplace does not move `names` if another thread already cached this
  // layer, so the two-phase locking stays correct.
  return itsDimNamesCache.try_emplace(layer, std::move(names)).first->second;
}

QueryStatus Handler::handleGetTile(Dali::State& theState,
                                   const Spine::HTTP::Request& theRequest,
                                   Spine::HTTP::Response& theResponse,
                                   const std::string& layer,
                                   const std::string& style,
                                   const std::string& tms_id,
                                   const std::string& tm_id,
                                   unsigned tile_row,
                                   unsigned tile_col,
                                   const std::string& format,
                                   const std::vector<std::string>& dimensionValues)
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
    // Honor a client-requested output size (WIDTH/HEIGHT); the projection size
    // must equal the output size, or the data is rendered against the wrong grid
    // and ends up displaced (worst at low zoom). Fall back to the TileMatrix's
    // native tile dimensions when the client does not specify a size.
    auto req_width = theRequest.getParameter("WIDTH");
    auto req_height = theRequest.getParameter("HEIGHT");
    thisRequest.addParameter(
        "projection.xsize",
        (req_width && !req_width->empty()) ? *req_width : Fmi::to_string(tm->tile_width));
    thisRequest.addParameter(
        "projection.ysize",
        (req_height && !req_height->empty()) ? *req_height : Fmi::to_string(tm->tile_height));
    thisRequest.addParameter("projection.crs",   wmsConfig.getCRSDefinition(tms->crs));
    thisRequest.addParameter("type",             demimetype(format));
    thisRequest.addParameter("customer",         wmsConfig.layerCustomer(layer));

    // Dimensions from the RESTful path (Time, Reference_time, Elevation) take
    // precedence over query parameters. They arrive as bare values in
    // capabilities order, so resolve the layer's ordered dimension identifiers
    // from the very same source GetCapabilities used and map them positionally.
    std::string path_time;
    std::string path_origintime;
    std::string path_elevation;
    if (!dimensionValues.empty())
    {
      const auto& dim_names = orderedDimensionNames(layer);

      for (std::size_t i = 0; i < dimensionValues.size() && i < dim_names.size(); ++i)
      {
        const std::string& nm = dim_names[i];
        const std::string& v = dimensionValues[i];
        if (v.empty())
          continue;
        if (nm == "time")
          path_time = v;
        else if (nm == "reference_time")
          path_origintime = v;
        else if (nm == "elevation")
          path_elevation = v;
      }
    }

    // Time: RESTful path dimension > TIME query parameter > most current time.
    auto time_param = theRequest.getParameter("TIME");
    if (!path_time.empty())
    {
      thisRequest.addParameter("time", path_time);
    }
    else if (time_param && !time_param->empty())
    {
      thisRequest.addParameter("time", *time_param);
    }
    else if (wmsConfig.isTemporal(layer))
    {
      Fmi::DateTime current_time = wmsConfig.mostCurrentTime(layer, {});
      if (!current_time.is_not_a_date_time())
        thisRequest.addParameter("time", Fmi::to_iso_string(current_time));
    }

    // Reference (analysis/model-run) time and elevation supplied via the path.
    if (!path_origintime.empty())
      thisRequest.addParameter("origintime", path_origintime);
    if (!path_elevation.empty())
      thisRequest.addParameter("elevation", path_elevation);

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

    // Dali reads dimension parameters (time, elevation, reference/origin time)
    // from the State's request, which is fixed at construction and shared across
    // handlers. When the RESTful path carried dimension segments, bind a State to
    // the augmented request so those dimensions actually reach the renderer.
    // Non-dimension tiles keep the original State unchanged.
    std::optional<Dali::State> dimState;
    if (!dimensionValues.empty())
    {
      dimState.emplace(theState.getPlugin(), thisRequest);
      dimState->setType(demimetype(format));
    }
    Dali::State& renderState = dimState ? *dimState : theState;

    renderState.setName(layer);
    renderState.setCustomer(wmsConfig.layerCustomer(layer));

    // Store tile z/x/y in State so PMTiles-backed OSMLayers can do direct passthrough.
    // tm_id is the zoom level identifier ("0"-"21"); tile_col=x, tile_row=y.
    try
    {
      const auto zoom = static_cast<uint8_t>(Fmi::stoul(tm_id));
      renderState.setTileCoords(
          zoom, static_cast<uint32_t>(tile_col), static_cast<uint32_t>(tile_row));
    }
    catch (...)
    { /* non-numeric tm_id — no tile coords set, passthrough disabled */
    }

    Dali::Product product;
    product.init(json, renderState, itsDaliConfig);
    if (product.type.empty())
      product.type = renderState.getType();

    return generateTile(renderState, thisRequest, theResponse, product);
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
    {
      auto etag = fmt::sprintf("\"%x\"", product_hash);
      theResponse.setHeader("ETag", etag);

      // Standalone conditional handling (RFC 7232): If-None-Match -> 304,
      // If-Match failure -> 412, with no body, before generating the tile.
      if (auto status = Spine::HTTP::conditionalResponseStatus(theRequest, etag))
      {
        theResponse.setStatus(*status);
        return QueryStatus::OK;
      }
    }

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
