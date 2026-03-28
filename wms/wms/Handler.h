// ======================================================================
/*!
 * \brief SmartMet WMS HTTP handler
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "Exception.h"
#include "../ogc/QueryStatus.h"
#include <json/value.h>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>
#include <spine/JsonCache.h>
#include <memory>
#include <set>
#include <string>

namespace SmartMet
{
namespace Spine
{
class Reactor;
}
namespace Plugin
{
namespace Dali
{
class Config;
class Product;
class State;
}
namespace WMS
{
using OGC::QueryStatus;

class Handler
{
 public:
  Handler(const Dali::Config& daliConfig, Spine::JsonCache& jsonCache);
  ~Handler();
  Handler(const Handler&) = delete;
  Handler& operator=(const Handler&) = delete;
  Handler(Handler&&) = delete;
  Handler& operator=(Handler&&) = delete;

  void init(std::unique_ptr<Config> wmsConfig);
  void shutdown();

  QueryStatus query(Spine::Reactor& theReactor,
                    Dali::State& theState,
                    const Spine::HTTP::Request& theRequest,
                    Spine::HTTP::Response& theResponse);

 private:
  const Dali::Config& itsDaliConfig;
  Spine::JsonCache& itsJsonCache;
  std::unique_ptr<Config> itsWMSConfig;

  bool authenticate(const Spine::HTTP::Request& theRequest) const;

  QueryStatus wmsGetMapQuery(Dali::State& theState,
                             const Spine::HTTP::Request& theRequest,
                             Spine::HTTP::Response& theResponse);
  QueryStatus wmsGetLegendGraphicQuery(Dali::State& theState,
                                       const Spine::HTTP::Request& theRequest,
                                       Spine::HTTP::Response& theResponse);
  QueryStatus wmsGetFeatureInfoQuery(Dali::State& theState,
                                     const Spine::HTTP::Request& theRequest,
                                     Spine::HTTP::Response& theResponse);
  QueryStatus wmsGetCapabilitiesQuery(Dali::State& theState,
                                      const Spine::HTTP::Request& theRequest,
                                      Spine::HTTP::Response& theResponse);
  void wmsPrepareGetLegendGraphicQuery(const Dali::State& theState,
                                       Spine::HTTP::Request& theRequest,
                                       Dali::Product& product) const;
  QueryStatus wmsGenerateProduct(Dali::State& theState,
                                 const Spine::HTTP::Request& theRequest,
                                 Spine::HTTP::Response& theResponse,
                                 Dali::Product& theProduct);
  QueryStatus wmsGenerateFeatureInfo(Dali::State& theState,
                                     const Spine::HTTP::Request& theRequest,
                                     Spine::HTTP::Response& theResponse,
                                     Dali::Product& theProduct);
  void wmsPreprocessJSON(Dali::State& theState,
                         const Spine::HTTP::Request& theRequest,
                         const std::string& theName,
                         Json::Value& theJson,
                         bool isCnfRequest,
                         int theStage);
  void formatWmsExceptionResponse(Fmi::Exception& wmsException,
                                  const std::string& theFormat,
                                  bool isdebug,
                                  Dali::State& theState,
                                  const Spine::HTTP::Request& theRequest,
                                  Spine::HTTP::Response& theResponse);
  QueryStatus handleWmsException(Fmi::Exception& exception,
                                 Dali::State& theState,
                                 const Spine::HTTP::Request& theRequest,
                                 Spine::HTTP::Response& theResponse);
  static Json::Value getExceptionJson(const std::string& description,
                                      const std::string& mapFormat,
                                      ExceptionFormat format,
                                      unsigned int width,
                                      unsigned int height);
  std::string getExceptionFormat(const std::string& theFormat) const;
  std::string getCapabilityFormat(const std::string& theFormat) const;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
