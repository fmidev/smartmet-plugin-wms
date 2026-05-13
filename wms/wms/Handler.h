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
#include <macgyver/Cache.h>
#include <macgyver/DateTime.h>
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

  // ------------------------------------------------------------
  // Server-side cache for GetCapabilities responses.
  //
  // GetCapabilities is the dominant URL on this plugin for some
  // workloads (aviation/observation hardware clients polling every
  // few seconds, ~4.5 MB body per response). Rendering it costs
  // ~440 ms per call and is invariant for the vast majority of
  // requests, since clients normally use no filter parameters.
  //
  // The cache key is a hash over the allow-listed request inputs
  // that actually affect the body. The cache value caches both the
  // rendered string and its ETag, plus enough freshness metadata
  // (config version + wall-clock built_at + TTL) to decide whether
  // a hit is still serveable.
  //
  // Sized by entry count (the structural cardinality is bounded by
  // the allowlist — a misconfigured client randomizing a query
  // param does not fragment the cache because unknown params are
  // ignored in the key). 25 entries gives ~110 MB worst case
  // (25 * 4.5 MB body); in practice the population sits in the
  // low double digits.
  // ------------------------------------------------------------

  struct CachedCapabilities
  {
    std::shared_ptr<std::string> body;  // shared so 304/204 paths can drop the body cheaply
    std::size_t product_hash;           // == Fmi::hash_value(*body); passed to formatResponse
    std::string etag;                   // == fmt::sprintf("\"%x\"", product_hash)
    Fmi::DateTime config_version;       // == Config::getCapabilitiesModificationTime() at render
    Fmi::DateTime built_at;             // wall-clock at render; used for TTL
  };

  using CapabilitiesCache = Fmi::Cache::Cache<std::size_t, CachedCapabilities>;
  std::unique_ptr<CapabilitiesCache> itsCapabilitiesCache;

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
