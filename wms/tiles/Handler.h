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
 * OGC API - Styles (Mapbox style encoding), for styling the MVT output with the
 * real wms-conf colours:
 *
 *   GET /tiles/styles                                            → Style set list
 *   GET /tiles/collections/{collId}/styles                       → Styles for a collection
 *   GET /tiles/collections/{collId}/styles/{styleId}?f=mapbox    → Mapbox GL style document
 *
 * Tile format negotiated via 'f' query parameter or Accept header.
 * GeoTIFF and Protobuf/MVT formats are intentionally not implemented here.
 */
// ======================================================================

#pragma once

#include "../MapboxStyle.h"
#include "../ogc/QueryStatus.h"
#include "Config.h"
#include <json/json.h>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>
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
}  // namespace Dali
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

  // OGC API - Styles endpoints (Mapbox style encoding)
  QueryStatus handleStyles(const std::string& base,
                           Dali::State& theState,
                           const Spine::HTTP::Request& theRequest,
                           Spine::HTTP::Response& resp);
  QueryStatus handleCollectionStyles(const std::string& base,
                                     const std::string& collId,
                                     Spine::HTTP::Response& resp);
  QueryStatus handleStyle(const std::string& base,
                          const std::string& collId,
                          const std::string& styleId,
                          Dali::State& theState,
                          const Spine::HTTP::Request& theRequest,
                          Spine::HTTP::Response& resp);

  // Resolve every styleable (isoband/isoline) MVT layer in a collection's
  // product: source-layer (qid), level table and CSS — the join material for a
  // Mapbox style. Returns the layers in product order (empty when none).
  std::vector<Dali::MapboxStyleLayer> resolveStyleLayers(const std::string& collId,
                                                         const std::string& styleId,
                                                         Dali::State& theState,
                                                         const Spine::HTTP::Request& theRequest);

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
