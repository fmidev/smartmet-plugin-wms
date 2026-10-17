// ======================================================================
/*!
 * \brief WMTS REST handler implementation
 */
// ======================================================================

#include "Handler.h"
#include "ApiDefinition.h"
#include "../Hash.h"
#include "../JsonTools.h"
#include "../Mime.h"
#include "../Plugin.h"
#include "../Product.h"
#include "../State.h"
#include "../ogc/BaseURL.h"
#include "../ogc/LayerHierarchy.h"
#include "../ogc/StyleSelection.h"
#include "../wms/Handler.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <macgyver/TimeParser.h>
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
std::vector<std::string> splitWmtsPath(const std::string& resource, const std::string& root)
{
  std::string prefix = root;
  while (!prefix.empty() && prefix.back() == '/')
    prefix.pop_back();
  prefix += "/";
  if (resource.size() <= prefix.size() || resource.compare(0, prefix.size(), prefix) != 0)
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

// WMS lets a client ask for TIME=current, and the capabilities advertise it
// with <Current>. The tile is rendered by Dali, which would read "current" as
// the wall clock and draw nothing for data not covering it, so resolve the
// keyword to the layer's default time here (the same one <Default> shows).
std::string resolveCurrentTime(const WMS::Config& wmsConfig,
                               const std::string& layer,
                               const std::string& value,
                               const std::string& origintime)
{
  if (!boost::iequals(value, "current"))
    return value;
  std::optional<Fmi::DateTime> reference_time;
  if (!origintime.empty())
    reference_time = Fmi::TimeParser::parse(origintime);
  auto t = wmsConfig.mostCurrentTime(layer, reference_time);
  return t.is_not_a_date_time() ? value : Fmi::to_iso_string(t);
}

// Append the names of the dimensions in the array, or of a single dimension hash if allowed
void append_dimension_names(CTPP::CDT& dims, bool allow_hash, std::vector<std::string>& names)
{
  if (dims.GetType() == CTPP::CDT::ARRAY_VAL)
  {
    for (std::size_t d = 0; d < dims.Size(); ++d)
      if (dims[d].Exists("name"))
        names.push_back(dims[d].At("name").GetString());
  }
  else if (allow_hash && dims.GetType() == CTPP::CDT::HASH_VAL && dims.Exists("name"))
  {
    names.push_back(dims.At("name").GetString());
  }
}

// Dimension values given in a RESTful path
struct PathDimensions
{
  std::string time;
  std::string origintime;
  std::string elevation;
};

// Map the RESTful path dimension values positionally to the ordered dimension names
PathDimensions path_dimensions(const std::vector<std::string>& dimensionValues,
                               const std::vector<std::string>& dim_names)
{
  PathDimensions dims;
  for (std::size_t i = 0; i < dimensionValues.size() && i < dim_names.size(); ++i)
  {
    const std::string& nm = dim_names[i];
    const std::string& v = dimensionValues[i];
    if (v.empty())
      continue;
    if (nm == "time")
      dims.time = v;
    else if (nm == "reference_time")
      dims.origintime = v;
    else if (nm == "elevation")
      dims.elevation = v;
  }
  return dims;
}

// Time: RESTful path dimension > TIME query parameter > most current time.
void add_time_parameter(Spine::HTTP::Request& thisRequest,
                        const Spine::HTTP::Request& theRequest,
                        const WMS::Config& wmsConfig,
                        const std::string& layer,
                        const std::string& path_time,
                        const std::string& path_origintime)
{
  auto time_param = theRequest.getParameter("TIME");
  if (!path_time.empty())
  {
    thisRequest.addParameter("time",
                             resolveCurrentTime(wmsConfig, layer, path_time, path_origintime));
  }
  else if (time_param && !time_param->empty())
  {
    thisRequest.addParameter("time",
                             resolveCurrentTime(wmsConfig, layer, *time_param, path_origintime));
  }
  else if (wmsConfig.isTemporal(layer))
  {
    Fmi::DateTime current_time = wmsConfig.mostCurrentTime(layer, {});
    if (!current_time.is_not_a_date_time())
      thisRequest.addParameter("time", Fmi::to_iso_string(current_time));
  }
}

// Capitalized WMTS dimension identifier (time -> Time, reference_time -> Reference_time)
std::string dimension_identifier(const std::string& name)
{
  std::string identifier = name;
  identifier[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(identifier[0])));
  for (std::size_t k = 1; k < identifier.size(); ++k)
    identifier[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(identifier[k])));
  return identifier;
}

