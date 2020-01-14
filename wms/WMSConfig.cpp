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
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/FmiApiKey.h>
#include <spine/Json.h>
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
#include <gdal/ogr_spatialref.h>
#include <macgyver/StringConversion.h>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

using SmartMet::Plugin::Dali::Config;
using SmartMet::Plugin::Dali::Layer;
using SmartMet::Plugin::Dali::Product;
using SmartMet::Plugin::Dali::State;
using SmartMet::Plugin::Dali::View;

namespace
{
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
    return boost::algorithm::istarts_with(name, pattern + ":");

  // Strip surrounding slashes first
  const std::string re_str = pattern.substr(1, pattern.size() - 2);
  const boost::regex re(re_str, boost::regex::icase);
  return boost::regex_search(name, re);
}

std::string makeLayerNamespace(const std::string& customer,
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

    std::vector<std::string> tokens, namespace_tokens;
    boost::algorithm::split(tokens, fileDir, boost::is_any_of("/"), boost::token_compress_on);

    std::string currentPath;

    do
    {
      // Safety check needed for some incorrect specifications
      if (tokens.empty())
        throw SmartMet::Spine::Exception(BCP, "Failed to generate layer namespace")
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
    throw SmartMet::Spine::Exception::Trace(BCP, "Making layer namespace failed!");
  }
}

