#include "WMSConfig.h"
#include "WMSException.h"
#include "WMSLayerFactory.h"

#include <spine/Exception.h>
#include <spine/FmiApiKey.h>
#include <spine/Json.h>

#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif

#include <macgyver/StringConversion.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/foreach.hpp>
#define BOOST_FILESYSTEM_NO_DEPRECATED

#include <algorithm>
#include <stdexcept>

#include <gdal/ogr_spatialref.h>

using namespace std;

namespace
{
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}

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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

WMSConfig::WMSConfig(const Plugin::Dali::Config& daliConfig,
                     const Spine::FileCache& theFileCache,
                     Engine::Querydata::Engine* qEngine,
#ifndef WITHOUT_AUTHENTICATION
                     Engine::Authentication::Engine* authEngine,
#endif
#ifndef WITHOUT_OBSERVATION
                     Engine::Observation::Engine* obsEngine,
#endif
                     Engine::Gis::Engine* gisEngine)
    : itsDaliConfig(daliConfig),
      itsFileCache(theFileCache),
      itsQEngine(qEngine),
      itsGisEngine(gisEngine),
#ifndef WITHOUT_AUTHENTICATION
      itsAuthEngine(authEngine),
#endif
#ifndef WITHOUT_OBSERVATION
      itsObsEngine(obsEngine),
#endif
      itsShutdownRequested(false),
      itsActiveThreadCount(0)

