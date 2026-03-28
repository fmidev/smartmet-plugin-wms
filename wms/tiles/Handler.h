// ======================================================================
/*!
 * \brief OGC API - Tiles request handler
 *
 * Implements the OGC API - Tiles 1.0 (OGC 20-057) REST interface:
 *
 *   GET /tiles                                                   → Landing page
 *   GET /tiles/conformance                                       → Conformance classes
 *   GET /tiles/tileMatrixSets                                    → TileMatrixSet list
 *   GET /tiles/tileMatrixSets/{tmsId}                           → TileMatrixSet definition
 *   GET /tiles/collections                                       → Collection list
 *   GET /tiles/collections/{collId}                             → Collection metadata
 *   GET /tiles/collections/{collId}/tiles                       → Available tile sets
 *   GET /tiles/collections/{collId}/tiles/{tmsId}               → Tileset metadata
 *   GET /tiles/collections/{collId}/tiles/{tmsId}/{tm}/{row}/{col} → Tile image
 *
 * Tile format negotiated via 'f' query parameter or Accept header.
 * GeoTIFF and Protobuf/MVT formats are intentionally not implemented here.
 */
// ======================================================================

#pragma once

#include "Config.h"
#include "../ogc/QueryStatus.h"
#include <spine/HTTP.h>
#include <macgyver/Exception.h>
#include <memory>
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
namespace Tiles
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

  void init(std::unique_ptr<Config> tilesConfig);
  void shutdown();

  QueryStatus query(Spine::Reactor& theReactor,
                    Dali::State& theState,
                    const Spine::HTTP::Request& theRequest,
                    Spine::HTTP::Response& theResponse);

 private:
  // Build "protocol://host[/apikey]/tiles" base URL for link generation
  std::string computeBaseUrl(const Spine::HTTP::Request& req) const;

  // Negotiate tile image format from 'f' param or Accept header; default image/png
  std::string negotiateFormat(const Spine::HTTP::Request& req) const;

  // Metadata endpoints — all return JSON
  QueryStatus handleLandingPage(const std::string& base, Spine::HTTP::Response& resp);
  QueryStatus handleConformance(const std::string& base, Spine::HTTP::Response& resp);
  QueryStatus handleTileMatrixSets(const std::string& base, Spine::HTTP::Response& resp);
  QueryStatus handleTileMatrixSet(const std::string& base,
                                   const std::string& tmsId,
                                   Spine::HTTP::Response& resp);
  QueryStatus handleCollections(const std::string& base,
                                 Dali::State& theState,
                                 const Spine::HTTP::Request& theRequest,
                                 Spine::HTTP::Response& resp);
  QueryStatus handleCollection(const std::string& base,
                                const std::string& collId,
                                Dali::State& theState,
                                const Spine::HTTP::Request& theRequest,
                                Spine::HTTP::Response& resp);
  QueryStatus handleCollectionTilesets(const std::string& base,
                                        const std::string& collId,
                                        Spine::HTTP::Response& resp);
  QueryStatus handleTilesetMetadata(const std::string& base,
                                     const std::string& collId,
                                     const std::string& tmsId,
                                     Spine::HTTP::Response& resp);

  // Tile rendering endpoint
  QueryStatus handleGetTile(Dali::State& theState,
                             const Spine::HTTP::Request& theRequest,
                             Spine::HTTP::Response& theResponse,
                             const std::string& collId,
                             const std::string& tmsId,
                             const std::string& tmId,
                             unsigned row,
                             unsigned col,
                             const std::string& format);

  QueryStatus generateTile(Dali::State& theState,
                            const Spine::HTTP::Request& theRequest,
                            Spine::HTTP::Response& theResponse,
                            Dali::Product& product);

  // Send application/problem+json error response (RFC 7807)
  void sendError(int status,
                 const std::string& title,
                 const std::string& detail,
                 Spine::HTTP::Response& resp);

  static void setJsonResponse(Spine::HTTP::Response& resp, const std::string& body);

  const Dali::Config& itsDaliConfig;
  std::unique_ptr<Config> itsTilesConfig;
};

}  // namespace Tiles
}  // namespace Plugin
}  // namespace SmartMet