// Reshape one WMS dimension entry (a HASH with name/default/value) into a
// WMTS <Dimension> and its RESTful ResourceURL placeholder.
void add_dimension(CTPP::CDT& e,
                   CTPP::CDT& wl,
                   const WMS::Config& wmsConfig,
                   CTPP::CDT& dims,
                   std::string& dim_path)
{
  if (!e.Exists("name"))
    return;
  std::string name = e.At("name").GetString();
  if (name.empty())
    return;
  const std::string identifier = dimension_identifier(name);

  CTPP::CDT dim(CTPP::CDT::HASH_VAL);
  dim["identifier"] = identifier;
  if (e.Exists("units") && !e.At("units").GetString().empty())
    dim["uom"] = e.At("units").GetString();
  if (e.Exists("unit_symbol") && !e.At("unit_symbol").GetString().empty())
    dim["unit_symbol"] = e.At("unit_symbol").GetString();
  if (e.Exists("default"))
    dim["default"] = e.At("default");
  else if (name == "time" && wl.Exists("name"))
  {
    // The same time a KVP GetTile without TIME renders (see handleGetTile):
    // latest for observations, nearest to the wall clock for forecasts,
    // the first time for forecasts entirely in the past.
    auto t = wmsConfig.mostCurrentTime(wl.At("name").GetString(), {});
    if (!t.is_not_a_date_time())
      dim["default"] = Fmi::to_iso_extended_string(t) + "Z";
  }
  // WMS says current="1" when the keyword "current" is accepted as a value;
  // GetTile resolves it for the time dimension (resolveCurrentTime).
  if (name == "time" && e.Exists("current") && e.At("current").GetInt() == 1)
    dim["current"] = 1;
  // WMS packs the values into one comma separated attribute; WMTS has one
  // <Value> element per value. An ISO 8601 start/end/period range stays
  // one value, as GeoServer and ADAGUC emit it.
  if (e.Exists("value"))
  {
    CTPP::CDT values(CTPP::CDT::ARRAY_VAL);
    std::vector<std::string> parts;
    // Boost 1.69 (RHEL8) split() takes the input by reference, no temporaries
    const std::string value = e.At("value").GetString();
    boost::algorithm::split(parts, value, boost::is_any_of(","));
    for (const auto& v : parts)
      if (!v.empty())
        values.PushBack(v);
    dim["values"] = values;
  }
  dims.PushBack(dim);
  dim_path += "/{" + identifier + "}";
}

// WGS84 bounding box of the layer
void add_layer_bbox(CTPP::CDT& layer, CTPP::CDT& wl)
{
  if (wl.Exists("ex_geographic_bounding_box"))
  {
    CTPP::CDT& bb = wl.At("ex_geographic_bounding_box");
    if (bb.Exists("west_bound_longitude"))  layer["bbox_min_x"] = bb.At("west_bound_longitude");
    if (bb.Exists("east_bound_longitude"))  layer["bbox_max_x"] = bb.At("east_bound_longitude");
    if (bb.Exists("south_bound_latitude"))  layer["bbox_min_y"] = bb.At("south_bound_latitude");
    if (bb.Exists("north_bound_latitude"))  layer["bbox_max_y"] = bb.At("north_bound_latitude");
  }
}

