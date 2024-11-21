#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "WMSConfig.h"
#include "CaseInsensitiveComparator.h"
#include "Layer.h"
#include "LayerFactory.h"
#include "Product.h"
#include "State.h"
#include "View.h"
#include "WMSException.h"
#include "WMSLayerFactory.h"
#include "WMSLayerHierarchy.h"

#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/move/make_unique.hpp>
#include <boost/regex.hpp>
#include <fmt/format.h>
#include <gis/EPSGInfo.h>
#include <gis/SpatialReference.h>
#include <macgyver/Exception.h>
#include <macgyver/FileSystem.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>
#include <spine/Exceptions.h>
#include <spine/FmiApiKey.h>
#include <spine/Json.h>
#include <algorithm>
#include <map>
#include <ogr_spatialref.h>
#include <stdexcept>
#include <string>

using SmartMet::Plugin::Dali::Config;
using SmartMet::Plugin::Dali::Layer;
using SmartMet::Plugin::Dali::Product;
using SmartMet::Plugin::Dali::State;
using SmartMet::Plugin::Dali::View;

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
Json::CharReaderBuilder charreaderbuilder;

// Recursively find the latest modification time for a file in the given directory
void check_modification_time(const std::string& theDir, Fmi::DateTime& max_time)
{
  try
  {
    if (!std::filesystem::exists(theDir) || !std::filesystem::is_directory(theDir))
      return;

    std::filesystem::directory_iterator end_itr;
    for (std::filesystem::directory_iterator itr(theDir); itr != end_itr; ++itr)
    {
      if (SmartMet::Spine::Reactor::isShuttingDown())
        return;

      if (is_directory(itr->status()))
      {
        std::string subdir = itr->path().filename().string();
        check_modification_time(theDir + subdir + "/", max_time);
      }
      else if (is_regular_file(itr->status()))
      {
        std::string filename = (theDir + itr->path().filename().string());
        std::optional<std::time_t> time_t_mod = Fmi::last_write_time(filename);
        if (time_t_mod)
        {
          auto ptime_mod = Fmi::date_time::from_time_t(*time_t_mod);
          if (max_time < ptime_mod)
            max_time = ptime_mod;
        }
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed getting maximum modification time!");
  }
}

/*
 * namespace patterns look like "/..../"
 */

bool looks_like_pattern(const std::string& pattern)
{
  return (boost::algorithm::starts_with(pattern, "/") && boost::algorithm::ends_with(pattern, "/"));
}

/*
 * Apply namespace filtering as in GeoServer with regex extension
 */

bool match_namespace_pattern(const std::string& name, const std::string& pattern)
{
  if (!looks_like_pattern(pattern))
    return (boost::algorithm::istarts_with(name, pattern + ":") || name == pattern);

  // Strip surrounding slashes first
  const std::string re_str = pattern.substr(1, pattern.size() - 2);
  const boost::regex re(re_str, boost::regex::icase);
  return boost::regex_search(name, re);
}

// Create layer name from customer name and the path to the configuration file
std::string make_layer_namespace(const std::string& customer,
                                 const std::string& productRoot,
                                 const std::string& fileDir)
{
  try
  {
    if (productRoot == fileDir)
    {
      // No directory structure here, return default namespace
      return customer;
    }

    std::vector<std::string> tokens;
    std::vector<std::string> namespace_tokens;
    boost::algorithm::split(tokens, fileDir, boost::is_any_of("/"), boost::token_compress_on);

    std::string currentPath;

    do
    {
      // Safety check needed for some incorrect specifications
      if (tokens.empty())
        throw Fmi::Exception(BCP, "Failed to generate layer namespace")
            .addParameter("customer", customer)
            .addParameter("file directory", fileDir)
            .addParameter("root directory", productRoot);

      namespace_tokens.push_back(tokens.back());
      tokens.pop_back();
      currentPath = boost::algorithm::join(tokens, "/");

    } while (currentPath != productRoot);

    namespace_tokens.push_back(customer);

    std::reverse(namespace_tokens.begin(), namespace_tokens.end());

    return boost::algorithm::join(namespace_tokens, ":");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Making layer namespace failed!");
  }
}

// If legend layer exists use it directly otherwise generate legend from configuration
// Returns true if the product contains a legend in itself
bool prepareLegendGraphic(Product& theProduct)
{
  std::list<std::shared_ptr<View>>& views = theProduct.views.views;
  std::shared_ptr<Layer> legendLayer;

  bool legendLayerFound = false;
  for (const auto& view : views)
  {
    std::list<std::shared_ptr<Layer>> layers = view->layers.layers;

    for (const auto& layer : layers)
    {
      std::list<std::shared_ptr<Layer>> sublayers = layer->layers.layers;

      if (layer->type)
      {
        if (*layer->type == "legend")
        {
          legendLayer = layer;
          legendLayerFound = true;
          break;
        }
      }

      for (const auto& sublayer : sublayers)
        std::list<std::shared_ptr<Layer>> sublayers2 = sublayer->layers.layers;
    }
    if (legendLayerFound)
      break;
  }

  if (legendLayer)
  {
    // legend layer found use it
    auto it = views.begin();
    it++;
    views.erase(it, views.end());

    auto view = *(views.begin());
    auto& layers = view->layers.layers;

    layers.erase(layers.begin(), layers.end());
    layers.push_back(legendLayer);

    return true;
  }

  // delete all layers and generate legend layer from configuration
  auto it = views.begin();
  it++;
  views.erase(it, views.end());
  auto view = *(views.begin());
  auto& layers = view->layers.layers;
  layers.erase(layers.begin(), layers.end());

  return false;
}

/*
 * We must not try to read backup files since they may not be in a valid state
 */

bool looks_valid_filename(const std::string& name)
{
  try
  {
    if (name.empty())
      return false;
    if (name[0] == '.')
      return false;
    if (name[name.size() - 1] == '~')
      return false;
    if (!boost::algorithm::ends_with(name, ".json"))
      return false;
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Filename validation failed!");
  }
}

/*
 * libconfig aggregates cannot be converted directly to CTPP counterparts,
 * but it can be done for scalars.
 */

void set_scalar(CTPP::CDT& tmpl,
                const libconfig::Config& config,
                const std::string& path,
                const std::string& variable)
{
  const auto& value = config.lookup(path);

  switch (value.getType())
  {
    case libconfig::Setting::Type::TypeInt:
    case libconfig::Setting::Type::TypeBoolean:
      tmpl[variable] = static_cast<int>(value);
      break;
    case libconfig::Setting::Type::TypeInt64:
      tmpl[variable] = static_cast<long>(value);
      break;
    case libconfig::Setting::Type::TypeFloat:
      tmpl[variable] = static_cast<double>(value);
      break;
    case libconfig::Setting::Type::TypeString:
      tmpl[variable] = value.c_str();
      break;

    case libconfig::Setting::Type::TypeArray:
    case libconfig::Setting::Type::TypeList:
    case libconfig::Setting::Type::TypeGroup:
    case libconfig::Setting::Type::TypeNone:
      throw Fmi::Exception(BCP, path + " value must be a scalar");
  }
}

void set_mandatory(CTPP::CDT& tmpl,
                   const libconfig::Config& config,
                   const std::string& prefix,
                   const std::string& variable)
{
  std::string path = prefix + "." + variable;
  set_scalar(tmpl, config, path, variable);
}

void set_optional(CTPP::CDT& tmpl,
                  const libconfig::Config& config,
                  const std::string& prefix,
                  const std::string& variable)
{
  std::string path = prefix + "." + variable;
  if (!config.exists(path))
    return;
  set_scalar(tmpl, config, path, variable);
}

// Update last known modification times given a filename
void update_product_modification_time(const std::string& filename,
                                      std::map<std::string, std::time_t>& modification_times)
{
  const std::optional<std::time_t> modtime = Fmi::last_write_time(filename);

  if (modtime)
  {
    auto pos = modification_times.find(filename);

    if (pos == modification_times.end())
      modification_times.insert({filename, *modtime});
    else
    {
      const std::time_t previous_time = pos->second;
      if (*modtime != 0 && *modtime != previous_time)
      {
        pos->second = *modtime;

        // Product file is reloaded when the map is next time requested
        auto message = Spine::log_time_str() + " File " + filename + " modified, reloading it\n";
        std::cout << message << std::flush;
      }
    }
  }
  else
  {
    Fmi::Exception err(BCP, "Failed to get file last write time");
    err.addParameter("Path", filename);
    throw err;
  }
}

// Update capabilities modification time
void update_capabilities_modification_time(Fmi::DateTime& mod_time, const SharedWMSLayer& layer)
{
  mod_time = std::max(mod_time, layer->modificationTime());
}

}  // namespace

CTPP::CDT WMSConfig::get_request(const libconfig::Config& config,
                                 const std::string& prefix,
                                 const std::string& variable) const
{
  CTPP::CDT request(CTPP::CDT::HASH_VAL);

  const std::string path = prefix + "." + variable;

  CTPP::CDT format_list(CTPP::CDT::ARRAY_VAL);
  if (variable != "getmap")
  {
    const auto& formats = config.lookup(path + ".format");
    if (!formats.isArray())
      throw Fmi::Exception(BCP, path + ".format  must be an array");

    for (int i = 0; i < formats.getLength(); i++)
      format_list.PushBack(formats[i].c_str());
  }
  else
  {
    for (const auto& format : itsSupportedMapFormats)
      format_list.PushBack(format);
  }
  request["format"] = format_list;

  const auto& dcptypes = config.lookup(path + ".dcptype");
  if (dcptypes.isArray())
    throw Fmi::Exception(BCP, path + ".dcptype must be an array");
  CTPP::CDT dcptype_list(CTPP::CDT::ARRAY_VAL);

  for (int i = 0; i < dcptypes.getLength(); i++)
  {
    const auto& dcptype_http = dcptypes[i]["http"];

    CTPP::CDT http(CTPP::CDT::HASH_VAL);
    CTPP::CDT get(CTPP::CDT::HASH_VAL);

    get["online_resource"] = dcptype_http["get"]["online_resource"].c_str();
    http["get"] = get;

    if (dcptype_http.exists("post"))
    {
      CTPP::CDT post(CTPP::CDT::HASH_VAL);
      post["online_resource"] = dcptype_http["post"]["online_resource"].c_str();
      http["post"] = post;
    }

    CTPP::CDT dcptype_element(CTPP::CDT::HASH_VAL);
    dcptype_element["http"] = http;
    dcptype_list.PushBack(dcptype_element);
  }
  request["dcptype"] = dcptype_list;

  return request;
}

CTPP::CDT WMSConfig::get_capabilities(const libconfig::Config& config) const
{
  // Extract capabilities into a CTTP hash

  CTPP::CDT capabilities(CTPP::CDT::HASH_VAL);

  set_mandatory(capabilities, config, "wms.get_capabilities", "version");
  set_optional(capabilities, config, "wms.get_capabilities", "headers");

  // Service part of capabilities

  CTPP::CDT service(CTPP::CDT::HASH_VAL);

  set_mandatory(service, config, "wms.get_capabilities.service", "title");
  set_optional(service, config, "wms.get_capabilities.service", "abstract");

  if (config.exists("wms.get_capabilities.service.keywords"))
  {
    CTPP::CDT keywords(CTPP::CDT::ARRAY_VAL);

    const auto& settings = config.lookup("wms.get_capabilities.service.keywords");

    if (!settings.isArray())
      throw Fmi::Exception(BCP, "wms.get_capabilities.service.keywords must be an array");

    for (int i = 0; i < settings.getLength(); i++)
      keywords.PushBack(settings[i].c_str());

    service["keywords"] = keywords;
  }

  set_mandatory(service, config, "wms.get_capabilities.service", "online_resource");

  if (config.exists("wms.get_capabilities.service.contact_information"))
  {
    CTPP::CDT contact_information(CTPP::CDT::HASH_VAL);

    if (config.exists("wms.get_capabilities.service.contact_information.contact_person_primary"))
    {
      CTPP::CDT contact_person_primary(CTPP::CDT::HASH_VAL);
      set_mandatory(contact_person_primary,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_person_primary",
                    "contact_person");
      set_mandatory(contact_person_primary,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_person_primary",
                    "contact_organization");
      contact_information["contact_person_primary"] = contact_person_primary;
    }

    set_optional(contact_information,
                 config,
                 "wms.get_capabilities.service.contact_information",
                 "contact_position");

    if (config.exists("wms.get_capabilities.service.contact_information.contact_address"))
    {
      CTPP::CDT contact_address(CTPP::CDT::HASH_VAL);
      set_mandatory(contact_address,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_address",
                    "address_type");
      set_mandatory(contact_address,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_address",
                    "address");
      set_mandatory(contact_address,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_address",
                    "city");
      set_mandatory(contact_address,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_address",
                    "state_or_province");
      set_mandatory(contact_address,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_address",
                    "post_code");
      set_mandatory(contact_address,
                    config,
                    "wms.get_capabilities.service.contact_information.contact_address",
                    "country");
      contact_information["contact_address"] = contact_address;
    }

    set_optional(contact_information,
                 config,
                 "wms.get_capabilities.service.contact_information",
                 "contact_voice_telephone");
    set_optional(contact_information,
                 config,
                 "wms.get_capabilities.service.contact_information",
                 "contact_facsimile_telephone");
    set_optional(contact_information,
                 config,
                 "wms.get_capabilities.service.contact_information",
                 "contact_electronic_mail_address");

    service["contact_information"] = contact_information;
  }

  set_optional(service, config, "wms.get_capabilities.service", "fees");
  set_optional(service, config, "wms.get_capabilities.service", "access_constraints");
  set_optional(service, config, "wms.get_capabilities.service", "layer_limit");
  set_optional(service, config, "wms.get_capabilities.service", "max_width");
  set_optional(service, config, "wms.get_capabilities.service", "max_height");

  capabilities["service"] = service;

  // Capability part

  CTPP::CDT capability(CTPP::CDT::HASH_VAL);

  // Request subpart

  CTPP::CDT request(CTPP::CDT::HASH_VAL);

  // Two obligatory parts and one optional one
  request["getcapabilities"] =
      get_request(config, "wms.get_capabilities.capability.request", "getcapabilities");
  request["getmap"] = get_request(config, "wms.get_capabilities.capability.request", "getmap");

  if (config.exists("wms.get_capabilities.capability.request.getfeatureinfo"))
    request["getfeatureinfo"] =
        get_request(config, "wms.get_capabilities.capability.request", "getfeatureinfo");

  capability["request"] = request;

  // Exceptions

  const auto& exceptions = config.lookup("wms.get_capabilities.capability.exception");
  if (!exceptions.isArray())
    throw Fmi::Exception(BCP, "wms.get_capabilities.capability.exception must be an array");
  CTPP::CDT exception_list(CTPP::CDT::ARRAY_VAL);
  for (int i = 0; i < exceptions.getLength(); i++)
    exception_list.PushBack(exceptions[i].c_str());
  capability["exception"] = exception_list;

  // Extensions

  if (config.exists("wms.get_capabilities.capability.extended_capabilities.inspire"))
  {
    CTPP::CDT extended_capabilities(CTPP::CDT::HASH_VAL);
    CTPP::CDT inspire(CTPP::CDT::HASH_VAL);

    const std::string prefix = "wms.get_capabilities.capability.extended_capabilities.inspire";
    set_mandatory(inspire, config, prefix, "metadata_url");
    set_mandatory(inspire, config, prefix, "default_language");
    set_mandatory(inspire, config, prefix, "supported_language");
    set_mandatory(inspire, config, prefix, "response_language");
    extended_capabilities["inspire"] = inspire;
    capability["extended_capabilities"] = extended_capabilities;
  }

  // The master layer settings

  CTPP::CDT master_layer(CTPP::CDT::HASH_VAL);
  set_mandatory(master_layer, config, "wms.get_capabilities.capability.master_layer", "title");
  set_optional(master_layer, config, "wms.get_capabilities.capability.master_layer", "abstract");
  set_optional(master_layer, config, "wms.get_capabilities.capability.master_layer", "queryable");
  set_optional(master_layer, config, "wms.get_capabilities.capability.master_layer", "opaque");
  set_optional(master_layer, config, "wms.get_capabilities.capability.master_layer", "cascaded");
  set_optional(master_layer, config, "wms.get_capabilities.capability.master_layer", "no_subsets");
  set_optional(master_layer, config, "wms.get_capabilities.capability.master_layer", "fixed_width");
  set_optional(
      master_layer, config, "wms.get_capabilities.capability.master_layer", "fixed_height");
  capability["master_layer"] = master_layer;

  // Finished the first part of the response, the capability layers will be added later

  capabilities["capability"] = capability;

  return capabilities;
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract supported units and EPSG, CRS and AUTO2 references
 */
// ----------------------------------------------------------------------

void WMSConfig::parse_references()
{
  const libconfig::Config& config = itsDaliConfig.getConfig();

  // Units settings is an array of unit,scale,name typles. The data is used when generating AUTO2
  // spatial references

  std::string units_name = "wms.supported_references.units";
  if (!config.exists(units_name))
  {
    itsLengthUnits["m"] = 1.0;  // enable meters by default
  }
  else
  {
    const auto& units_settings = config.lookup(units_name);
    if (!units_settings.isList())
      throw Fmi::Exception(BCP, units_name + " must be a list of groups");

    for (int i = 0; i < units_settings.getLength(); i++)
    {
      const auto& group = units_settings[i];

      if (!group.isGroup())
        throw Fmi::Exception(BCP, "units settings must be a list of groups");

      // we ignore the name of the units, it's for documentation only
      std::string unit = group["unit"];
      double scale = group["scale"];

      itsLengthUnits[unit] = scale;
    }
  }

  // EPSG settings is an array of integers, whose name shall be EPSG:<int>

  const auto& epsg_settings = config.lookup("wms.supported_references.epsg");

  if (!epsg_settings.isArray())
    throw Fmi::Exception(BCP, "wms.supported_references.epsg must be an array of integers");

  for (int i = 0; i < epsg_settings.getLength(); i++)
  {
    int num = epsg_settings[i];
    std::string name = "EPSG:" + Fmi::to_string(num);
    auto epsginfo = Fmi::EPSGInfo::getInfo(num);
    if (!epsginfo)
      throw Fmi::Exception(BCP, "Unknown EPSG in WMS config EPSG list")
          .addParameter("EPSG", Fmi::to_string(num));

    const auto& bbox = epsginfo->bbox;
    bool enabled = true;
    Fmi::SpatialReference crs(name);
    bool geographic = crs.isGeographic();

    WMSSupportedReference ref(name, bbox, enabled, geographic);
    itsWMSSupportedReferences.insert(std::make_pair(name, ref));
  }

  // CRS settings is a list of id,proj,bbox tuples, whose name shall be CRS:<id>

  const auto& crs_settings = config.lookup("wms.supported_references.crs");

  if (!crs_settings.isList())
    throw Fmi::Exception(BCP, "wms.supported_references.crs must be a list");

  for (int i = 0; i < crs_settings.getLength(); i++)
  {
    const auto& group = crs_settings[i];

    if (!group.isGroup())
      throw Fmi::Exception(BCP, "wms.supported_references.crs must be a list of groups");

    std::string id = group["id"];
    std::string proj = group["proj"];
    std::string name = "CRS:" + id;

    bool enabled = true;
    group.lookupValue("enabled", enabled);

    const auto& bbox_array = group["bbox"];
    if (!bbox_array.isArray())
      throw Fmi::Exception(BCP, "wms.supported_references.crs bboxes must be arrays");

    if (bbox_array.getLength() != 4)
      throw Fmi::Exception(BCP, "wms.supported_references.crs bboxes must have 4 elements");

    double west = bbox_array[0];
    double east = bbox_array[1];
    double south = bbox_array[2];
    double north = bbox_array[3];
    Fmi::BBox bbox(west, east, south, north);

    Fmi::SpatialReference crs(proj);
    bool geographic = crs.isGeographic();

    WMSSupportedReference ref(proj, bbox, enabled, geographic);
    itsWMSSupportedReferences.insert(std::make_pair(name, ref));
  }

  // AUTO2 settings is a list of id, proj, bbox tuple, whose name shall be AUTO2:<id>

  std::string auto2_name = "wms.supported_references.auto2";
  if (config.exists(auto2_name))
  {
    const auto& auto2_settings = config.lookup(auto2_name);

    if (!auto2_settings.isList())
      throw Fmi::Exception(BCP, auto2_name + " must be a list of groups");

    for (int i = 0; i < auto2_settings.getLength(); i++)
    {
      const auto& group = auto2_settings[i];

      if (!group.isGroup())
        throw Fmi::Exception(BCP, auto2_name + " must be a list of groups");

      const int id = group["id"];
      std::string proj = group["proj"];

      itsAutoProjections[id] = proj;

      const Fmi::BBox bbox(-180, 180, -90, 90);
      const bool enabled = true;
      const bool geographic = false;
      const std::string name = fmt::format("AUTO2:{}", id);

      WMSSupportedReference ref(name, bbox, enabled, geographic);
      itsWMSSupportedReferences.insert(std::make_pair(name, ref));
    }
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize a WMS configuration
 *
 * Note: heavy tasks are performed in init() to enable quicker shutdowns
 */
// ----------------------------------------------------------------------

WMSConfig::WMSConfig(const Config& daliConfig,
                     const Spine::JsonCache& theJsonCache,
                     Engine::Querydata::Engine* qEngine,
#ifndef WITHOUT_AUTHENTICATION
                     Engine::Authentication::Engine* authEngine,
#endif
#ifndef WITHOUT_OBSERVATION
                     Engine::Observation::Engine* obsEngine,
#endif
                     Engine::Gis::Engine* gisEngine,
                     Engine::Grid::Engine* gridEngine)
    : itsDaliConfig(daliConfig),
      itsJsonCache(theJsonCache),
      itsQEngine(qEngine),
      itsGisEngine(gisEngine),
      itsGridEngine(gridEngine),
#ifndef WITHOUT_AUTHENTICATION
      itsAuthEngine(authEngine),
#endif
#ifndef WITHOUT_OBSERVATION
      itsObsEngine(obsEngine),
#endif
      itsLegendGraphicSettings(daliConfig.getConfig())
{
  try
  {
    const libconfig::Config& config = daliConfig.getConfig();

    config.lookupValue("wms.get_capabilities.disable_updates", itsCapabilityUpdatesDisabled);
    config.lookupValue("wms.get_capabilities.update_interval", itsCapabilityUpdateInterval);
    config.lookupValue("wms.get_capabilities.expiration_time", itsCapabilityExpirationTime);

    const auto& exceptions = config.lookup("wms.get_capabilities.capability.exception");
    if (!exceptions.isArray())
      throw Fmi::Exception(BCP, "wms.get_capabilities.capability.exception must be an array");
    for (int i = 0; i < exceptions.getLength(); i++)
      itsSupportedWMSExceptions.insert(exceptions[i].c_str());

    const auto& getmap_formats =
        config.lookup("wms.get_capabilities.capability.request.getmap.format");

    if (!getmap_formats.isArray())
      throw Fmi::Exception(
          BCP, "wms.get_capabilities.capability.request.getmap.format must be an array");

    for (int i = 0; i < getmap_formats.getLength(); i++)
      itsSupportedMapFormats.insert(getmap_formats[i].c_str());

    const auto& capability_formats =
        config.lookup("wms.get_capabilities.capability.request.getcapabilities.format");
    if (!capability_formats.isArray())
      throw Fmi::Exception(
          BCP, "wms.get_capabilities.capability.request.getcapabilities.format must be an array");
    for (int i = 0; i < capability_formats.getLength(); i++)
      itsSupportedWMSGetCapabilityFormats.insert(capability_formats[i].c_str());

    std::string wmsVersions = config.lookup("wms.supported_versions").c_str();
    boost::algorithm::split(itsSupportedWMSVersions, wmsVersions, boost::algorithm::is_any_of(","));
    parse_references();

    config.lookupValue("wms.margin", itsMargin);
    // Default layout is flat
    std::string layout = "flat";
    // Layout can be defined in configuration file
    config.lookupValue("wms.get_capabilities.layout", layout);
    if (layout == "flat")
      itsLayerHierarchyType = WMSLayerHierarchy::HierarchyType::flat;
    else if (layout == "recursive")
      itsLayerHierarchyType = WMSLayerHierarchy::HierarchyType::recursive;
    else if (layout == "recursivetimes")
      itsLayerHierarchyType = WMSLayerHierarchy::HierarchyType::recursivetimes;
    else
      throw Fmi::Exception::Trace(
          BCP, ("Error! Invalid layout defined in configuration file: " + layout));

    config.lookupValue("wms.get_capabilities.enableintervals", itsMultipleIntervals);

    // Parse GetCapability settings once to make sure the config file is valid
    get_capabilities(config);
  }
  catch (...)
  {
    Spine::Exceptions::handle("WMS plugin");
  }
}

WMSConfig::~WMSConfig()
{
  if (itsGetCapabilitiesTask)
  {
    std::cout << "ERROR [WMS][WMSConfig]: Missing call to WMSConfig::shutdown(). Terminating..."
              << std::endl;
    abort();
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Heavy initializations are done outside the constructor
 *
 * The idea is to do a fast construction so that shutdown requests can
 * be passed faster to the class.
 */
// ----------------------------------------------------------------------

void WMSConfig::init()
{
  if (Spine::Reactor::isShuttingDown())
    return;

  // Do first layer scan
  updateLayerMetaData();

  if (Spine::Reactor::isShuttingDown())
    return;

  // Begin the update loop

  if (!itsCapabilityUpdatesDisabled)
  {
    itsGetCapabilitiesTask.reset(new Fmi::AsyncTask("WMSConfig: capabilities update task",
                                                    [this]() { capabilitiesUpdateLoop(); }));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Shutdown
 */
// ----------------------------------------------------------------------

void WMSConfig::shutdown()
{
  try
  {
    if (itsGetCapabilitiesTask)
    {
      itsGetCapabilitiesTask->cancel();
      itsGetCapabilitiesTask->wait();
      itsGetCapabilitiesTask.reset();
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Starting shutdown failed!");
  }
}

void WMSConfig::capabilitiesUpdateLoop()
{
  try
  {
    while (!Spine::Reactor::isShuttingDown())
    {
      try
      {
        // update capabilities every N seconds
        // FIXME: do we need to put interruption points into methods called below?
        boost::this_thread::sleep_for(boost::chrono::seconds(itsCapabilityUpdateInterval));
        updateLayerMetaData();
        updateModificationTime();
      }
      catch (...)
      {
        Fmi::Exception exception(BCP, "Could not update capabilities!", nullptr);
        exception.printError();
      }
    }
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Could not update capabilities!", nullptr);
    exception.printError();
  }
}

#ifndef WITHOUT_AUTHENTICATION
bool WMSConfig::validateGetMapAuthorization(const Spine::HTTP::Request& theRequest) const
{
  // Everything ok if authentication is disabled
  if (itsAuthEngine == nullptr)
    return true;

  try
  {
    auto layersopt = theRequest.getParameter("LAYERS");
    if (!layersopt)
      return true;

    std::string layerstring = *layersopt;

    if (layerstring.empty())
      return true;

    // Get apikey
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);
    if (!apikey)
      return true;

    std::vector<std::string> layers;
    boost::algorithm::split(layers, layerstring, boost::algorithm::is_any_of(","));

    if (layers.empty())
      return true;

    // All layers must match in order to grant access
    return itsAuthEngine->authorize(*apikey, layers, "wms");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetMap authorization failed!");
  }
}
#endif

void warn_layer(const std::string& badfile, std::set<std::string>& warned_files)
{
  if (warned_files.find(badfile) != warned_files.end())
    return;

  Fmi::Exception exception(BCP, "Failed to parse configuration!", nullptr);
  exception.addParameter("Path", badfile);
  exception.printError();

  // Don't warn again about the same file
  warned_files.insert(badfile);
}

void WMSConfig::updateLayerMetaDataForCustomerLayer(
    const std::filesystem::recursive_directory_iterator& itr,
    const std::string& customer,
    const std::string& productdir,
    const std::shared_ptr<LayerMap>& mylayers,
    LayerMap& newProxies,
    std::map<SharedWMSLayer, std::map<std::string, std::string>>& externalLegends)
{
  if (!is_regular_file(itr->status()))
    return;

  const auto pathname = itr->path().string();
  const auto filename = itr->path().filename().string();  // used in catch block for error message

  if (!looks_valid_filename(filename))
    return;

  try
  {
    const auto layername = itr->path().stem().string();

    // Determine namespace from directory structure
    std::string layerNamespace =
        make_layer_namespace(customer, productdir, itr->path().parent_path().string());

    const auto fullLayername = layerNamespace + ":" + layername;

    // Check for modified product files here

    update_product_modification_time(pathname, itsProductFileModificationTime);

    // Se if the metadata has expired and hence the layer must be created from scracth
    // to update its available times etc

    bool mustUpdate = true;

    if (mylayers && mylayers->find(fullLayername) != mylayers->end())
    {
      const auto& oldProxy = mylayers->at(fullLayername);
      SharedWMSLayer oldLayer = oldProxy.getLayer();
      const auto timestamp = oldLayer->metaDataUpdateTime();

      bool expired = false;
      if (!timestamp.is_not_a_date_time())
      {
        const auto age = Fmi::SecondClock::universal_time() - timestamp;
        expired = (age.total_seconds() >= oldLayer->metaDataUpdateInterval());
      }

      // check if metadata need to be updated
      // for example for icemaps it is not necessary so often
      mustUpdate = (expired || oldLayer->mustUpdateLayerMetaData());
    }

    // Insert old or recreated layer into the layers to be published next

    if (!mustUpdate)
    {
      const auto& oldProxy = mylayers->at(fullLayername);
      newProxies.insert({fullLayername, oldProxy});
    }
    else
    {
      auto newlayers = WMSLayerFactory::createWMSLayers(
          pathname, fullLayername, layerNamespace, customer, *this);

      if (newlayers.empty())
        warn_layer(pathname, itsWarnedFiles);
      else
      {
        for (const auto& newlayer : newlayers)
        {
          if (newlayer)
          {
            update_capabilities_modification_time(itsCapabilitiesModificationTime, newlayer);

            if (!newlayer->getLegendFiles().empty())
              externalLegends[newlayer] = newlayer->getLegendFiles();

            WMSLayerProxy newProxy(itsGisEngine, newlayer);
            auto newName = newlayer->getName();      // JSON may override layer name
            newProxies.insert({newName, newProxy});  // so not using fullLayername here
          }
        }
      }
    }
  }
  catch (...)
  {
    // Ignore and report failed product definitions
    warn_layer(pathname, itsWarnedFiles);
  }
}

void WMSConfig::updateLayerMetaDataForCustomer(
    const std::filesystem::directory_iterator& dir,
    const std::shared_ptr<LayerMap>& mylayers,
    LayerMap& newProxies,
    std::map<SharedWMSLayer, std::map<std::string, std::string>>& externalLegends)
{
  if (!is_directory(dir->status()))
    return;

  const auto productdir = dir->path().string() + "/products";
  const auto customer = dir->path().filename().string();

  std::filesystem::recursive_directory_iterator end_prod_itr;

  // Find product (layer) definitions
  for (std::filesystem::recursive_directory_iterator itr(productdir); itr != end_prod_itr; ++itr)
  {
    if (Spine::Reactor::isShuttingDown())
      return;

    try
    {
      updateLayerMetaDataForCustomerLayer(
          itr, customer, productdir, mylayers, newProxies, externalLegends);
    }
    catch (...)
    {
      Fmi::Exception exception(
          BCP,
          "Lost " + std::string(itr->path().c_str()) + " while scanning the filesystem!",
          nullptr);
      exception.addParameter("Path", itr->path().c_str());
      exception.printError();
    }
  }
}

void WMSConfig::updateLayerMetaData()
{
  try
  {
    auto mylayers = itsLayers.load();

    // New shared pointer which will be atomically set into production
    auto newProxies = std::make_shared<LayerMap>();

    const auto wms_mode_on = true;
    const auto customerdir = itsDaliConfig.rootDirectory(wms_mode_on) + "/customers";

    std::map<SharedWMSLayer, std::map<std::string, std::string>> externalLegends;

    std::filesystem::directory_iterator end_itr;
    for (std::filesystem::directory_iterator itr(customerdir); itr != end_itr; ++itr)
    {
      if (Spine::Reactor::isShuttingDown())
        return;

      try
      {
        updateLayerMetaDataForCustomer(itr, mylayers, *newProxies, externalLegends);
      }
      catch (...)
      {
        Fmi::Exception exception(
            BCP,
            "Lost " + std::string(itr->path().c_str()) + " while scanning the filesystem!",
            nullptr);
        exception.addParameter("Path", itr->path().c_str());
        exception.printError();
      }
    }

    // It external legend files are used set legend dimension here
    for (auto& externalLegendItem : externalLegends)
    {
      const auto& externalLegendItems = externalLegendItem.second;
      for (auto& proxyItem : *newProxies)
      {
        for (const auto& legendStyleItem : externalLegendItems)
        {
          if (proxyItem.second.getLayer()->getName() == legendStyleItem.second)
          {
            externalLegendItem.first->setLegendDimension(*proxyItem.second.getLayer(),
                                                         legendStyleItem.first);
          }
        }
      }
    }

    itsLayers.store(newProxies);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Layer metadata update failed!");
  }
}

void WMSConfig::updateModificationTime()
{
  try
  {
    auto max_time = itsCapabilitiesModificationTime;

    // Check all files under WMS-root
    check_modification_time(itsDaliConfig.rootDirectory(true) + "/", max_time);

    if (itsCapabilitiesModificationTime < max_time)
      itsCapabilitiesModificationTime = max_time;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Layer metadata update failed!");
  }
}

#ifndef WITHOUT_AUTHENTICATION
CTPP::CDT WMSConfig::getCapabilities(const std::optional<std::string>& apikey,
                                     const std::string& language,
                                     const std::optional<std::string>& starttime,
                                     const std::optional<std::string>& endtime,
                                     const std::optional<std::string>& reference_time,
                                     const std::optional<std::string>& wms_namespace,
                                     WMSLayerHierarchy::HierarchyType hierarchy_type,
                                     bool show_hidden,
                                     bool multiple_intervals,
                                     bool authenticate) const
#else
CTPP::CDT WMSConfig::getCapabilities(const std::optional<std::string>& apikey,
                                     const std::string& language,
                                     const std::optional<std::string>& starttime,
                                     const std::optional<std::string>& endtime,
                                     const std::optional<std::string>& reference_time,
                                     const std::optional<std::string>& wms_namespace,
                                     WMSLayerHierarchy::HierarchyType hierarchy_type,
                                     bool show_hidden,
                                     bool multiple_intervals) const
#endif
{
  try
  {
    // Atomic copy of layer data
    auto my_layers = itsLayers.load();

    if (hierarchy_type != WMSLayerHierarchy::HierarchyType::flat)
    {
#ifndef WITHOUT_AUTHENTICATION
      WMSLayerHierarchy lh(*my_layers,
                           wms_namespace,
                           hierarchy_type,
                           show_hidden,
                           apikey,
                           authenticate,
                           itsAuthEngine);
#else
      WMSLayerHierarchy lh(*my_layers, wms_namespace, hierarchy_type, show_hidden);
#endif

      //	  std::cout << "Hierarchy:\n" << lh << std::endl;

      return lh.getCapabilities(multiple_intervals,
                                language,
                                itsDaliConfig.defaultLanguage(),
                                starttime,
                                endtime,
                                reference_time);
    }

    // Return array of individual layer capabilities
    CTPP::CDT layersCapabilities(CTPP::CDT::ARRAY_VAL);

    const std::string wmsService = "wms";

    for (const auto& iter_pair : *my_layers)
    {
#ifndef WITHOUT_AUTHENTICATION
      const auto& layer_name = iter_pair.first;
      // If authentication is requested, skip the layer if authentication fails
      if (apikey && authenticate)
        if (itsAuthEngine == nullptr || !itsAuthEngine->authorize(*apikey, layer_name, wmsService))
          continue;
#endif

      auto cdt = iter_pair.second.getCapabilities(multiple_intervals,
                                                  show_hidden,
                                                  language,
                                                  itsDaliConfig.defaultLanguage(),
                                                  starttime,
                                                  endtime,
                                                  reference_time);

      // Note: The std::optional is empty for hidden layers.
      if (cdt)
      {
        if (!wms_namespace)
        {
          layersCapabilities.PushBack(*cdt);
        }
        else
        {
          // Return capability only if the namespace matches
          if (cdt->Exists("name"))
          {
            std::string name = (*cdt)["name"].GetString();
            if (match_namespace_pattern(name, *wms_namespace))
              layersCapabilities.PushBack(*cdt);
          }
        }
      }
    }

    return layersCapabilities;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetCapabilities failed!");
  }
}

std::string WMSConfig::layerCustomer(const std::string& theLayerName) const
{
  try
  {
    auto my_layers = itsLayers.load();

    auto it = my_layers->find(theLayerName);
    if (it == my_layers->end())
      return "";  // Should we throw an error!?

    return it->second.getLayer()->getCustomer();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Extracting customer for layer failed!");
  }
}

const std::set<std::string>& WMSConfig::supportedMapFormats() const
{
  return itsSupportedMapFormats;
}
const std::set<std::string>& WMSConfig::supportedWMSVersions() const
{
  return itsSupportedWMSVersions;
}
const std::map<std::string, WMSSupportedReference>& WMSConfig::supportedWMSReferences() const
{
  return itsWMSSupportedReferences;
}
const std::set<std::string>& WMSConfig::supportedWMSExceptions() const
{
  return itsSupportedWMSExceptions;
}
const std::set<std::string>& WMSConfig::supportedWMSGetCapabilityFormats() const
{
  return itsSupportedWMSGetCapabilityFormats;
}

bool WMSConfig::isValidMapFormat(const std::string& theMapFormat) const
{
  // Special case for config JSON requests
  if (theMapFormat == "cnf")
    return true;

  return (itsSupportedMapFormats.find(theMapFormat) != itsSupportedMapFormats.end());
}

bool WMSConfig::isValidVersion(const std::string& theVersion) const
{
  return (itsSupportedWMSVersions.find(theVersion) != itsSupportedWMSVersions.end());
}

bool WMSConfig::isValidLayer(const std::string& theLayer,
                             bool theAcceptHiddenLayerFlag /*= true*/) const
{
  try
  {
    return isValidLayerImpl(theLayer, theAcceptHiddenLayerFlag);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking layer validity failed!");
  }
}

bool WMSConfig::isValidStyle(const std::string& theLayer, const std::string& theStyle) const
{
  try
  {
    if (!isValidLayerImpl(theLayer, true))
      return false;

    // empty means default style
    CaseInsensitiveComparator cicomp;
    if (theStyle.empty() || cicomp(theStyle, "default"))
      return true;

    auto my_layers = itsLayers.load();

    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidStyle(theStyle);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking style validity failed!");
  }
}

bool WMSConfig::isValidInterval(const std::string& theLayer,
                                int interval_start,
                                int interval_end) const
{
  try
  {
    if (!isValidLayerImpl(theLayer, true))
      return false;

    auto my_layers = itsLayers.load();

    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidInterval(interval_start, interval_end);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking style validity failed!");
  }
}

std::pair<std::string, std::string> WMSConfig::getDefaultInterval(const std::string& theLayer) const
{
  try
  {
    std::pair<std::string, std::string> ret{"", ""};

    if (!isValidLayerImpl(theLayer, true))
      return ret;

    auto my_layers = itsLayers.load();

    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->getDefaultInterval();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Getting default interval failed!");
  }
}

std::string WMSConfig::getCRSDefinition(const std::string& theCRS) const
{
  try
  {
    // Normal EPSG and named references
    const auto pos = itsWMSSupportedReferences.find(theCRS);
    if (pos != itsWMSSupportedReferences.end())
      return pos->second.proj;

    if (!boost::algorithm::starts_with(theCRS, "AUTO2:"))
      throw Fmi::Exception(BCP, "Unknown spatial reference").addParameter("CRS", theCRS);

    // Supported AUTO2 projections

    const auto params = theCRS.substr(6, std::string::npos);

    std::vector<std::string> parts;
    boost::algorithm::split(parts, params, boost::algorithm::is_any_of(","));

    if (parts.size() != 4)
      throw Fmi::Exception(BCP, "AUTO2 definitions require 4 parameters: id,scale,lon,lat")
          .addParameter("CRS", theCRS);

    const auto id = Fmi::stoi(parts[0]);
    const auto scale = Fmi::stod(parts[1]);
    const auto lon = Fmi::stod(parts[2]);
    const auto lat = Fmi::stod(parts[3]);

    const auto apos = itsAutoProjections.find(id);
    if (apos == itsAutoProjections.end())
      throw Fmi::Exception(BCP, "Unsupported AUTO2 projection").addParameter("CRS", theCRS);

    auto proj = apos->second;

    // Find the unit
    std::string unit_name;
    for (const auto& unit : itsLengthUnits)
    {
      if (unit.second == scale)
      {
        unit_name = unit.first;
        break;
      }
    }

    if (unit_name.empty())
      throw Fmi::Exception(BCP, "No known unit for the given projection scale")
          .addParameter("CRS", theCRS);

    // Make parameter substitutions

    const auto false_northing = (lat >= 0 ? 0 : 10000000.0);
    const auto central_meridian = -183 + 6 * std::min(std::floor((lon + 180) / 6) + 1.0, 60.0);
    boost::replace_all(proj, "${lon0}", Fmi::to_string(lon));
    boost::replace_all(proj, "${lat0}", Fmi::to_string(lat));
    boost::replace_all(proj, "${false_northing}", Fmi::to_string(false_northing));
    boost::replace_all(proj, "${central_meridian}", Fmi::to_string(central_meridian));
    boost::replace_all(proj, "${units}", unit_name);
    return proj;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GDAL definition for CRS not available!");
  }
}

bool WMSConfig::isValidCRS(const std::string& theLayer, const std::string& theCRS) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = itsLayers.load();
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidCRS(theCRS);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking CRS validity failed!");
  }
}

bool WMSConfig::isValidElevation(const std::string& theLayer, int theElevation) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = itsLayers.load();
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidElevation(theElevation);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking time validity failed!");
  }
}

bool WMSConfig::isValidReferenceTime(const std::string& theLayer,
                                     const Fmi::DateTime& theReferenceTime) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = itsLayers.load();
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidReferenceTime(theReferenceTime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking reference time validity failed!");
  }
}

bool WMSConfig::isValidTime(const std::string& theLayer,
                            const Fmi::DateTime& theTime,
                            const std::optional<Fmi::DateTime>& theReferenceTime) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = itsLayers.load();
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidTime(theTime, theReferenceTime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking time validity failed!");
  }
}

bool WMSConfig::isTemporal(const std::string& theLayer) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = itsLayers.load();
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isTemporal();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking whether layer is temporal failed!");
  }
}

