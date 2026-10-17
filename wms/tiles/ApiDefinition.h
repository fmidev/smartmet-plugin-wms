// ======================================================================
/*!
 * \brief Generated OpenAPI definition of the OGC API - Tiles interface
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
namespace Tiles
{
class Config;

// The definition of /tiles. Paths are relative to the tiles root URL.
// The title and description of the API, also used in the API catalog
extern const char* const API_TITLE;
extern const char* const API_DESCRIPTION;

Spine::OpenAPI::Document apiDefinition(const Config& config,
                                       const std::vector<OGC::OpenApiLayer>& layers,
                                       const Spine::OpenAPI::Settings& settings);

// Conformance classes of /tiles/conformance
std::vector<std::string> conformanceClasses();

}  // namespace Tiles
}  // namespace Plugin
}  // namespace SmartMet