// Styles — carry over from WMS layer, ensuring at least 'default' exists.
void add_layer_styles(CTPP::CDT& layer, CTPP::CDT& wl)
{
  // The WMS capabilities hash names a style with "name" (<Name> in WMS
  // 1.3.0) but the WMTS template reads "identifier" (<ows:Identifier>), so
  // the key has to be translated. Copying the hash verbatim published an
  // empty <ows:Identifier> for every style, leaving clients no style name
  // to put in the ResourceURL {Style} segment.
  if (wl.Exists("style"))
  {
    CTPP::CDT& wms_styles = wl.At("style");
    std::size_t si = 0;
    for (std::size_t i = 0; i < wms_styles.Size(); ++i)
    {
      CTPP::CDT& src = wms_styles[i];
      CTPP::CDT style(CTPP::CDT::HASH_VAL);
      if (src.Exists("name")) style["identifier"] = src.At("name");
      if (src.Exists("title")) style["title"] = src.At("title");
      if (src.Exists("abstract")) style["abstract"] = src.At("abstract");
      if (src.Exists("legend_url")) style["legend_url"] = src.At("legend_url");
      layer["styles"][si++] = style;
    }
  }

  if (!layer.Exists("styles"))
  {
    CTPP::CDT default_style(CTPP::CDT::HASH_VAL);
    default_style["identifier"] = "default";
    default_style["title"] = "Default";
    layer["styles"][0] = default_style;
  }
}

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
void add_layer_dimensions(CTPP::CDT& layer, CTPP::CDT& wl, const WMS::Config& wmsConfig)
{
  CTPP::CDT dims(CTPP::CDT::ARRAY_VAL);
  std::string dim_path;
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
        add_dimension(node[d], wl, wmsConfig, dims, dim_path);
    }
    else if (node.GetType() == CTPP::CDT::HASH_VAL)
    {
      add_dimension(node, wl, wmsConfig, dims, dim_path);
    }
  }
  if (dims.Size() > 0)
    layer["dimensions"] = dims;
  // Always defined so the ResourceURL template can interpolate it
  // unconditionally (empty for non-temporal layers).
  layer["dim_path"] = dim_path;
}

// WMTS capabilities of a WMS capabilities layer
CTPP::CDT layer_capabilities(CTPP::CDT& wl, const Config& wmtsConfig)
{
  const auto& wmsConfig = wmtsConfig.wmsConfig();
  CTPP::CDT layer(CTPP::CDT::HASH_VAL);

  if (wl.Exists("name"))     layer["identifier"] = wl.At("name");
  if (wl.Exists("title"))    layer["title"]      = wl.At("title");
  if (wl.Exists("abstract")) layer["abstract"]   = wl.At("abstract");

  add_layer_bbox(layer, wl);
  add_layer_styles(layer, wl);

  // Supported tile formats
  std::size_t fmti = 0;
  for (const auto& fmt : wmtsConfig.supportedFormats())
    layer["formats"][fmti++] = fmt;

  // TileMatrixSet links — all standard TMS
  std::size_t tmsi = 0;
  for (const auto& tms : wmtsConfig.tileMatrixSets())
    layer["tile_matrix_set_links"][tmsi++] = tms.identifier;

  add_layer_dimensions(layer, wl, wmsConfig);
  return layer;
}

// Capabilities of a tile matrix set
CTPP::CDT tile_matrix_set_cdt(const TileMatrixSet& tms)
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
  return tms_cdt;
}

}  // namespace

Handler::Handler(const Dali::Config& daliConfig) : itsDaliConfig(daliConfig) {}

void Handler::init(std::unique_ptr<Config> wmtsConfig)
{
  itsWMTSConfig = std::move(wmtsConfig);

  auto settings = Spine::OpenAPI::Settings::parse(itsDaliConfig.getConfig());
  if (settings.enabled)
  {
    itsApi = std::make_unique<Spine::OpenAPI::Publisher>(std::move(settings));
    itsApi->addToCatalog(itsDaliConfig.wmtsUrl(), API_TITLE, API_DESCRIPTION);
  }
}

