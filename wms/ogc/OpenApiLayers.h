// ======================================================================
/*!
 * \brief Layer information for the generated OpenAPI definitions
 *
 * The OGC API - Tiles and WMTS definitions list the layers the client may
 * see, with the values a request may use. Like the capabilities, a request
 * with an apikey sees only the layers the apikey has access to, so the
 * definitions are generated separately for each apikey.
 */
// ======================================================================

#pragma once

#include <macgyver/DateTime.h>
#include <spine/OpenApi.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
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

// The layers of the capabilities for the apikey, in capabilities order
std::vector<OpenApiLayer> openApiLayers(const WMS::Config& wmsConfig,
                                        const Dali::Config& daliConfig,
                                        const std::optional<std::string>& apikey);

// ----------------------------------------------------------------------
/*!
 * \brief Definitions generated for each apikey
 *
 * A definition is regenerated when the capabilities have changed. While a
 * definition is being regenerated, other requests for the same apikey get
 * the previous one instead of waiting. The least recently used definitions
 * are dropped when there are more than maxEntries of them.
 */
// ----------------------------------------------------------------------

class OpenApiCache
{
 public:
  using Builder = std::function<Spine::OpenAPI::Document(const std::vector<OpenApiLayer>&)>;

  explicit OpenApiCache(std::size_t maxEntries = 20) : itsMaxEntries(maxEntries) {}

  std::shared_ptr<const Spine::OpenAPI::Definition> get(const WMS::Config& wmsConfig,
                                                        const Dali::Config& daliConfig,
                                                        const std::optional<std::string>& apikey,
                                                        const Builder& builder);

 private:
  struct Entry
  {
    Fmi::DateTime modified;  // capabilities time of the definition
    std::shared_ptr<const Spine::OpenAPI::Definition> definition;
    bool building = false;
    std::size_t used = 0;  // for dropping the least recently used entries
  };

  std::mutex itsMutex;
  std::map<std::string, Entry> itsEntries;  // apikey ("" for none) to definition
  std::size_t itsCounter = 0;
  std::size_t itsMaxEntries;
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