// if legend layer exixts use it directly
// otherwise generate legend from configuration
bool prepareLegendGraphic(Product& theProduct)
{
  std::list<boost::shared_ptr<View> >& views = theProduct.views.views;
  boost::shared_ptr<Layer> legendLayer;

  bool legendLayerFound = false;
  for (auto& view : views)
  {
    std::list<boost::shared_ptr<Layer> > layers = view->layers.layers;

    for (auto& layer : layers)
    {
      std::list<boost::shared_ptr<Layer> > sublayers = layer->layers.layers;

      if (layer->type)
      {
        if (*layer->type == "legend")
        {
          legendLayer = layer;
          legendLayerFound = true;
          break;
        }
      }

      for (auto& sublayer : sublayers)
      {
        std::list<boost::shared_ptr<Layer> > sublayers2 = sublayer->layers.layers;
      }
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

}  // anonymous namespace

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
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
    throw Spine::Exception::Trace(BCP, "Filename validation failed!");
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
      throw Spine::Exception(BCP, path + " value must be a scalar");
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

CTPP::CDT get_request(const libconfig::Config& config,
                      const std::string& prefix,
                      const std::string& variable)
{
  CTPP::CDT request(CTPP::CDT::HASH_VAL);

  const std::string path = prefix + "." + variable;

  const auto& formats = config.lookup(path + ".format");
  if (!formats.isArray())
    throw Spine::Exception(BCP, path + ".format  must be an array");

  CTPP::CDT format_list(CTPP::CDT::ARRAY_VAL);
  for (int i = 0; i < formats.getLength(); i++)
    format_list.PushBack(formats[i].c_str());
  request["format"] = format_list;

  const auto& dcptypes = config.lookup(path + ".dcptype");
  if (dcptypes.isArray())
    throw Spine::Exception(BCP, path + ".dcptype must be an array");
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

CTPP::CDT get_capabilities(const libconfig::Config& config)
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
      throw Spine::Exception(BCP, "wms.get_capabilities.service.keywords must be an array");

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
    throw Spine::Exception(BCP, "wms.get_capabilities.capability.exception must be an array");
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
 * \brief Extract supported EPSG and CRS references from the configuration
 */
// ----------------------------------------------------------------------

void WMSConfig::parse_references()
{
  // EPSG settings is an array of integers, whose name shall be EPSG:<int>

  const libconfig::Config& config = itsDaliConfig.getConfig();

  const auto& epsg_settings = config.lookup("wms.supported_references.epsg");

  if (!epsg_settings.isArray())
    throw Spine::Exception(BCP, "wms.supported_references.epsg must be an array of integers");

  for (int i = 0; i < epsg_settings.getLength(); i++)
  {
    int num = epsg_settings[i];
    std::string name = "EPSG:" + Fmi::to_string(num);
    itsSupportedWMSReferences[name] = name;

    Engine::Gis::BBox bbox = itsGisEngine->getBBox(num);
    itsWMSBBoxes.insert(std::make_pair(name, bbox));
  }

  // CRS settings is an array of id,proj,bbox tuples, whose name shall be CRS:<id>

  const auto& crs_settings = config.lookup("wms.supported_references.crs");

  if (!crs_settings.isList())
    throw Spine::Exception(BCP, "wms.supported_references.crs must be a list");

  for (int i = 0; i < crs_settings.getLength(); i++)
  {
    const auto& group = crs_settings[i];

    if (!group.isGroup())
      throw Spine::Exception(BCP, "wms.supported_references.crs must be a list of groups");

    std::string id = group["id"];
    std::string proj = group["proj"];
    std::string name = "CRS:" + id;

    itsSupportedWMSReferences[name] = proj;

    const auto& bbox = group["bbox"];
    if (!bbox.isArray())
      throw Spine::Exception(BCP, "wms.supported_references.crs bboxes must be arrays");

    if (bbox.getLength() != 4)
      throw Spine::Exception(BCP, "wms.supported_references.crs bboxes must have 4 elements");

    double west = bbox[0];
    double east = bbox[1];
    double south = bbox[2];
    double north = bbox[3];

    itsWMSBBoxes.insert(std::make_pair(name, Engine::Gis::BBox(west, east, south, north)));
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
                     Engine::Gis::Engine* gisEngine)
    : itsDaliConfig(daliConfig),
      itsJsonCache(theJsonCache),
      itsQEngine(qEngine),
      itsGisEngine(gisEngine),
#ifndef WITHOUT_AUTHENTICATION
      itsAuthEngine(authEngine),
#endif
#ifndef WITHOUT_OBSERVATION
      itsObsEngine(obsEngine),
#endif
      itsActiveThreadCount(0),
      itsShutdownRequested(false),
      itsLegendGraphicSettings(daliConfig.getConfig())
{
  try
  {
    const libconfig::Config& config = daliConfig.getConfig();

    std::string wmsMapFormats = config.lookup("wms.map_formats").c_str();

    config.lookupValue("wms.disable_updates", itsCapabilityUpdatesDisabled);
    config.lookupValue("wms.update_interval", itsCapabilityUpdateInterval);

    boost::algorithm::split(
        itsSupportedMapFormats, wmsMapFormats, boost::algorithm::is_any_of(","));

    const auto& exceptions = config.lookup("wms.get_capabilities.capability.exception");
    if (!exceptions.isArray())
      throw Spine::Exception(BCP, "wms.get_capabilities.capability.exception must be an array");
    for (int i = 0; i < exceptions.getLength(); i++)
      itsSupportedWMSExceptions.insert(exceptions[i].c_str());

    const auto& capability_formats =
        config.lookup("wms.get_capabilities.capability.request.getcapabilities.format");
    if (!capability_formats.isArray())
      throw Spine::Exception(
          BCP, "wms.get_capabilities.capability.request.getcapabilities.format must be an array");
    for (int i = 0; i < capability_formats.getLength(); i++)
      itsSupportedWMSGetCapabilityFormats.insert(capability_formats[i].c_str());

    std::string wmsVersions = config.lookup("wms.supported_versions").c_str();
    boost::algorithm::split(itsSupportedWMSVersions, wmsVersions, boost::algorithm::is_any_of(","));
    parse_references();

    config.lookupValue("wms.margin", itsMargin);

    // Parse GetCapability settings once to make sure the config file is valid
    get_capabilities(config);
  }
  catch (const libconfig::SettingNotFoundException& e)
  {
    throw Spine::Exception(BCP, "Setting not found").addParameter("Setting path", e.getPath());
  }
  catch (const libconfig::ParseException& e)
  {
    throw Spine::Exception::Trace(BCP, "WMS Configuration error!")
        .addParameter("Line", Fmi::to_string(e.getLine()));
  }
  catch (const libconfig::ConfigException&)
  {
    throw Spine::Exception::Trace(BCP, "WMS Configuration error!");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "WMS Configuration initialization failed!");
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
  if (itsShutdownRequested)
    return;

  // Do first layer scan
  updateLayerMetaData();

  if (itsShutdownRequested)
    return;

  // Begin the update loop

  if (!itsCapabilityUpdatesDisabled)
  {
    itsGetCapabilitiesThread = boost::movelib::make_unique<boost::thread>(
        boost::bind(&WMSConfig::capabilitiesUpdateLoop, this));
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
    if (itsShutdownRequested)
      return;

    itsShutdownRequested = true;
    itsShutdownCondition.notify_all();

    while (itsActiveThreadCount > 0)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Starting shutdown failed!");
  }
}

void WMSConfig::capabilitiesUpdateLoop()
{
  try
  {
    ++itsActiveThreadCount;
    while (!itsShutdownRequested)
    {
      try
      {
        // update capabilities every N seconds
        boost::system_time timeout =
            boost::get_system_time() + boost::posix_time::seconds(itsCapabilityUpdateInterval);

        boost::unique_lock<boost::mutex> lock(itsShutdownMutex);
        while (!itsShutdownRequested)
        {
          if (!itsShutdownCondition.timed_wait(lock, timeout))
            break;  // timeout
        }

        if (!itsShutdownRequested)
          updateLayerMetaData();
      }
      catch (...)
      {
        Spine::Exception exception(BCP, "Could not update capabilities!", nullptr);
        exception.printError();
      }
    }
    --itsActiveThreadCount;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Capabilities update failed!");
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
    throw Spine::Exception::Trace(BCP, "GetMap authorization failed!");
  }
}
#endif

void WMSConfig::updateLayerMetaData()
{
  try
  {
    // New shared pointer which will be atomically set into production
    boost::shared_ptr<LayerMap> newProxies(boost::make_shared<LayerMap>());

    const bool use_wms = true;
    std::string customerdir(itsDaliConfig.rootDirectory(use_wms) + "/customers");

    boost::filesystem::directory_iterator end_itr;
    for (boost::filesystem::directory_iterator itr(customerdir); itr != end_itr; ++itr)
    {
      if (itsShutdownRequested)
        return;

      try
      {
        if (is_directory(itr->status()))
        {
          std::string productdir(itr->path().string() + "/products");
          std::string customername = itr->path().filename().string();

          boost::filesystem::recursive_directory_iterator end_prod_itr;

          // Find product (layer) definitions
          for (boost::filesystem::recursive_directory_iterator itr2(productdir);
               itr2 != end_prod_itr;
               ++itr2)
          {
            if (itsShutdownRequested)
              return;

            try
            {
              if (is_regular_file(itr2->status()))
              {
                std::string layername = itr2->path().stem().string();
                std::string filename = itr2->path().filename().string();

                if (looks_valid_filename(filename))
                {
                  // Determine namespace from directory structure
                  std::string theNamespace = ::makeLayerNamespace(
                      customername, productdir, itr2->path().parent_path().string());
                  try
                  {
                    std::string pathName = itr2->path().string();
                    std::string fullLayername = theNamespace + ":" + layername;

                    // Check for modified product files here
                    std::time_t modtime = boost::filesystem::last_write_time(pathName);
                    if (itsProductFileModificationTime.find(pathName) ==
                        itsProductFileModificationTime.end())
                      itsProductFileModificationTime.insert(make_pair(pathName, modtime));
                    else
                    {
                      std::time_t previousModtime = itsProductFileModificationTime.at(pathName);
                      if (modtime != 0 && modtime != previousModtime)
                      {
                        // Product file is reloaded when the map is next time requested
                        itsProductFileModificationTime[pathName] = modtime;
                        std::string fileModifiedMsg =
                            Spine::log_time_str() + " File " + pathName + " modified, reloading it";
                        std::cout << fileModifiedMsg << std::endl;
                      }
                    }
                    bool mustUpdate = true;
                    if (itsLayers && itsLayers->find(fullLayername) != itsLayers->end())
                    {
                      WMSLayerProxy oldProxy = itsLayers->at(fullLayername);
                      SharedWMSLayer oldLayer = oldProxy.getLayer();
                      boost::posix_time::ptime timestamp = oldLayer->metaDataUpdateTime();

                      bool metadataUpdateIntervalExpired = false;
                      if (!timestamp.is_not_a_date_time())
                      {
                        boost::posix_time::time_duration diff =
                            (boost::posix_time::second_clock::universal_time() - timestamp);
                        unsigned int diff_seconds =
                            ((diff.hours() * 60 * 60) + (diff.minutes() * 60) + diff.seconds());
                        metadataUpdateIntervalExpired =
                            diff_seconds >= oldLayer->metaDataUpdateInterval();
                      }

                      // check if metadata need to be updated
                      // for example for icemaps it is not necessary so often
                      mustUpdate =
                          (metadataUpdateIntervalExpired || oldLayer->mustUpdateLayerMetaData());

                      if (!mustUpdate)
                        newProxies->insert(make_pair(fullLayername, oldProxy));
                    }

                    if (mustUpdate)
                    {
                      SharedWMSLayer wmsLayer(WMSLayerFactory::createWMSLayer(
                          pathName, theNamespace, customername, *this));
                      WMSLayerProxy newProxy(itsGisEngine, wmsLayer);
                      newProxies->insert(make_pair(fullLayername, newProxy));
                    }
                  }
                  catch (...)
                  {
                    // Ignore and report failed product definitions

                    auto badfile = itr2->path().c_str();
                    if (itsWarnedFiles.find(badfile) == itsWarnedFiles.end())
                    {
                      Spine::Exception exception(BCP, "Failed to parse configuration!", nullptr);
                      exception.addParameter("Path", itr2->path().c_str());
                      exception.printError();

                      // Don't warn again about the same file
                      itsWarnedFiles.insert(badfile);
                    }
                    continue;
                  }
                }
              }
            }
            catch (...)
            {
              Spine::Exception exception(
                  BCP,
                  "Lost " + std::string(itr2->path().c_str()) + " while scanning the filesystem!",
                  nullptr);
              exception.addParameter("Path", itr2->path().c_str());
              exception.printError();
            }
          }
        }
      }
      catch (...)
      {
        Spine::Exception exception(
            BCP,
            "Lost " + std::string(itr->path().c_str()) + " while scanning the filesystem!",
            nullptr);
        exception.addParameter("Path", itr->path().c_str());
        exception.printError();
      }
    }

    boost::atomic_store(&itsLayers, newProxies);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Layer metadata update failed!");
  }
}

#ifndef WITHOUT_AUTHENTICATION
CTPP::CDT WMSConfig::getCapabilities(const boost::optional<std::string>& apikey,
                                     const boost::optional<std::string>& starttime,
                                     const boost::optional<std::string>& endtime,
                                     const boost::optional<std::string>& wms_namespace,
                                     bool authenticate) const
#else
CTPP::CDT WMSConfig::getCapabilities(const boost::optional<std::string>& apikey,
                                     const boost::optional<std::string>& starttime,
                                     const boost::optional<std::string>& endtime,
                                     const boost::optional<std::string>& wms_namespace) const
#endif
{
  try
  {
    // Return array of individual layer capabilities
    CTPP::CDT layersCapabilities(CTPP::CDT::ARRAY_VAL);

    const std::string wmsService = "wms";

    // Atomic copy of layer data
    auto my_layers = boost::atomic_load(&itsLayers);

    for (const auto& iter_pair : *my_layers)
    {
#ifndef WITHOUT_AUTHENTICATION
      const auto& layer_name = iter_pair.first;
      // If authentication is requested, skip the layer if authentication fails
      if (apikey && authenticate)
        if (itsAuthEngine == nullptr || !itsAuthEngine->authorize(*apikey, layer_name, wmsService))
          continue;
#endif

      auto cdt = iter_pair.second.getCapabilities(starttime, endtime);

      // Note: The boost::optional is empty for hidden layers.
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
    throw Spine::Exception::Trace(BCP, "GetCapabilities failed!");
  }
}

std::string WMSConfig::layerCustomer(const std::string& theLayerName) const
{
  try
  {
    auto my_layers = boost::atomic_load(&itsLayers);

    auto it = my_layers->find(theLayerName);
    if (it == my_layers->end())
      return "";  // Should we throw an error!?

    return it->second.getLayer()->getCustomer();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Extracting customer for layer failed!");
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
const std::map<std::string, std::string>& WMSConfig::supportedWMSReferences() const
{
  return itsSupportedWMSReferences;
}
const std::set<std::string>& WMSConfig::supportedWMSExceptions() const
{
  return itsSupportedWMSExceptions;
}
const std::set<std::string>& WMSConfig::supportedWMSGetCapabilityFormats() const
{
  return itsSupportedWMSGetCapabilityFormats;
}

const std::map<std::string, Engine::Gis::BBox>& WMSConfig::WMSBBoxes() const
{
  return itsWMSBBoxes;
}

bool WMSConfig::isValidMapFormat(const std::string& theMapFormat) const
{
  try
  {
    return (itsSupportedMapFormats.find(theMapFormat) != itsSupportedMapFormats.end());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking valid MapFormat failed!");
  }
}

bool WMSConfig::isValidVersion(const std::string& theVersion) const
{
  try
  {
    return (itsSupportedWMSVersions.find(theVersion) != itsSupportedWMSVersions.end());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking WMS version failed!");
  }
}

bool WMSConfig::isValidLayer(const std::string& theLayer,
                             bool theAcceptHiddenLayerFlag /*= false*/) const
{
  try
  {
    return isValidLayerImpl(theLayer, theAcceptHiddenLayerFlag);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking layer validity failed!");
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

    auto my_layers = boost::atomic_load(&itsLayers);

    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidStyle(theStyle);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking style validity failed!");
  }
}

const std::string& WMSConfig::getCRSDefinition(const std::string& theCRS) const
{
  try
  {
    return itsSupportedWMSReferences.at(theCRS);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "GDAL defintion for CRS not available!");
  }
}

bool WMSConfig::isValidCRS(const std::string& theLayer, const std::string& theCRS) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = boost::atomic_load(&itsLayers);
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidCRS(theCRS);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking CRS validity failed!");
  }
}

bool WMSConfig::isValidTime(const std::string& theLayer,
                            const boost::posix_time::ptime& theTime,
                            const Engine::Querydata::Engine& /* theQEngine */) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = boost::atomic_load(&itsLayers);
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isValidTime(theTime);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking time validity failed!");
  }
}

bool WMSConfig::isTemporal(const std::string& theLayer) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = boost::atomic_load(&itsLayers);
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->isTemporal();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking whether layer is temporal failed!");
  }
}

