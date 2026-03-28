// ======================================================================
/*!
 * \brief WMTS REST request handler
 *
 * Handles:
 *   GET /wmts/1.0.0/WMTSCapabilities.xml          → GetCapabilities
 *   GET /wmts/1.0.0/{layer}/{style}/{tms}/{tm}/{row}/{col}.{ext}  → GetTile
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "../ogc/QueryStatus.h"
#include <spine/HTTP.h>
#include <macgyver/Exception.h>
#include <memory>
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
                            const std::string& format);

  QueryStatus generateTile(Dali::State& theState,
                           const Spine::HTTP::Request& theRequest,
                           Spine::HTTP::Response& theResponse,
                           Dali::Product& theProduct);

  void sendException(const std::string& code,
                     const std::string& text,
                     Dali::State& theState,
                     const Spine::HTTP::Request& theRequest,
                     Spine::HTTP::Response& theResponse);

  const Dali::Config& itsDaliConfig;
  std::unique_ptr<Config> itsWMTSConfig;
};

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
