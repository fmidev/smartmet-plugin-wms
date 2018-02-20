// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "WMS.h"
#include "WMSLayer.h"
#include "WMSLayerProxy.h"
#include "WMSLegendGraphicSettings.h"

#include <engines/gis/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/querydata/Engine.h>
#include <spine/FileCache.h>
#include <spine/Thread.h>

#include <boost/optional.hpp>
#include <boost/utility.hpp>
#include <ctpp2/CDT.hpp>
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
}  // namespace Engine
#endif

namespace Plugin
{
namespace Dali
{
class Product;
class State;
}  // namespace Dali
namespace WMS
{
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
  CTPP::CDT getCapabilities(const boost::optional<std::string>& apikey,
                            const boost::optional<std::string>& wms_namespace,
                            bool authenticate = true) const;
#else
  CTPP::CDT getCapabilities(const boost::optional<std::string>& apikey,
                            const boost::optional<std::string>& wms_namespace) const;
#endif

  void init();

  std::string layerCustomer(const std::string& theLayerName) const;
  const std::set<std::string>& supportedMapFormats() const;
  const std::set<std::string>& supportedWMSVersions() const;
  const std::map<std::string, std::string>& supportedWMSReferences() const;
  const std::map<std::string, Engine::Gis::BBox>& WMSBBoxes() const;
  const std::string& getCRSDefinition(const std::string& theCRS) const;
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
  std::vector<Json::Value> getLegendGraphic(const std::string& theLayerName,
                                            std::size_t& width,
                                            std::size_t& height) const;

  bool inspireExtensionSupported() const;
  CTPP::CDT getCapabilitiesResponseVariables() const;

  void shutdown();

  void getLegendGraphic(const std::string& theLayerName,
                        Dali::Product& theProduct,
                        const Dali::State& theState) const;

  const Spine::FileCache& getFileCache() const { return itsFileCache; }
  const Plugin::Dali::Config& getDaliConfig() const { return itsDaliConfig; }
  const Engine::Querydata::Engine* qEngine() const { return itsQEngine; }
  const Engine::Gis::Engine* gisEngine() const { return itsGisEngine; }
#ifndef WITHOUT_OBSERVATION
  const Engine::Observation::Engine* obsEngine() const { return itsObsEngine; }
#endif

 private:
  void parse_references();

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
  // supported wms epsg references. Map from name to GDAL definition
  std::map<std::string, std::string> itsSupportedWMSReferences;
  // the bounding boxes for all spatial references
  std::map<std::string, Engine::Gis::BBox> itsWMSBBoxes;

  bool itsInspireExtensionSupported = false;

  // Valid WMS layers (name -> layer proxy). This must be a shared pointer
  // and must be used via atomic_load and atomic_store, since CTPP::CDT is not thread safe.

  using LayerMap = std::map<std::string, WMSLayerProxy>;
  boost::shared_ptr<LayerMap> itsLayers;

  std::unique_ptr<boost::thread> itsGetCapabilitiesThread;

  void capabilitiesUpdateLoop();
  void updateLayerMetaData();

#ifndef WITHOUT_OBSERVATION
  std::set<std::string> getObservationProducers() const;
#endif
  // For locking purposes
  bool isValidLayerImpl(const std::string& theLayer, bool theAcceptHiddenLayerFlag = false) const;

  friend class WMSLayerFactory;

  // Shutdown variables

  boost::atomic<int> itsActiveThreadCount;
  boost::atomic<bool> itsShutdownRequested;
  boost::mutex itsShutdownMutex;
  boost::condition_variable itsShutdownCondition;

  std::map<std::string, Json::Value> itsLegendGraphicLayers;
  // configuration info for legend (parameter names, units, legend size)
  WMSLegendGraphicSettings itsLegendGraphicSettings;
  // Keep track of product file modification times and report if time is changed
  std::map<std::string, std::time_t> itsProductFileModificationTime;
};  // class WMSConfig

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