bool WMSConfig::currentValue(const std::string& theLayer) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return false;

    auto my_layers = boost::atomic_load(&itsLayers);
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->currentValue();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Getting current value for layer failed!");
  }
}

boost::posix_time::ptime WMSConfig::mostCurrentTime(const std::string& theLayer) const
{
  try
  {
    if (!isValidLayerImpl(theLayer))
      return boost::posix_time::not_a_date_time;

    auto my_layers = boost::atomic_load(&itsLayers);
    SharedWMSLayer layer = my_layers->at(theLayer).getLayer();

    return layer->mostCurrentTime();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Getting most current time failed!");
  }
}

Json::Value WMSConfig::json(const std::string& theLayerName) const
{
  try
  {
    auto my_layers = boost::atomic_load(&itsLayers);
    if (my_layers->find(theLayerName) == my_layers->end())
      return "";

    return itsJsonCache.get(my_layers->at(theLayerName).getLayer()->getDaliProductFile());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Getting layer JSON failed!");
  }
}

std::vector<Json::Value> WMSConfig::getLegendGraphic(const std::string& layerName,
                                                     const std::string& styleName,
                                                     std::size_t& width,
                                                     std::size_t& height,
                                                     const std::string& language) const
{
  std::vector<Json::Value> ret;

  auto my_layers = boost::atomic_load(&itsLayers);

  std::string customer = layerName.substr(0, layerName.find(':'));

  std::string legendDirectory = itsDaliConfig.rootDirectory(true) + "/customers/legends";

  std::string legendGraphicID = layerName + "::" + styleName;

  LegendGraphicResult result = my_layers->at(layerName).getLayer()->getLegendGraphic(
      itsLegendGraphicSettings, legendGraphicID, language);
  width = result.width;
  height = result.height;

  for (const auto& legendLayer : result.legendLayers)
  {
    Json::Value json;
    Json::Reader reader;
    bool json_ok = reader.parse(legendLayer, json);
    if (!json_ok)
    {
      std::string msg = reader.getFormattedErrorMessages();
      std::replace(msg.begin(), msg.end(), '\n', ' ');
      throw Spine::Exception(BCP, "Legend template file parsing failed!").addDetail(msg);
    }

    const bool use_wms = true;
    Spine::JSON::preprocess(
        json,
        itsDaliConfig.rootDirectory(use_wms),
        itsDaliConfig.rootDirectory(use_wms) + "/customers/" + customer + "/layers",
        itsJsonCache);

    Spine::JSON::dereference(json);

    ret.push_back(json);
  }

  return ret;
}

