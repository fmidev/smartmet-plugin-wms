// ======================================================================
/*!
 * \brief SmartMet Dali plugin
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "Product.h"
#include "StyleSheet.h"
#include "wms/Handler.h"
#include "wmts/Handler.h"
#include <engines/authentication/Engine.h>
#include <engines/contour/Engine.h>
#include <engines/geonames/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/querydata/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <macgyver/TemplateFactory.h>
#include <spine/FileCache.h>
#include <spine/HTTP.h>
#include <spine/JsonCache.h>
#include <spine/Reactor.h>
#include <spine/SmartMetCache.h>
#include <spine/SmartMetPlugin.h>
#include <memory>
#include <set>

namespace SmartMet
{
namespace Spine
{
class Parameter;
}

namespace Plugin
{
namespace Dali
{
class Filter;

using ImageCache = Spine::SmartMetCache;

class Plugin : public SmartMetPlugin
{
 public:
  ~Plugin() override;
  Plugin(Spine::Reactor* theReactor, const char* theConfig);

  Plugin() = delete;
  Plugin(const Plugin& other) = delete;
  Plugin& operator=(const Plugin& other) = delete;
  Plugin(Plugin&& other) = delete;
  Plugin& operator=(Plugin&& other) = delete;

  const std::string& getPluginName() const override;
  int getRequiredAPIVersion() const override;
  bool queryIsFast(const Spine::HTTP::Request& theRequest) const override;

  // Get the engines
  const Engine::Grid::Engine* getGridEngine() const { return itsGridEngine.get(); }
  const Engine::Querydata::Engine& getQEngine() const { return *itsQEngine; }
  const Engine::Contour::Engine& getContourEngine() const { return *itsContourEngine; }
  const Engine::Gis::Engine& getGisEngine() const { return *itsGisEngine; }
  const Engine::Geonames::Engine& getGeoEngine() const { return *itsGeoEngine; }
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine& getObsEngine() const { return *itsObsEngine; }
#endif
  // Plugin specific public API:

  const Config& getConfig() const;
  Fmi::SharedFormatter getTemplate(const std::string& theName) const;

  Json::Value getProductJson(const Spine::HTTP::Request& theRequest,
                             const State& theState,
                             const std::string& theName,
                             int stage) const;
  std::string getStyle(const std::string& theCustomer,
                       const std::string& theCSS,
                       bool theWmsFlag) const;
  std::map<std::string, std::string> getStyle(const std::string& theCustomer,
                                              const std::string& theCSS,
                                              bool theWmsFlag,
                                              const std::string& theSelector);
  std::string getFilter(const std::string& theCustomer,
                        const std::string& theName,
                        bool theWmsFlag) const;
  std::size_t getFilterHash(const std::string& theCustomer,
                            const std::string& theName,
                            bool theWmsFlag) const;
  std::string getMarker(const std::string& theCustomer,
                        const std::string& theName,
                        bool theWmsFlag) const;
  std::size_t getMarkerHash(const std::string& theCustomer,
                            const std::string& theName,
                            bool theWmsFlag) const;
  std::string getSymbol(const std::string& theCustomer,
                        const std::string& theName,
                        bool theWmsFlag) const;
  std::size_t getSymbolHash(const std::string& theCustomer,
                            const std::string& theName,
                            bool theWmsFlag) const;
  std::string getPattern(const std::string& theCustomer,
                         const std::string& theName,
                         bool theWmsFlag) const;
  std::size_t getPatternHash(const std::string& theCustomer,
                             const std::string& theName,
                             bool theWmsFlag) const;
  std::string getGradient(const std::string& theCustomer,
                          const std::string& theName,
                          bool theWmsFlag) const;
  std::size_t getGradientHash(const std::string& theCustomer,
                              const std::string& theName,
                              bool theWmsFlag) const;
  std::string getColorMap(const std::string& theCustomer,
                          const std::string& theName,
                          bool theWmsFlag) const;
  std::size_t getColorMapHash(const std::string& theCustomer,
                              const std::string& theName,
                              bool theWmsFlag) const;

  void formatResponse(const std::string& theSvg,
                      const std::string& theType,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse,
                      bool usetimer,
                      const Product& theProduct = Product(),
                      std::size_t theHash = 0);

  std::shared_ptr<std::string> findInImageCache(std::size_t hash) const;

  static Spine::HTTP::ParamMap extractValidParameters(const Spine::HTTP::ParamMap& theParams);

 private:
  friend class WMS::Handler;

  void init() override;
  void shutdown() override;

  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse) override;

  void daliQuery(Spine::Reactor& theReactor,
                 State& theState,
                 const Spine::HTTP::Request& theRequest,
                 Spine::HTTP::Response& theResponse);

  Fmi::Cache::CacheStatistics getCacheStats() const override;
  std::string resolveFilePath(const std::string& theCustomer,
                              const std::string& theSubDir,
                              const std::string& theFileName,
                              bool theWmsFlag,
                              std::list<std::string>& theTestedPaths) const;
  std::string resolveSvgPath(const std::string& theCustomer,
                             const std::string& theSubDir,
                             const std::string& theFileName,
                             bool theWmsFlag) const;

  static void print(const ParameterInfos& infos);

  // Plugin configuration
  const std::string itsModuleName;
  Config itsConfig;

  // Cache server and engine instances
  Spine::Reactor* itsReactor;
  std::shared_ptr<Engine::Grid::Engine> itsGridEngine;
  std::shared_ptr<Engine::Querydata::Engine> itsQEngine;
  std::shared_ptr<Engine::Contour::Engine> itsContourEngine;
  std::shared_ptr<Engine::Gis::Engine> itsGisEngine;
  std::shared_ptr<Engine::Geonames::Engine> itsGeoEngine;
  std::shared_ptr<Engine::Authentication::Engine> authEngine;
#ifndef WITHOUT_OBSERVATION
  std::shared_ptr<Engine::Observation::Engine> itsObsEngine;
#endif

  // Cache templates
  Fmi::TemplateFactory itsTemplateFactory;

  // Cache files
  mutable Spine::FileCache itsFileCache;
  mutable Spine::JsonCache itsJsonCache;

  // Style sheet cache
  Fmi::Cache::Cache<std::size_t, StyleSheet> itsStyleSheetCache;

  // Cache results
  mutable std::unique_ptr<ImageCache> itsImageCache;

  // WMS handler (owns WMS configuration and state)
  std::unique_ptr<WMS::Handler> itsWMSHandler;
  WMS::Config* itsWMSConfig = nullptr;  // non-owning pointer into itsWMSHandler

  // WMTS handler (shares WMS layer registry via itsWMSConfig)
  std::unique_ptr<WMTS::Handler> itsWMTSHandler;

  // URLs which have already generated a warning or qid duplicates or on some other problem. We do
  // not wish to fill the logs with the same warnings again and again.
  std::set<std::string> itsWarnedURLs;

};  // class Plugin

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