// -----------------------------------------------------------------------
/*!
 * \brief GET /wmts/api, /wmts/api/docs and the documentation files
 *
 * The definition lists the layers, styles and dimensions the apikey of the
 * client has access to. It is regenerated on demand when the capabilities
 * have changed since it was generated.
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleApi(const Spine::HTTP::Request& theRequest,
                               Spine::HTTP::Response& theResponse)
{
  try
  {
    const auto& root = itsDaliConfig.wmtsUrl();

    // The documentation files do not need the definition. The definition lists the layers
    // the apikey of the client has access to, like the capabilities.
    std::shared_ptr<const Spine::OpenAPI::Definition> definition;
    if (theRequest.getResource().find("/api/docs/") == std::string::npos)
    {
      auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, false);
      definition = itsApiCache.get(
          itsWMTSConfig->wmsConfig(),
          itsDaliConfig,
          apikey,
          [this](const std::vector<OGC::OpenApiLayer>& layers)
          { return apiDefinition(*itsWMTSConfig, layers, itsApi->settings()); });
    }

    itsApi->handle(
        theRequest, theResponse, root, Spine::OpenAPI::serverUrl(theRequest, root), definition);
    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WMTS API definition failed!");
  }
}

void Handler::shutdown()
{
  // Nothing to shut down — rendering pipeline is stateless per request.
}

// -----------------------------------------------------------------------
/*!
 * \brief Parse a RESTful GetFeatureInfo path and route it to handleGetFeatureInfo
 */
// -----------------------------------------------------------------------
QueryStatus Handler::routeGetFeatureInfo(const Spine::Reactor& theReactor,
                                         Dali::State& theState,
                                         const Spine::HTTP::Request& theRequest,
                                         Spine::HTTP::Response& theResponse,
                                         const std::vector<std::string>& parts)
{
  const std::size_t n = parts.size();
  const std::string& layer   = parts[1];
  const std::string& style   = parts[2];
  const std::string& tms_id  = parts[n - 6];
  const std::string& tm_id   = parts[n - 5];
  const std::string& row_str = parts[n - 4];
  const std::string& col_str = parts[n - 3];
  const std::string& j_str   = parts[n - 2];
  const std::string& i_str   = parts[n - 1];  // "{I}.{ext}"

  std::vector<std::string> dimensionValues(parts.begin() + 3, parts.begin() + (n - 6));

  unsigned tile_row = 0;
  unsigned tile_col = 0;
  unsigned pixel_j = 0;
  unsigned pixel_i = 0;
  std::string ext;

  try
  {
    tile_row = Fmi::stoul(row_str);
    tile_col = Fmi::stoul(col_str);
    pixel_j = Fmi::stoul(j_str);
  }
  catch (...)
  {
    sendException("InvalidParameterValue",
                  "Invalid TileRow/TileCol/J in FeatureInfo URL",
                  theState, theRequest, theResponse);
    return QueryStatus::OK;
  }

  if (!parseColAndFormat(i_str, pixel_i, ext))
  {
    sendException("InvalidParameterValue",
                  "Invalid I or info format: " + i_str,
                  theState, theRequest, theResponse);
    return QueryStatus::OK;
  }

  std::string info_format;
  if (ext == "json")
    info_format = "application/json";
  else if (ext == "html")
    info_format = "text/html";
  else
  {
    sendException("InvalidParameterValue",
                  "Unsupported info format: " + ext,
                  theState, theRequest, theResponse);
    return QueryStatus::OK;
  }

  return handleGetFeatureInfo(theReactor, theState, theRequest, theResponse,
                              layer, style, tms_id, tm_id,
                              tile_row, tile_col, pixel_j, pixel_i,
                              info_format, dimensionValues);
}

// -----------------------------------------------------------------------
/*!
 * \brief Parse a RESTful GetTile path and route it to handleGetTile
 */
// -----------------------------------------------------------------------
QueryStatus Handler::routeGetTile(Dali::State& theState,
                                  const Spine::HTTP::Request& theRequest,
                                  Spine::HTTP::Response& theResponse,
                                  const std::vector<std::string>& parts)
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

// -----------------------------------------------------------------------
/*!
 * \brief Main WMTS query entry point — parses REST path and routes request
 */
