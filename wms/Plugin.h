// ======================================================================
/*!
 * \brief SmartMet Dali plugin
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "Product.h"
#include "StyleSheet.h"
#include "WMSConfig.h"
#include "WMSException.h"
#include "WMSQueryStatus.h"
#include <engines/contour/Engine.h>
#include <engines/geonames/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/querydata/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <boost/move/unique_ptr.hpp>
#include <macgyver/TemplateFactory.h>
#include <spine/FileCache.h>
#include <spine/HTTP.h>
#include <spine/JsonCache.h>
#include <spine/Reactor.h>
#include <spine/SmartMetCache.h>
#include <spine/SmartMetPlugin.h>

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
  const Engine::Grid::Engine* getGridEngine() const { return itsGridEngine; }
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

 private:
  void init() override;
  void shutdown() override;

  bool authenticate(const Spine::HTTP::Request& theRequest) const;

  void requestHandler(Spine::Reactor& theReactor,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse) override;

  void daliQuery(Spine::Reactor& theReactor,
                 State& theState,
                 const Spine::HTTP::Request& theRequest,
                 Spine::HTTP::Response& theResponse);
  WMSQueryStatus wmsQuery(Spine::Reactor& theReactor,
                          State& theState,
                          const Spine::HTTP::Request& theRequest,
                          Spine::HTTP::Response& theResponse);
  WMSQueryStatus wmsGetMapQuery(State& theState,
                                const Spine::HTTP::Request& theRequest,
                                Spine::HTTP::Response& theResponse);
  WMSQueryStatus wmsGetLegendGraphicQuery(State& theState,
                                          const Spine::HTTP::Request& theRequest,
                                          Spine::HTTP::Response& theResponse);
  WMSQueryStatus wmsGetCapabilitiesQuery(State& theState,
                                         const Spine::HTTP::Request& theRequest,
                                         Spine::HTTP::Response& theResponse);
  void wmsPrepareGetLegendGraphicQuery(const State& theState,
                                       Spine::HTTP::Request& theRequest,
                                       Product& product) const;
  WMSQueryStatus wmsGenerateProduct(State& theState,
                                    const Spine::HTTP::Request& theRequest,
                                    Spine::HTTP::Response& theResponse,
                                    Product& theProduct);
  void wmsPreprocessJSON(State& theState,
                         const Spine::HTTP::Request& theRequest,
                         Json::Value& theJson,
                         bool isCnfRequest,
                         int theStage);

  void formatResponse(const std::string& theSvg,
                      const std::string& theType,
                      const Spine::HTTP::Request& theRequest,
                      Spine::HTTP::Response& theResponse,
                      bool usetimer,
                      const Product& theProduct = Product(),
                      std::size_t theHash = 0);

  std::string getExceptionFormat(const std::string& theFormat) const;
  std::string getCapabilityFormat(const std::string& theFormat) const;

  std::string parseWMSException(Fmi::Exception& wmsException,
                                const std::string& theFormat,
                                bool isdebug) const;

  void formatWmsExceptionResponse(Fmi::Exception& wmsException,
                                  const std::string& theFormat,
                                  bool isdebug,
                                  const Spine::HTTP::Request& theRequest,
                                  Spine::HTTP::Response& theResponse,
                                  bool usetimer);
  WMSQueryStatus handleWmsException(Fmi::Exception& exception,
                                    State& theState,
                                    const Spine::HTTP::Request& theRequest,
                                    Spine::HTTP::Response& theResponse);
  static Json::Value getExceptionJson(const std::string& description,
                                      const std::string& mapFormat,
                                      WmsExceptionFormat format,
                                      unsigned int width,
                                      unsigned int height);

  Fmi::Cache::CacheStatistics getCacheStats() const override;
  std::string resolveFilePath(const std::string& theCustomer,
                              const std::string& theSubDir,
                              const std::string& theFileName,
                              bool theWmsFlag) const;
  std::string resolveSvgPath(const std::string& theCustomer,
                             const std::string& theSubDir,
                             const std::string& theFileName,
                             bool theWmsFlag) const;

  // Plugin configuration
  const std::string itsModuleName;
  Config itsConfig;

  // Cache server and engine instances
  Spine::Reactor* itsReactor;
  Engine::Grid::Engine* itsGridEngine;
  Engine::Querydata::Engine* itsQEngine;
  Engine::Contour::Engine* itsContourEngine;
  Engine::Gis::Engine* itsGisEngine;
  Engine::Geonames::Engine* itsGeoEngine;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine* itsObsEngine;
#endif

  // Cache templates
  Fmi::TemplateFactory itsTemplateFactory;

  // Cache files
  mutable Spine::FileCache itsFileCache;
  mutable Spine::JsonCache itsJsonCache;

  // Style sheet cache
  Fmi::Cache::Cache<std::size_t, StyleSheet> itsStyleSheetCache;

  // Cache results
  mutable boost::movelib::unique_ptr<ImageCache> itsImageCache;

  // WMS configuration
  boost::movelib::unique_ptr<WMS::WMSConfig> itsWMSConfig;

};  // class Plugin

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
