#include "WMSConfig.h"
#include "WMSException.h"
#include "WMSLayerFactory.h"

#include <spine/Exception.h>
#include <spine/Json.h>
#include <engines/authentication/Engine.h>

#include <macgyver/String.h>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#define BOOST_FILESYSTEM_NO_DEPRECATED

#include <stdexcept>
#include <algorithm>

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

WMSConfig::WMSConfig(const SmartMet::Plugin::Dali::Config& daliConfig,
                     const SmartMet::Spine::FileCache& theFileCache,
                     SmartMet::Engine::Querydata::Engine* qEngine,
                     SmartMet::Engine::Authentication::Engine* authEngine,
                     SmartMet::Engine::Gis::Engine* gisEngine)
    : itsDaliConfig(daliConfig),
      itsFileCache(theFileCache),
      itsQEngine(qEngine),
      itsGisEngine(gisEngine),
      itsAuthEngine(authEngine),
      itsShutdownRequested(false),
      itsActiveThreadCount(0)

{
  try
  {
    itsSupportedWMSVersions.insert("1.3.0");
    itsSupportedMapFormats.insert("image/svg+xml");
    itsSupportedMapFormats.insert("image/png");

    std::string wmsVersions;
    std::string wmsMapFormats;
    daliConfig.getConfig().lookupValue("wms.versions", wmsVersions);
    daliConfig.getConfig().lookupValue("wms.mapformats", wmsMapFormats);

    boost::algorithm::split(
        itsSupportedMapFormats, wmsMapFormats, boost::algorithm::is_any_of(","));
    boost::algorithm::split(itsSupportedWMSVersions, wmsVersions, boost::algorithm::is_any_of(","));

    if (daliConfig.getConfig().exists("wms.inspire_extension"))
    {
      // inspire extension is supported if wms.inspire_extension configuration exists
      itsInspireExtensionSupported = true;
      daliConfig.getConfig().lookupValue("wms.inspire_extension.default_language",
                                         itsInspireExtensionDefaultLanguage);
      daliConfig.getConfig().lookupValue("wms.inspire_extension.supported_language",
                                         itsInspireExtensionSupportedLanguage);
      daliConfig.getConfig().lookupValue("wms.inspire_extension.response_language",
                                         itsInspireExtensionResponseLanguage);
    }

    // Do first layer scan
    updateLayerMetaData();

    // Begin the update loop
    itsGetCapabilitiesThread.reset(
        new boost::thread(boost::bind(&WMSConfig::capabilitiesUpdateLoop, this)));
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
        SmartMet::Spine::Exception exception(BCP, "Could not update capabilities!", NULL);
        std::cout << exception.getStackTrace();
      }
    }
    itsActiveThreadCount--;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::validateGetMapAuthorization(const SmartMet::Spine::HTTP::Request& theRequest) const
{
  try
  {
    // Existence of layers must be guaranteed before calling this function
    std::string layerstring = *(theRequest.getParameter("LAYERS"));
    std::string apikey;

    // Try Get-parameter first
    auto apikey_param = theRequest.getParameter("fmi-apikey");
    if (!apikey_param)
    {
      auto apikey_header = theRequest.getHeader("fmi-apikey");
      if (!apikey_header)
        return true;
      else
        apikey = *apikey_header;
    }
    else
      apikey = *apikey_param;

    std::vector<std::string> layers;
    boost::algorithm::split(layers, layerstring, boost::algorithm::is_any_of(","));

    if (layers.empty())
      return true;

    // All layers must match in order to grant access
    return itsAuthEngine->authorize(apikey, layers, "wms");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

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
                    SmartMet::Spine::Exception exception(
                        BCP, "Failed to parse configuration!", NULL);
                    exception.addParameter("Path", itr2->path().c_str());
                    std::cout << exception.getStackTrace();
                    continue;
                  }
                }
              }
            }
            catch (...)
            {
              SmartMet::Spine::Exception exception(
                  BCP,
                  "Lost " + std::string(itr2->path().c_str()) + " while scanning the filesystem!",
                  NULL);
              exception.addParameter("Path", itr2->path().c_str());
              std::cout << exception.getStackTrace();
              ;
            }
          }
        }
      }
      catch (...)
      {
        SmartMet::Spine::Exception exception(
            BCP,
            "Lost " + std::string(itr->path().c_str()) + " while scanning the filesystem!",
            NULL);
        exception.addParameter("Path", itr->path().c_str());
        std::cout << exception.getStackTrace();
      }
    }

    SmartMet::Spine::WriteLock theLock(itsGetCapabilitiesMutex);

    std::swap(itsLayers, newProxies);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSConfig::getCapabilities(const boost::optional<std::string>& apikey,
                                       bool authenticate) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

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
        if (authenticate == true)
        {
          if (itsAuthEngine->authorize(theKey, layer_name, wmsService))
          {
            resultCapabilities += iter_pair.second.getCapabilities();
          }
        }
      }
    }

    return resultCapabilities;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSConfig::layerCustomer(const std::string& theLayerName) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    auto it = itsLayers.find(theLayerName);
    if (it == itsLayers.end())
      return "";  // Should we throw an error!?

    return it->second.getLayer()->getCustomer();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidLayer(const std::string& theLayer) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    return isValidLayerImpl(theLayer);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidStyle(const std::string& theLayer, const std::string& theStyle) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidCRS(const std::string& theLayer, const std::string& theCRS) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->isValidCRS(theCRS);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isValidTime(const std::string& theLayer,
                            const boost::posix_time::ptime& theTime,
                            const SmartMet::Engine::Querydata::Engine& theQEngine) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->isValidTime(theTime);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::isTemporal(const std::string& theLayer) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->isTemporal();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::currentValue(const std::string& theLayer) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return false;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->currentValue();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

boost::posix_time::ptime WMSConfig::mostCurrentTime(const std::string& theLayer) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (isValidLayerImpl(theLayer) == false)
      return boost::posix_time::not_a_date_time;

    SharedWMSLayer layer = itsLayers.at(theLayer).getLayer();

    return layer->mostCurrentTime();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSConfig::jsonText(const std::string& theLayerName) const
{
  try
  {
    SmartMet::Spine::ReadLock theLock(itsGetCapabilitiesMutex);

    if (itsLayers.find(theLayerName) == itsLayers.end())
      return "";

    return itsFileCache.get(itsLayers.at(theLayerName).getLayer()->getDaliProductFile());
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSConfig::inspireExtensionSupported() const
{
  return itsInspireExtensionSupported;
}
const std::string& WMSConfig::inspireExtensionDefaultLanguage() const
{
  return itsInspireExtensionDefaultLanguage;
}
const std::string& WMSConfig::inspireExtensionSupportedLanguage() const
{
  return itsInspireExtensionSupportedLanguage;
}
const std::string& WMSConfig::inspireExtensionResponseLanguage() const
{
  return itsInspireExtensionResponseLanguage;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