// -----------------------------------------------------------------------
QueryStatus Handler::query(const Spine::Reactor& theReactor,
                           Dali::State& theState,
                           const Spine::HTTP::Request& theRequest,
                           Spine::HTTP::Response& theResponse)
{
  try
  {
    const std::string& resource = theRequest.getResource();

    // The OpenAPI definition and its documentation
    if (itsApi && Spine::OpenAPI::Publisher::isApiResource(resource, itsDaliConfig.wmtsUrl()))
      return handleApi(theRequest, theResponse);

    auto parts = splitWmtsPath(resource, itsDaliConfig.wmtsUrl());

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

    // GetFeatureInfo (RESTful, OGC 07-057r7 FeatureInfo resource):
    //   version/layer/style[/dim…]/TileMatrixSet/TileMatrix/TileRow/TileCol/{J}/{I}.ext
    // Same shape as GetTile below plus the two pixel segments. A GetTile URL
    // with two dimension segments has the same length as a dimensionless
    // FeatureInfo URL, so the tail is disambiguated by which segment is a
    // known TileMatrixSet id (never numeric-only, so no collision with J/I or
    // TileRow).
    if (parts.size() >= 9)
    {
      const std::size_t n = parts.size();
      if (itsWMTSConfig->findTileMatrixSet(parts[n - 6]) != nullptr)
      {
        return routeGetFeatureInfo(theReactor, theState, theRequest, theResponse, parts);
      }
    }

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
      return routeGetTile(theState, theRequest, theResponse, parts);
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
    // Host, X-Forwarded-Proto and the apikey are client-controlled, so they are validated
    // before being written into the rendered document
    const std::string protocol = OGC::requestProtocol(theRequest);
    const std::string host = OGC::requestHost(theRequest, "localhost");
    const std::string apikey_path = OGC::requestApiKeyPath(theRequest);
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);

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
      layers_cdt.PushBack(layer_capabilities(layer_list[i], *itsWMTSConfig));
    }
    hash["layers"] = layers_cdt;

    // --- TileMatrixSets ---
    std::size_t tms_i = 0;
    for (const auto& tms : itsWMTSConfig->tileMatrixSets())
      hash["tile_matrix_sets"][tms_i++] = tile_matrix_set_cdt(tms);

    const bool print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);
    if (print_hash)
      std::cout << fmt::format("WMTS GetCapabilities CDT:\n{}\n", hash.RecursiveDump());

    std::string output;
    std::string log;
    tmpl->process(hash, output, log);

    // Replace __hostname__ and __apikey__ placeholders that come from WMS layer LegendURL values
    boost::replace_all(output, "__hostname__", protocol + host);
    boost::replace_all(output, "__apikey__", apikey_path);

    theResponse.setHeader("Content-Type", "application/xml; charset=UTF-8");
    theResponse.setContent(std::move(output));
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
    std::scoped_lock lock(itsDimNamesMutex);
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
        append_dimension_names((*ti)["time_dimension"], false, names);
      auto ei = layer_obj->getElevationDimensionInfo();
      if (ei && ei->Exists("elevation_dimension"))
        append_dimension_names((*ei)["elevation_dimension"], true, names);
    }
  }
  catch (...)
  {
    // Leave empty; caller falls back to query-parameter / default handling.
  }

  std::scoped_lock lock(itsDimNamesMutex);
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
    PathDimensions path_dims;
    if (!dimensionValues.empty())
      path_dims = path_dimensions(dimensionValues, orderedDimensionNames(layer));
    const std::string& path_time = path_dims.time;
    const std::string& path_origintime = path_dims.origintime;
    const std::string& path_elevation = path_dims.elevation;

    // Time: RESTful path dimension > TIME query parameter > most current time.
    add_time_parameter(thisRequest, theRequest, wmsConfig, layer, path_time, path_origintime);

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
      Dali::JsonTools::apply_variant_references(json, layer);
      Spine::JSON::preprocess(json, root, layers_root, wmsConfig.getJsonCache());
      Spine::JSON::dereference(json);
      Dali::JsonTools::apply_variant(json, layer);
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
      // A freshly constructed State defaults to useWms(false), which resolves
      // CSS, markers, patterns, filters and gradients under the Dali root
      // instead of the WMS root. Carry the flag over from the State the plugin
      // configured for this request, or every product referencing a CSS file
      // fails with "Failed to find CSS file" the moment a dimension is given.
      dimState->useWms(theState.useWms());
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
 * \brief Serve a WMTS GetFeatureInfo request (RESTful FeatureInfo resource)
 *
 * Deliberately a thin translation: the tile address is converted into the
 * WMS GetFeatureInfo vocabulary (BBOX/WIDTH/HEIGHT of the addressed tile,
 * I/J passed through) and the request is delegated to the WMS handler, so
 * both services share one feature-info implementation and output templates.
 */
