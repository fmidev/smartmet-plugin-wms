// ======================================================================
/*!
 * \brief SmartMet Dali plugin
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "Product.h"
#include "TemplateFactory.h"
#include "WMSConfig.h"
#include "WMSException.h"
#include "WMSGetCapabilities.h"

#include <engines/querydata/Engine.h>
#include <engines/contour/Engine.h>
#include <engines/geonames/Engine.h>
#include <engines/gis/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif

#include <spine/SmartMetPlugin.h>
#include <spine/HTTP.h>
#include <spine/FileCache.h>
#include <spine/Reactor.h>
#include <spine/SmartMetCache.h>

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

// To signify various return types of the WMS query
enum class WMSQueryStatus
{
  OK,
  EXCEPTION,
  FORBIDDEN
};

using ImageCache = SmartMet::Spine::SmartMetCache;

class Plugin : public SmartMetPlugin, private boost::noncopyable
{
 public:
  Plugin(SmartMet::Spine::Reactor* theReactor, const char* theConfig);
  virtual ~Plugin();

  const std::string& getPluginName() const;
  int getRequiredAPIVersion() const;
  bool queryIsFast(const SmartMet::Spine::HTTP::Request& theRequest) const;

  // Get the engines
  const SmartMet::Engine::Querydata::Engine& getQEngine() const { return *itsQEngine; }
  const SmartMet::Engine::Contour::Engine& getContourEngine() const { return *itsContourEngine; }
  const SmartMet::Engine::Gis::Engine& getGisEngine() const { return *itsGisEngine; }
  const SmartMet::Engine::Geonames::Engine& getGeoEngine() const { return *itsGeoEngine; }
#ifndef WITHOUT_OBSERVATION
  SmartMet::Engine::Observation::Engine& getObsEngine() const { return *itsObsEngine; }
#endif
  // Plugin specific public API:

  const Config& getConfig() const;
  SharedFormatter getTemplate(const std::string& theName) const;
  Product getProduct(const SmartMet::Spine::HTTP::Request& theRequest,
                     const State& theState,
                     const std::string& theName,
                     bool theDebugFlag) const;
  std::string getStyle(const std::string& theCustomer,
                       const std::string& theCSS,
                       bool theWmsFlag) const;
  std::string getFilter(const std::string& theName, bool theWmsFlag) const;
  std::size_t getFilterHash(const std::string& theName, bool theWmsFlag) const;
  std::string getMarker(const std::string& theCustomer,
                        const std::string& theSymbol,
                        bool theWmsFlag) const;
  std::size_t getMarkerHash(const std::string& theCustomer,
                            const std::string& theSymbol,
                            bool theWmsFlag) const;
  std::string getSymbol(const std::string& theCustomer,
                        const std::string& theSymbol,
                        bool theWmsFlag) const;
  std::size_t getSymbolHash(const std::string& theCustomer,
                            const std::string& theSymbol,
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

 protected:
  void init();
  void shutdown();
  void requestHandler(SmartMet::Spine::Reactor& theReactor,
                      const SmartMet::Spine::HTTP::Request& theRequest,
                      SmartMet::Spine::HTTP::Response& theResponse);

 private:
  Plugin();

  void daliQuery(SmartMet::Spine::Reactor& theReactor,
                 const SmartMet::Spine::HTTP::Request& req,
                 SmartMet::Spine::HTTP::Response& response);
  WMSQueryStatus wmsQuery(SmartMet::Spine::Reactor& theReactor,
                          const SmartMet::Spine::HTTP::Request& req,
                          SmartMet::Spine::HTTP::Response& response);
  void formatResponse(const std::string& theSvg,
                      const std::string& theFormat,
                      const SmartMet::Spine::HTTP::Request& theRequest,
                      SmartMet::Spine::HTTP::Response& theResponse,
                      bool usetimer,
                      std::size_t theHash = 0);
  std::string mimeType(const std::string& theFormat) const;

  std::string parseWMSException(SmartMet::Spine::Exception& wmsException) const;

  // Plugin configuration
  const std::string itsModuleName;
  Config itsConfig;

  // Cache server and engine instances
  SmartMet::Spine::Reactor* itsReactor;
  SmartMet::Engine::Querydata::Engine* itsQEngine;
  SmartMet::Engine::Contour::Engine* itsContourEngine;
  SmartMet::Engine::Gis::Engine* itsGisEngine;
  SmartMet::Engine::Geonames::Engine* itsGeoEngine;
#ifndef WITHOUT_OBSERVATION
  SmartMet::Engine::Observation::Engine* itsObsEngine;
#endif

  // Cache templates
  TemplateFactory itsTemplateFactory;

  // Cache files
  mutable SmartMet::Spine::FileCache itsFileCache;

  // Cache results
  mutable std::unique_ptr<ImageCache> itsImageCache;

  // WMS configuration
  std::unique_ptr<WMS::WMSConfig> itsWMSConfig;
  // WMS Capabilities
  std::unique_ptr<WMS::WMSGetCapabilities> itsWMSGetCapabilities;

};  // class Plugin

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