bool WMSConfig::isValidLayerImpl(const std::string& theLayer,
                                 bool theAcceptHiddenLayerFlag /*= false*/) const
{
  try
  {
    auto my_layers = boost::atomic_load(&itsLayers);
    if (my_layers->find(theLayer) != my_layers->end())
    {
      if (theAcceptHiddenLayerFlag)
        return true;

      WMSLayerProxy lp = my_layers->at(theLayer);
      return !(lp.getLayer()->isHidden());
    }

    return (my_layers->find(theLayer) != my_layers->end());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Checking layer validity failed!");
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

  return std::set<std::string>();
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

  std::size_t width = 100;
  std::size_t height = 100;
  std::vector<Json::Value> legendLayers =
      getLegendGraphic(theLayerName, theStyleName, width, height, theLanguage);
  theProduct.width = width;
  theProduct.height = height;

  Json::Value nulljson;
  boost::shared_ptr<View> view = *(theProduct.views.views.begin());

  for (const auto& legendL : legendLayers)
  {
    boost::shared_ptr<Layer> legendLayer(Dali::LayerFactory::create(legendL));
    legendLayer->init(legendL, theState, itsDaliConfig, theProduct);
    view->layers.layers.push_back(legendLayer);

    // add defs
    auto defs = legendL.get("defs", nulljson);
    if (!defs.isNull())
    {
      auto defLayers = defs.get("layers", nulljson);
      if (!defLayers.isNull())
      {
        for (const auto& defLayerJson : defLayers)
        {
          if (!defLayerJson.isNull())
          {
            boost::shared_ptr<Layer> defLayer(Dali::LayerFactory::create(defLayerJson));
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

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