bool WMSConfig::currentValue(const std::string& theLayer) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = itsLayers.load();
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->currentValue();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Getting current value for layer failed!");
  }
}

Fmi::DateTime WMSConfig::mostCurrentTime(const std::string& theLayer,
                                         const std::optional<Fmi::DateTime>& reference_time) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return Fmi::DateTime::NOT_A_DATE_TIME;

    auto my_layers = itsLayers.load();
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->mostCurrentTime(reference_time);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Getting most current time failed!");
  }
}

// Get the JSON for a single WMS layer

Json::Value WMSConfig::json(const std::string& theLayerName) const
{
  try
  {
    auto my_layers = itsLayers.load();
    if (my_layers->find(theLayerName) == my_layers->end())
      return "";

    return itsJsonCache.get(my_layers->at(theLayerName).getLayer()->getProductFile());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Getting layer JSON failed!");
  }
}

std::vector<Json::Value> WMSConfig::getLegendGraphic(const std::string& layerName,
                                                     const std::string& styleName,
                                                     std::size_t& width,
                                                     std::size_t& height,
                                                     const std::string& language) const
{
  std::vector<Json::Value> ret;

  auto my_layers = itsLayers.load();

  std::string customer = layerName.substr(0, layerName.find(':'));

  //  std::string legendDirectory = itsDaliConfig.rootDirectory(true) + "/customers/legends";

  std::string legendGraphicID = layerName + "::" + styleName;

  //  std::cout << "legendGraphicID: " << legendGraphicID << std::endl;

  LegendGraphicResult result =
      my_layers->at(layerName).getLayer()->getLegendGraphic(legendGraphicID, language);
  width = result.width;
  height = result.height;

  for (const auto& legendLayer : result.legendLayers)
  {
    Json::Value legend_j;

    std::unique_ptr<Json::CharReader> reader(charreaderbuilder.newCharReader());
    std::string errors;
    if (!reader->parse(
            legendLayer.c_str(), legendLayer.c_str() + legendLayer.size(), &legend_j, &errors))
      throw Fmi::Exception(BCP, "Legend template file parsing failed!")
          .addParameter("Message", errors);

    const bool wms_mode_on = true;
    Spine::JSON::preprocess(
        legend_j,
        itsDaliConfig.rootDirectory(wms_mode_on),
        itsDaliConfig.rootDirectory(wms_mode_on) + "/customers/" + customer + "/layers",
        itsJsonCache);

    Spine::JSON::dereference(legend_j);

    ret.push_back(legend_j);
  }

  return ret;
}

