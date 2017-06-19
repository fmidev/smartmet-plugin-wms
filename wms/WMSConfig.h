// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "WMS.h"
#include "WMSLayer.h"

#include <engines/gis/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/querydata/Engine.h>
#include <spine/FileCache.h>
#include <spine/Thread.h>

#include <boost/optional.hpp>
#include <boost/utility.hpp>
#include <libconfig.h++>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace SmartMet
{
#ifndef WITHOUT_AUTHENTICATION
namespace Engine
{
namespace Authentication
{
class Engine;
}
}
#endif

namespace Plugin
{
namespace Dali
{
class Product;
class State;
}
namespace WMS
{
// For getCapabilities caching
class WMSLayerProxy
{
 public:
  WMSLayerProxy(Engine::Gis::Engine* gisEngine, SharedWMSLayer theLayer)
      : itsGisEngine(gisEngine), itsLayer(theLayer)
  {
    itsCapabilitiesXML = itsLayer->generateGetCapabilities(*itsGisEngine);
  }

  SharedWMSLayer getLayer() const { return itsLayer; }
  std::string getCapabilities() const { return itsCapabilitiesXML; }
  void update()
  {
    itsLayer->updateLayerMetaData();
    itsCapabilitiesXML = itsLayer->generateGetCapabilities(*itsGisEngine);
  }

 private:
  Engine::Gis::Engine* itsGisEngine;
  SharedWMSLayer itsLayer;
  std::string layerName;
  std::string itsCapabilitiesXML;
};

class WMSConfig
{
 public:
  WMSConfig() = delete;
  WMSConfig(const Plugin::Dali::Config& daliConfig,
            const Spine::FileCache& theFileCache,
            Engine::Querydata::Engine* qEngine,
#ifndef WITHOUT_AUTHENTICATION
            Engine::Authentication::Engine* authEngine,
#endif
#ifndef WITHOUT_OBSERVATION
            Engine::Observation::Engine* obsEngine,
#endif
            Engine::Gis::Engine* gisEngine);

#ifndef WITHOUT_AUTHENTICATION
  std::string getCapabilities(const boost::optional<std::string>& apikey,
                              const std::string& host,
                              bool authenticate = true) const;
#else
  std::string getCapabilities(const boost::optional<std::string>& apikey,
                              const std::string& host) const;
#endif

  std::string layerCustomer(const std::string& theLayerName) const;
  const std::set<std::string>& supportedMapFormats() const;
  const std::set<std::string>& supportedWMSVersions() const;
  bool isValidMapFormat(const std::string& theMapFormat) const;
  bool isValidVersion(const std::string& theVersion) const;
  bool isValidLayer(const std::string& theLayer, bool theAcceptHiddenLayerFlag = false) const;
  bool isValidStyle(const std::string& theLayer, const std::string& theStyle) const;
  bool isValidCRS(const std::string& theLayer, const std::string& theCRS) const;

#ifndef WITHOUT_AUTHENTICATION
  bool validateGetMapAuthorization(const Spine::HTTP::Request& theRequest) const;
#endif

  bool isValidTime(const std::string& theLayer,
                   const boost::posix_time::ptime& theTime,
                   const Engine::Querydata::Engine& theQEngine) const;
  bool isTemporal(const std::string& theLayer) const;
  bool currentValue(const std::string& theLayer) const;
  boost::posix_time::ptime mostCurrentTime(const std::string& theLayer) const;
  std::string jsonText(const std::string& theLayerName) const;
  std::vector<Json::Value> getLegendGraphic(const std::string& theLayerName) const;

  bool inspireExtensionSupported() const;
  const std::map<std::string, std::string>& getCapabilitiesResponseVariables() const;

  void shutdown();

  void getLegendGraphic(const std::string& theLayerName,
                        Dali::Product& theProduct,
                        const Dali::State& theState) const;

 private:
  const Plugin::Dali::Config& itsDaliConfig;
  const Spine::FileCache& itsFileCache;

  // Engines for GetCapabilities
  Engine::Querydata::Engine* itsQEngine = nullptr;
  Engine::Gis::Engine* itsGisEngine = nullptr;

#ifndef WITHOUT_AUTHENTICATION
  // For GetCapabilities and GetMap Authentication
  Engine::Authentication::Engine* itsAuthEngine = nullptr;
#endif
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* itsObsEngine = nullptr;
#endif

  // supported map formats
  std::set<std::string> itsSupportedMapFormats;
  // supported wms versions
  std::set<std::string> itsSupportedWMSVersions;

  bool itsInspireExtensionSupported = false;

  std::map<std::string, std::string> itsGetCapabilitiesResponseVariables;

  // Valid WMS layers (name -> layer proxy)
  std::map<std::string, WMSLayerProxy> itsLayers;

  std::unique_ptr<boost::thread> itsGetCapabilitiesThread;

  void capabilitiesUpdateLoop();

  void updateLayerMetaData();

#ifndef WITHOUT_OBSERVATION
  std::set<std::string> getObservationProducers() const;
#endif
  // For locking purposes
  bool isValidLayerImpl(const std::string& theLayer, bool theAcceptHiddenLayerFlag = false) const;

  friend class WMSLayerFactory;

  mutable Spine::MutexType itsGetCapabilitiesMutex;

  bool itsShutdownRequested;
  int itsActiveThreadCount;

  std::map<std::string, Json::Value> itsLegendGraphicLayers;
};  // class WMSConfig

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
