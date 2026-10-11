// ======================================================================
/*!
 * \brief Generated OpenAPI definition of the WMTS RESTful interface
 *
 * WMTS is not an OGC API, the definition documents the RESTful resources of
 * WMTS 1.0.0 (OGC 07-057r7) for clients and code generators.
 */
// ======================================================================

#pragma once

#include "../ogc/OpenApiLayers.h"
#include <spine/OpenApi.h>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WMTS
{
class Config;

// The definition of /wmts. Paths are relative to the WMTS root URL.
Spine::OpenAPI::Document apiDefinition(const Config& config,
                                       const std::vector<OGC::OpenApiLayer>& layers,
                                       const Spine::OpenAPI::Settings& settings);

}  // namespace WMTS
}  // namespace Plugin
}  // namespace SmartMet