bool WMSConfig::isValidLayerImpl(const std::string& theLayer,
                                 bool theAcceptHiddenLayerFlag /*= false*/) const
{
  try
  {
    auto my_layers = itsLayers.load();
    if (my_layers->find(theLayer) == my_layers->end())
      return false;

    if (theAcceptHiddenLayerFlag)
      return true;

    WMSLayerProxy lp = my_layers->at(theLayer);
    return !(lp.getLayer()->isHidden());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking layer validity failed!");
  }
}

bool WMSConfig::inspireExtensionSupported() const
{
  return itsInspireExtensionSupported;
}

CTPP::CDT WMSConfig::getCapabilitiesResponseVariables() const
{
  return get_capabilities(itsDaliConfig.getConfig());
}

#ifndef WITHOUT_OBSERVATION
std::set<std::string> WMSConfig::getObservationProducers() const
{
  if (itsObsEngine != nullptr)
    return itsObsEngine->getValidStationTypes();

  return {};
}
#endif

void WMSConfig::getLegendGraphic(const std::string& theLayerName,
                                 const std::string& theStyleName,
                                 Product& theProduct,
                                 const State& theState,
                                 const std::string& theLanguage) const
{
  if (prepareLegendGraphic(theProduct))
    return;

  std::size_t width = 10;
  std::size_t height = 10;
  std::vector<Json::Value> legendLayers =
      getLegendGraphic(theLayerName, theStyleName, width, height, theLanguage);
  theProduct.width = width;
  theProduct.height = height;

  Json::Value nulljson;
  std::shared_ptr<View> view = *(theProduct.views.views.begin());

  for (auto& legendL : legendLayers)
  {
    std::shared_ptr<Layer> legendLayer(Dali::LayerFactory::create(legendL));
    legendLayer->init(legendL, theState, itsDaliConfig, theProduct);
    view->layers.layers.push_back(legendLayer);

    // add defs
    auto defs = legendL.get("defs", nulljson);
    if (!defs.isNull())
    {
      auto defLayers = defs.get("layers", nulljson);
      if (!defLayers.isNull())
      {
        for (auto& defLayerJson : defLayers)
        {
          if (!defLayerJson.isNull())
          {
            std::shared_ptr<Layer> defLayer(Dali::LayerFactory::create(defLayerJson));
            defLayer->init(defLayerJson, theState, itsDaliConfig, theProduct);
            theProduct.defs.layers.layers.push_back(defLayer);
          }
        }
      }
    }
  }
}

const WMSLegendGraphicSettings& WMSConfig::getLegendGraphicSettings() const
{
  return itsLegendGraphicSettings;
}

Fmi::DateTime WMSConfig::getCapabilitiesExpirationTime() const
{
  return (Fmi::SecondClock::universal_time() + Fmi::Seconds(itsCapabilityExpirationTime));
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
