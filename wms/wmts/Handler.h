// ======================================================================
/*!
 * \brief WMTS REST request handler
 *
 * Handles:
 *   GET /wmts/1.0.0/WMTSCapabilities.xml          → GetCapabilities
 *   GET /wmts/1.0.0/{layer}/{style}/{tms}/{tm}/{row}/{col}.{ext}  → GetTile
 *   GET /wmts/1.0.0/{layer}/{style}/{tms}/{tm}/{row}/{col}/{j}/{i}.{ext}
 *                                                 → GetFeatureInfo
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "../ogc/QueryStatus.h"
#include <spine/HTTP.h>
#include <macgyver/Exception.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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
class Handler;
}
namespace WMTS
{
using OGC::QueryStatus;

class Handler
{
 public:
  Handler(const Dali::Config& daliConfig);
  ~Handler() = default;
  Handler(const Handler&) = delete;
  Handler& operator=(const Handler&) = delete;
  Handler(Handler&&) = delete;
  Handler& operator=(Handler&&) = delete;

  void init(std::unique_ptr<Config> wmtsConfig);
  void shutdown();

  // GetFeatureInfo is delegated to the WMS handler (the request is translated
  // into WMS GetFeatureInfo vocabulary); the pointer is wired by the Plugin
  // after both handlers exist. Not owned.
  void setWMSHandler(WMS::Handler* wmsHandler) { itsWMSHandler = wmsHandler; }

  QueryStatus query(Spine::Reactor& theReactor,
                    Dali::State& theState,
                    const Spine::HTTP::Request& theRequest,
                    Spine::HTTP::Response& theResponse);

 private:
  QueryStatus handleGetCapabilities(Dali::State& theState,
                                    const Spine::HTTP::Request& theRequest,
                                    Spine::HTTP::Response& theResponse);

  QueryStatus handleGetTile(Dali::State& theState,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse,
                            const std::string& layer,
                            const std::string& style,
                            const std::string& tms_id,
                            const std::string& tm_id,
                            unsigned tile_row,
                            unsigned tile_col,
                            const std::string& format,
                            const std::vector<std::string>& dimensionValues = {});

  QueryStatus handleGetFeatureInfo(Spine::Reactor& theReactor,
                                   Dali::State& theState,
                                   const Spine::HTTP::Request& theRequest,
                                   Spine::HTTP::Response& theResponse,
                                   const std::string& layer,
                                   const std::string& style,
                                   const std::string& tms_id,
                                   const std::string& tm_id,
                                   unsigned tile_row,
                                   unsigned tile_col,
                                   unsigned pixel_j,
                                   unsigned pixel_i,
                                   const std::string& info_format,
                                   const std::vector<std::string>& dimensionValues = {});

  QueryStatus generateTile(Dali::State& theState,
                           const Spine::HTTP::Request& theRequest,
                           Spine::HTTP::Response& theResponse,
                           Dali::Product& theProduct);

  void sendException(const std::string& code,
                     const std::string& text,
                     Dali::State& theState,
                     const Spine::HTTP::Request& theRequest,
                     Spine::HTTP::Response& theResponse);

  // Ordered RESTful dimension identifiers for a layer (e.g. ["time",
  // "reference_time", "elevation"]), matching the order GetCapabilities
  // advertises. Cached: the set of dimensions a layer exposes is fixed by its
  // configuration and does not change between model runs (only the values do),
  // so this avoids a per-tile querydata lookup during animation.
  const std::vector<std::string>& orderedDimensionNames(const std::string& layer) const;

  const Dali::Config& itsDaliConfig;
  std::unique_ptr<Config> itsWMTSConfig;
  WMS::Handler* itsWMSHandler = nullptr;  // not owned; see setWMSHandler()

  mutable std::mutex itsDimNamesMutex;
  mutable std::map<std::string, std::vector<std::string>> itsDimNamesCache;
};

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
