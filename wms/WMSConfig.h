// ======================================================================
/*!
 * \brief Configuration file API for WMS
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "WMSLayer.h"
#include "WMSLayerHierarchy.h"
#include "WMSLayerProxy.h"
#include "WMSLegendGraphicSettings.h"
#include "WMSSupportedReference.h"
#include <boost/move/unique_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/smart_ptr/atomic_shared_ptr.hpp>
#include <boost/utility.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/querydata/Engine.h>
#include <macgyver/AsyncTask.h>
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

  virtual ~WMSConfig();

  WMSConfig() = delete;
  WMSConfig(const WMSConfig& other) = delete;
  WMSConfig operator=(const WMSConfig& other) = delete;
  WMSConfig(WMSConfig&& other) = delete;
  WMSConfig operator=(WMSConfig&& other) = delete;

#ifndef WITHOUT_AUTHENTICATION
  CTPP::CDT getCapabilities(const boost::optional<std::string>& apikey,
                            const std::string& language,
                            const boost::optional<std::string>& starttime,
                            const boost::optional<std::string>& endtime,
                            const boost::optional<std::string>& reference_time,
                            const boost::optional<std::string>& wms_namespace,
                            WMSLayerHierarchy::HierarchyType hierarchy_type,
							bool show_hidden,
                            bool multiple_intervals,
                            bool authenticate = true) const;
#else
  CTPP::CDT getCapabilities(const boost::optional<std::string>& apikey,
                            const std::string& language,
                            const boost::optional<std::string>& starttime,
                            const boost::optional<std::string>& endtime,
                            const boost::optional<std::string>& reference_time,
                            const boost::optional<std::string>& wms_namespace,
                            WMSLayerHierarchy::HierarchyType hierarchy_type,
							bool show_hidden,
                            bool multiple_intervals) const;
#endif

  void init();

  std::string layerCustomer(const std::string& theLayerName) const;
  const std::set<std::string>& supportedMapFormats() const;
  const std::set<std::string>& supportedWMSVersions() const;
  const std::set<std::string>& supportedWMSExceptions() const;
  const std::set<std::string>& supportedWMSGetCapabilityFormats() const;
  const std::map<std::string, WMSSupportedReference>& supportedWMSReferences() const;
  std::string getCRSDefinition(const std::string& theCRS) const;
  bool isValidMapFormat(const std::string& theMapFormat) const;
  bool isValidVersion(const std::string& theVersion) const;
  bool isValidLayer(const std::string& theLayer, bool theAcceptHiddenLayerFlag = true) const;
  bool isValidStyle(const std::string& theLayer, const std::string& theStyle) const;
  bool isValidCRS(const std::string& theLayer, const std::string& theCRS) const;
  bool isValidInterval(const std::string& theLayer, int interval_start, int interval_end) const;
  std::pair<std::string, std::string> getDefaultInterval(const std::string& theLayer) const;

#ifndef WITHOUT_AUTHENTICATION
  bool validateGetMapAuthorization(const Spine::HTTP::Request& theRequest) const;
#endif

  bool isValidElevation(const std::string& theLayer, int theElevation) const;
  bool isValidTime(const std::string& theLayer,
                   const Fmi::DateTime& theTime,
                   const boost::optional<Fmi::DateTime>& theReferenceTime) const;
  bool isValidReferenceTime(const std::string& theLayer,
                            const Fmi::DateTime& theReferenceTime) const;

  bool isTemporal(const std::string& theLayer) const;
  bool currentValue(const std::string& theLayer) const;
  Fmi::DateTime mostCurrentTime(
      const std::string& theLayer,
      const boost::optional<Fmi::DateTime>& reference_time) const;
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
  WMSLayerHierarchy::HierarchyType getLayerHierarchyType() const { return itsLayerHierarchyType; }
  bool multipleIntervals() const { return itsMultipleIntervals; }
  Fmi::DateTime getCapabilitiesExpirationTime() const;
  Fmi::DateTime getCapabilitiesModificationTime() const
  {
    return itsCapabilitiesModificationTime;
  }

#ifndef WITHOUT_OBSERVATION
  std::set<std::string> getObservationProducers() const;
#endif

 private:
  void parse_references();
  CTPP::CDT get_capabilities(const libconfig::Config& config) const;
  CTPP::CDT get_request(const libconfig::Config& config,
                        const std::string& prefix,
                        const std::string& variable) const;

  const Plugin::Dali::Config& itsDaliConfig;
  const Spine::JsonCache& itsJsonCache;

  // Engines for GetCapabilities, not owned pointers
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

  std::map<std::string, WMSSupportedReference> itsWMSSupportedReferences;

  // supported length units and their conversion factor to meters
  std::map<std::string, double> itsLengthUnits;

  // supported AUTO2 spatial references
  std::map<int, std::string> itsAutoProjections;

  bool itsCapabilityUpdatesDisabled = false;  // disable updates after initial scan?
  int itsCapabilityUpdateInterval = 5;        // scan interval in seconds
  int itsCapabilityExpirationTime = 60;

  bool itsInspireExtensionSupported = false;

  // Valid WMS layers (name -> layer proxy). This must be a shared pointer
  // and must be used via atomic_load and atomic_store, since CTPP::CDT is not thread safe.

  using LayerMap = std::map<std::string, WMSLayerProxy>;
  boost::atomic_shared_ptr<LayerMap> itsLayers;

  std::unique_ptr<Fmi::AsyncTask> itsGetCapabilitiesTask;

  void capabilitiesUpdateLoop();

  void updateLayerMetaData();

  void updateLayerMetaDataForCustomer(
      const boost::filesystem::directory_iterator& dir,
      const boost::shared_ptr<LayerMap>& mylayers,
      LayerMap& newProxies,
      std::map<SharedWMSLayer, std::map<std::string, std::string>>& externalLegends);

  void updateLayerMetaDataForCustomerLayer(
      const boost::filesystem::recursive_directory_iterator& itr,
      const std::string& customer,
      const std::string& productdir,
      const boost::shared_ptr<LayerMap>& mylayers,
      LayerMap& newProxies,
      std::map<SharedWMSLayer, std::map<std::string, std::string>>& externalLegends);

  void updateModificationTime();

  // For locking purposes
  bool isValidLayerImpl(const std::string& theLayer, bool theAcceptHiddenLayerFlag = true) const;

  friend class WMSLayerFactory;

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

  // Mode of layer hierarchy
  WMSLayerHierarchy::HierarchyType itsLayerHierarchyType = WMSLayerHierarchy::HierarchyType::flat;

  // Flag indicates if multiple intervals are allowed
  bool itsMultipleIntervals = false;

  // Set of files for which a warning has already been printed
  std::set<std::string> itsWarnedFiles;

  Fmi::DateTime itsCapabilitiesModificationTime = boost::posix_time::from_time_t(0);

};  // class WMSConfig

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