{
  try
  {
    const libconfig::Config& config = daliConfig.getConfig();

    std::string wmsVersions;
    std::string wmsMapFormats;
    config.lookupValue("wms.versions", wmsVersions);
    config.lookupValue("wms.mapformats", wmsMapFormats);

    boost::algorithm::split(
        itsSupportedMapFormats, wmsMapFormats, boost::algorithm::is_any_of(","));
    boost::algorithm::split(itsSupportedWMSVersions, wmsVersions, boost::algorithm::is_any_of(","));

    if (config.exists("wms.get_capabilities_response.inspire_extension"))
    {
      // inspire extension is supported if wms.inspire_extension configuration exists
      itsInspireExtensionSupported = true;
      itsGetCapabilitiesResponseVariables.insert(make_pair("default_language", ""));
      itsGetCapabilitiesResponseVariables.insert(make_pair("supported_language", ""));
      itsGetCapabilitiesResponseVariables.insert(make_pair("response_language", ""));
      itsGetCapabilitiesResponseVariables.insert(
          make_pair("metadata_url",
                    "http://catalog.fmi.fi/geonetwork/srv/en/"
                    "csw?SERVICE=CSW&amp;VERSION=2.0.2&amp;REQUEST=GetRecordById&amp;ID=1234dfc1-"
                    "4c08-4491-8ca0-b8ea2941c24a&amp;outputSchema=http://www.isotc211.org/2005/"
                    "gmd&amp;elementSetName=full"));
      config.lookupValue("wms.get_capabilities_response.inspire_extension.default_language",
                         itsGetCapabilitiesResponseVariables["default_language"]);
      config.lookupValue("wms.get_capabilities_response.inspire_extension.supported_language",
                         itsGetCapabilitiesResponseVariables["supported_language"]);
      config.lookupValue("wms.get_capabilities_response.inspire_extension.response_language",
                         itsGetCapabilitiesResponseVariables["response_language"]);
      config.lookupValue("wms.get_capabilities_response.inspire_extension.metadata_url",
                         itsGetCapabilitiesResponseVariables["metadata_url"]);
    }

    itsGetCapabilitiesResponseVariables.insert(make_pair("title", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("abstract", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("keywords", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("online_resource", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_person", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("organization", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_position", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_address_type", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_address", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_address_city", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_address_province", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_address_post_code", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("contact_address_country", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("telephone_number", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("facsimile_number", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("fees", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("access_constraints", ""));
    itsGetCapabilitiesResponseVariables.insert(make_pair("layers_title", ""));

    config.lookupValue("wms.get_capabilities_response.keywords",
                       itsGetCapabilitiesResponseVariables["keywords"]);
    config.lookupValue("wms.get_capabilities_response.title",
                       itsGetCapabilitiesResponseVariables["title"]);
    config.lookupValue("wms.get_capabilities_response.abstract",
                       itsGetCapabilitiesResponseVariables["abstract"]);
    config.lookupValue("wms.get_capabilities_response.online_resource",
                       itsGetCapabilitiesResponseVariables["online_resource"]);
    config.lookupValue("wms.get_capabilities_response.fees",
                       itsGetCapabilitiesResponseVariables["fees"]);
    config.lookupValue("wms.get_capabilities_response.access_constraints",
                       itsGetCapabilitiesResponseVariables["access_constraints"]);
    config.lookupValue("wms.get_capabilities_response.layers_title",
                       itsGetCapabilitiesResponseVariables["layers_title"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.contact_person",
                       itsGetCapabilitiesResponseVariables["contact_person"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.organization",
                       itsGetCapabilitiesResponseVariables["organization"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.contact_position",
                       itsGetCapabilitiesResponseVariables["contact_position"]);

    config.lookupValue("wms.get_capabilities_response.contact_info.address_type",
                       itsGetCapabilitiesResponseVariables["contact_address_type"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.address",
                       itsGetCapabilitiesResponseVariables["contact_address"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.city",
                       itsGetCapabilitiesResponseVariables["contact_address_city"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.state_or_province",
                       itsGetCapabilitiesResponseVariables["contact_address_province"]);

    config.lookupValue("wms.get_capabilities_response.contact_info.post_code",
                       itsGetCapabilitiesResponseVariables["contact_address_post_code"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.country",
                       itsGetCapabilitiesResponseVariables["contact_address_country"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.telephone_number",
                       itsGetCapabilitiesResponseVariables["telephone_number"]);
    config.lookupValue("wms.get_capabilities_response.contact_info.facsimile_number",
                       itsGetCapabilitiesResponseVariables["facsimile_number"]);

    // read headers
    itsGetCapabilitiesResponseVariables.insert(make_pair("header", ""));
    if (config.exists("wms.get_capabilities_response.headers"))
    {
      libconfig::Setting& groups = config.lookup("wms.get_capabilities_response.headers");
      for (int i = 0; i < groups.getLength(); i++)
      {
        std::string name;
        std::string value;
        config.lookupValue(Fmi::to_string("wms.get_capabilities_response.headers.[%d].name", i),
                           name);
        config.lookupValue(Fmi::to_string("wms.get_capabilities_response.headers.[%d].value", i),
                           value);

        itsGetCapabilitiesResponseVariables["headers"] += name;
        itsGetCapabilitiesResponseVariables["headers"] += "=\"";
        itsGetCapabilitiesResponseVariables["headers"] += value;
        itsGetCapabilitiesResponseVariables["headers"] += "\"";
        if (i < groups.getLength() - 1)
          itsGetCapabilitiesResponseVariables["headers"] += " \n";
      }
    }
    else
    {
      // default values
      itsGetCapabilitiesResponseVariables["headers"] += "xmlns=\"http://www.opengis.net/wms\" \n";
      itsGetCapabilitiesResponseVariables["headers"] +=
          "xmlns:sld=\"http://www.opengis.net/sld\" \n";
      itsGetCapabilitiesResponseVariables["headers"] +=
          "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" \n";
      itsGetCapabilitiesResponseVariables["headers"] +=
          "xmlns:ms=\"http://mapserver.gis.umn.edu/mapserver\" \n";
      itsGetCapabilitiesResponseVariables["headers"] +=
          "xmlns:inspire_common=\"http://inspire.ec.europa.eu/schemas/common/1.0\" \n";
      itsGetCapabilitiesResponseVariables["headers"] +=
          "xmlns:inspire_vs=\"http://inspire.ec.europa.eu/schemas/inspire_vs/1.0\" \n";
      itsGetCapabilitiesResponseVariables["headers"] +=
          "xsi:schemaLocation=\"http://www.opengis.net/wms "
          "http://schemas.opengis.net/wms/1.3.0/capabilities_1_3_0.xsd  http://www.opengis.net/sld "
          "http://schemas.opengis.net/sld/1.1.0/sld_capabilities.xsd  "
          "http://inspire.ec.europa.eu/schemas/inspire_vs/1.0 "
          "http://inspire.ec.europa.eu/schemas/inspire_vs/1.0/inspire_vs.xsd\"";
    }

    // Do first layer scan
    updateLayerMetaData();

    // Begin the update loop
    itsGetCapabilitiesThread.reset(
        new boost::thread(boost::bind(&WMSConfig::capabilitiesUpdateLoop, this)));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    std::cout << "  -- Shutdown requested (WMSConfig)\n";
    itsShutdownRequested = true;

    while (itsActiveThreadCount > 0)
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void WMSConfig::capabilitiesUpdateLoop()
{
  try
  {
    itsActiveThreadCount++;
    while (!itsShutdownRequested)
    {
      try
      {
        for (int i = 0; (!itsShutdownRequested && i < 5); i++)
          boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

        if (!itsShutdownRequested)
          updateLayerMetaData();
      }
      catch (...)
      {
        Spine::Exception exception(BCP, "Could not update capabilities!", NULL);
        exception.printError();
      }
    }
    itsActiveThreadCount--;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

#ifndef WITHOUT_AUTHENTICATION
bool WMSConfig::validateGetMapAuthorization(const Spine::HTTP::Request& theRequest) const
{
  // Everything ok if authentication is disabled
  if (!itsAuthEngine)
    return true;

  try
  {
    // Existence of layers must be guaranteed before calling this function
    std::string layerstring = *(theRequest.getParameter("LAYERS"));

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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
#endif

void WMSConfig::updateLayerMetaData()
{
  try
  {
    // These should probably be in constructor
    boost::algorithm::split(
        itsSupportedMapFormats, itsDaliConfig.wmsMapFormats(), boost::algorithm::is_any_of(","));
    boost::algorithm::split(
        itsSupportedWMSVersions, itsDaliConfig.wmsVersions(), boost::algorithm::is_any_of(","));

    std::map<std::string, WMSLayerProxy> newProxies;

    const bool use_wms = true;
    std::string customerdir(itsDaliConfig.rootDirectory(use_wms) + "/customers");

    boost::filesystem::directory_iterator end_itr;
    for (boost::filesystem::directory_iterator itr(customerdir); itr != end_itr; ++itr)
    {
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
                    std::string fullLayername(theNamespace + ":" + layername);
                    SharedWMSLayer wmsLayer(WMSLayerFactory::createWMSLayer(
                        itr2->path().string(), theNamespace, customername, *this));

                    WMSLayerProxy newProxy(itsGisEngine, wmsLayer);
                    newProxies.insert(make_pair(fullLayername, newProxy));
                  }
                  catch (...)
                  {
                    // Ignore and report failed product definitions
                    Spine::Exception exception(BCP, "Failed to parse configuration!", NULL);
                    exception.addParameter("Path", itr2->path().c_str());
                    exception.printError();
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
                  NULL);
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
            NULL);
        exception.addParameter("Path", itr->path().c_str());
        exception.printError();
      }
    }

    Spine::WriteLock theLock(itsGetCapabilitiesMutex);

    std::swap(itsLayers, newProxies);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

#ifndef WITHOUT_AUTHENTICATION
std::string WMSConfig::getCapabilities(const boost::optional<std::string>& apikey,
                                       bool authenticate) const
#else
std::string WMSConfig::getCapabilities(const boost::optional<std::string>& apikey) const
#endif
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    std::string wmsService = "wms";  // Make this an include or something

    std::string resultCapabilities;

    if (!apikey)
    {
      // No apikey given, access is granted to everything
      for (const auto& iter_pair : itsLayers)
      {
        resultCapabilities += iter_pair.second.getCapabilities();
      }
    }
    else
    {
      std::string theKey = *apikey;

      for (const auto& iter_pair : itsLayers)
      {
        const std::string layer_name = iter_pair.first;
#ifndef WITHOUT_AUTHENTICATION
        if (authenticate == true)
        {
          if (itsAuthEngine && itsAuthEngine->authorize(theKey, layer_name, wmsService))
          {
            resultCapabilities += iter_pair.second.getCapabilities();
          }
        }
#else
        resultCapabilities += iter_pair.second.getCapabilities();
#endif
      }
    }

    return resultCapabilities;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSConfig::layerCustomer(const std::string& theLayerName) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    auto it = itsLayers.find(theLayerName);
    if (it == itsLayers.end())
      return "";  // Should we throw an error!?

    return it->second.getLayer()->getCustomer();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
bool WMSConfig::isValidMapFormat(const std::string& theMapFormat) const
{
  try
  {
    return (itsSupportedMapFormats.find(theMapFormat) != itsSupportedMapFormats.end());
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidLayer(const std::string& theLayer) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    return isValidLayerImpl(theLayer);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidStyle(const std::string& theLayer, const std::string& theStyle) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    // empty means default style
    if (theStyle.empty())
      return true;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->isValidStyle(theStyle);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidCRS(const std::string& theLayer, const std::string& theCRS) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->isValidCRS(theCRS);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidTime(const std::string& theLayer,
                            const boost::posix_time::ptime& theTime,
                            const Engine::Querydata::Engine& theQEngine) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->isValidTime(theTime);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isTemporal(const std::string& theLayer) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->isTemporal();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::currentValue(const std::string& theLayer) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->currentValue();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

boost::posix_time::ptime WMSConfig::mostCurrentTime(const std::string& theLayer) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return boost::posix_time::not_a_date_time;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->mostCurrentTime();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSConfig::jsonText(const std::string& theLayerName) const
{
  try
  {
    Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (itsLayers.find(theLayerName) == itsLayers.end())
      return "";

    return itsFileCache.get(itsLayers.at(theLayerName).getLayer()->getDaliProductFile());
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidLayerImpl(const std::string& theLayer) const
{
  try
  {
    return (itsLayers.find(theLayer) != itsLayers.end());
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::inspireExtensionSupported() const
{
  return itsInspireExtensionSupported;
}

const std::map<std::string, std::string>& WMSConfig::getCapabilitiesResponseVariables() const
{
  return itsGetCapabilitiesResponseVariables;
}

#ifndef WITHOUT_OBSERVATION
std::set<std::string> WMSConfig::getObservationProducers() const
{
  if (itsObsEngine)
    return itsObsEngine->getValidStationTypes();

  return std::set<std::string>();
}
#endif

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