// -----------------------------------------------------------------------
QueryStatus Handler::handleGetFeatureInfo(const Spine::Reactor& theReactor,
                                          Dali::State& theState,
                                          const Spine::HTTP::Request& theRequest,
                                          Spine::HTTP::Response& theResponse,
                                          const std::string& layer,
                                          const std::string& style,
                                          const std::string& tms_id,
                                          const std::string& tm_id,
                                          unsigned tile_row,
                                          unsigned tile_col,
                                          unsigned pixel_j,
                                          unsigned pixel_i,
                                          const std::string& info_format,
                                          const std::vector<std::string>& dimensionValues)
{
  try
  {
    if (itsWMSHandler == nullptr)
      throw Fmi::Exception(BCP, "WMS handler not wired to the WMTS handler");

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

    if (pixel_i >= tm->tile_width || pixel_j >= tm->tile_height)
    {
      sendException("PointIJOutOfRange",
                    fmt::format("Pixel ({},{}) out of range ({}x{})",
                                pixel_i, pixel_j, tm->tile_width, tm->tile_height),
                    theState, theRequest, theResponse);
      return QueryStatus::OK;
    }

    TileBBox bbox = computeTileBBox(*tms, *tm, tile_row, tile_col);

    // Build the WMS GetFeatureInfo request. WMS 1.3.0 BBOX uses the CRS axis
    // order: lat,lon for geographic CRSs, x,y otherwise (same convention the
    // GetTile path uses for the Dali projection bbox).
    auto thisRequest = theRequest;
    thisRequest.addParameter("service", "WMS");
    thisRequest.addParameter("request", "GetFeatureInfo");
    thisRequest.addParameter("version", "1.3.0");
    thisRequest.addParameter("layers", layer);
    thisRequest.addParameter("query_layers", layer);
    thisRequest.addParameter("styles", style == "default" ? "" : style);
    thisRequest.addParameter("crs", tms->crs);
    thisRequest.addParameter("bbox",
                             tms->is_geographic
                                 ? fmt::format("{},{},{},{}",
                                               bbox.min_y, bbox.min_x, bbox.max_y, bbox.max_x)
                                 : fmt::format("{},{},{},{}",
                                               bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y));
    thisRequest.addParameter("width", Fmi::to_string(tm->tile_width));
    thisRequest.addParameter("height", Fmi::to_string(tm->tile_height));
    thisRequest.addParameter("format", "image/png");
    thisRequest.addParameter("info_format", info_format);
    thisRequest.addParameter("i", Fmi::to_string(pixel_i));
    thisRequest.addParameter("j", Fmi::to_string(pixel_j));

    // Dimensions from the RESTful path (Time, Reference_time, Elevation) —
    // same positional mapping as GetTile.
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
          thisRequest.addParameter(
              "time", resolveCurrentTime(itsWMTSConfig->wmsConfig(), layer, v, ""));
        else if (nm == "reference_time")
          thisRequest.addParameter("origintime", v);
        else if (nm == "elevation")
          thisRequest.addParameter("elevation", v);
      }
    }

    return itsWMSHandler->query(theReactor, theState, thisRequest, theResponse);
  }
  catch (...)
  {
    Fmi::Exception ex(BCP, "WMTS GetFeatureInfo failed!", nullptr);
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
                            const Spine::HTTP::Request& /*theRequest*/,
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
    theResponse.setContent(std::move(output));
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
