// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "WMSLayer.h"
#include "WMSLayerProxy.h"
#include "WMSLegendGraphicSettings.h"
#include <boost/move/unique_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/utility.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/querydata/Engine.h>
#include <engines/grid/Engine.h>
#include <spine/JsonCache.h>
#include <spine/Thread.h>
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
            const Spine::JsonCache& theJsonCache,
            Engine::Querydata::Engine* qEngine,
#ifndef WITHOUT_AUTHENTICATION
            Engine::Authentication::Engine* authEngine,
#endif
#ifndef WITHOUT_OBSERVATION
            Engine::Observation::Engine* obsEngine,
#endif
            Engine::Gis::Engine* gisEngine,
            Engine::Grid::Engine* gridEngine);

#ifndef WITHOUT_AUTHENTICATION
  CTPP::CDT getCapabilities(const boost::optional<std::string>& apikey,
                            const boost::optional<std::string>& starttime,
                            const boost::optional<std::string>& endtime,
                            const boost::optional<std::string>& wms_namespace,
                            bool authenticate = true) const;
#else
  CTPP::CDT getCapabilities(const boost::optional<std::string>& apikey,
                            const boost::optional<std::string>& starttime,
                            const boost::optional<std::string>& endtime,
                            const boost::optional<std::string>& wms_namespace) const;
#endif

  void init();

  std::string layerCustomer(const std::string& theLayerName) const;
  const std::set<std::string>& supportedMapFormats() const;
  const std::set<std::string>& supportedWMSVersions() const;
  const std::set<std::string>& supportedWMSExceptions() const;
  const std::set<std::string>& supportedWMSGetCapabilityFormats() const;
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
  Json::Value json(const std::string& theLayerName) const;
  std::vector<Json::Value> getLegendGraphic(const std::string& theLayerName,
                                            const std::string& theStyleName,
                                            std::size_t& width,
                                            std::size_t& height,
                                            const std::string& language) const;

  bool inspireExtensionSupported() const;
  CTPP::CDT getCapabilitiesResponseVariables() const;

  void shutdown();

  void getLegendGraphic(const std::string& theLayerName,
                        const std::string& theStyleName,
                        Dali::Product& theProduct,
                        const Dali::State& theState,
                        const std::string& language) const;

  const Spine::JsonCache& getJsonCache() const { return itsJsonCache; }
  const Plugin::Dali::Config& getDaliConfig() const { return itsDaliConfig; }
  const Engine::Querydata::Engine* qEngine() const { return itsQEngine; }
  const Engine::Gis::Engine* gisEngine() const { return itsGisEngine; }
  const Engine::Grid::Engine* gridEngine() const { return itsGridEngine; }
#ifndef WITHOUT_OBSERVATION
  const Engine::Observation::Engine* obsEngine() const { return itsObsEngine; }
#endif

  const WMSLegendGraphicSettings& getLegendGraphicSettings() const;

  int getMargin() const { return itsMargin; }

 private:
  void parse_references();

  const Plugin::Dali::Config& itsDaliConfig;
  const Spine::JsonCache& itsJsonCache;

  // Engines for GetCapabilities
  Engine::Querydata::Engine* itsQEngine = nullptr;
  Engine::Gis::Engine* itsGisEngine = nullptr;
  Engine::Grid::Engine* itsGridEngine = nullptr;


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
  // supported wms exceptions
  std::set<std::string> itsSupportedWMSExceptions;
  // supported wms getcapability formats
  std::set<std::string> itsSupportedWMSGetCapabilityFormats;
  // supported wms epsg references. Map from name to GDAL definition
  std::map<std::string, std::string> itsSupportedWMSReferences;
  // the bounding boxes for all spatial references
  std::map<std::string, Engine::Gis::BBox> itsWMSBBoxes;

  bool itsCapabilityUpdatesDisabled = false;  // disable updates after initial scan?
  int itsCapabilityUpdateInterval = 5;        // scan interval in seconds

  bool itsInspireExtensionSupported = false;

  // Valid WMS layers (name -> layer proxy). This must be a shared pointer
  // and must be used via atomic_load and atomic_store, since CTPP::CDT is not thread safe.

  using LayerMap = std::map<std::string, WMSLayerProxy>;
  boost::shared_ptr<LayerMap> itsLayers;

  boost::movelib::unique_ptr<boost::thread> itsGetCapabilitiesThread;

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

  // Default margin for arrows, symbols, numbers etc is 0 pixels for backward
  // compatibility. This value should be about half of the size of the biggest
  // symbol in use. Layers using bigger symbols should probably use a layer specific
  // setting so that processing other layers will not slow down unnecessarily.
  int itsMargin = 0;

  // Set of files for which a warning has already been printed
  std::set<std::string> itsWarnedFiles;

};  // class WMSConfig

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
