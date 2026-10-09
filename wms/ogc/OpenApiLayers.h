// ======================================================================
/*!
 * \brief Layer information for the generated OpenAPI definitions
 *
 * The OGC API - Tiles and WMTS definitions list the layers that anonymous
 * clients can see, with the values a request may use. Layers restricted
 * with an apikey are never listed.
 */
// ======================================================================

#pragma once

#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
}  // namespace Dali
namespace WMS
{
class Config;
}  // namespace WMS
namespace OGC
{
struct OpenApiLayer
{
  std::string id;
  std::string title;
  std::vector<std::string> styles;      // style names, "default" first
  std::vector<std::string> elevations;  // selectable levels, empty for surface data
  std::vector<std::string> dimensions;  // WMTS dimension identifiers (Time, Reference_time,
                                        // Elevation) in the order of the ResourceURL template
};

// The layers of the anonymous capabilities, in capabilities order
std::vector<OpenApiLayer> openApiLayers(const WMS::Config& wmsConfig,
                                        const Dali::Config& daliConfig);

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
