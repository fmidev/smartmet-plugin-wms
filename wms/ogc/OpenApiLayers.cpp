#include "OpenApiLayers.h"
#include "../Config.h"
#include "../wms/Config.h"
#include <boost/algorithm/string.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <algorithm>
#include <cctype>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{
// Selectable levels of an elevation dimension. A single level 0 is surface data, which
// has nothing to select.
void add_elevations(CTPP::CDT& dim, std::vector<std::string>& elevations)
{
  if (!dim.Exists("value"))
    return;
  // boost 1.69 (RHEL8) split does not accept a temporary
  const std::string value_list = dim.At("value").GetString();
  std::vector<std::string> values;
  boost::algorithm::split(values, value_list, boost::is_any_of(","));
  values.erase(std::remove(values.begin(), values.end(), ""), values.end());
  if (values.size() == 1)
  {
    try
    {
      if (Fmi::stod(values.front()) == 0.0)
        return;
    }
    catch (...)
    {
    }
  }
  for (const auto& v : values)
    if (std::find(elevations.begin(), elevations.end(), v) == elevations.end())
      elevations.push_back(v);
}

// The WMTS identifier of a dimension: the name capitalized (see wmts/Handler.cpp)
void add_dimension(CTPP::CDT& dim, std::vector<std::string>& dimensions)
{
  if (!dim.Exists("name"))
    return;
  std::string identifier = dim.At("name").GetString();
  if (identifier.empty())
    return;
  for (auto& c : identifier)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  identifier[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(identifier[0])));
  dimensions.push_back(identifier);
}

void add_dimensions(CTPP::CDT& node, std::vector<std::string>& dimensions)
{
  if (node.GetType() == CTPP::CDT::ARRAY_VAL)
    for (std::size_t i = 0; i < node.Size(); ++i)
      add_dimension(node[i], dimensions);
  else if (node.GetType() == CTPP::CDT::HASH_VAL)
    add_dimension(node, dimensions);
}

// Selectable levels of all the elevation dimensions
void add_all_elevations(CTPP::CDT& dim, std::vector<std::string>& elevations)
{
  if (dim.GetType() == CTPP::CDT::ARRAY_VAL)
    for (std::size_t j = 0; j < dim.Size(); ++j)
      add_elevations(dim[j], elevations);
  else if (dim.GetType() == CTPP::CDT::HASH_VAL)
    add_elevations(dim, elevations);
}

// Add the names of the styles in the capabilities style list, skipping duplicates
void add_styles(CTPP::CDT& styles, std::vector<std::string>& names)
{
  if (styles.GetType() != CTPP::CDT::ARRAY_VAL)
    return;
  for (std::size_t j = 0; j < styles.Size(); ++j)
    if (styles[j].Exists("name"))
    {
      auto name = styles[j].At("name").GetString();
      if (std::find(names.begin(), names.end(), name) == names.end())
        names.push_back(name);
    }
}

}  // namespace

std::vector<OpenApiLayer> openApiLayers(const WMS::Config& wmsConfig,
                                        const Dali::Config& daliConfig,
                                        const std::optional<std::string>& apikey)
{
  try
  {
    const auto& language = daliConfig.defaultLanguage();

    // The layers the apikey has access to, as in the capabilities
    CTPP::CDT layers = wmsConfig.getCapabilities(
        apikey, language, {}, {}, {}, {}, LayerHierarchy::HierarchyType::flat, false, false);

    std::vector<OpenApiLayer> ret;
    for (std::size_t i = 0; i < layers.Size(); ++i)
    {
      CTPP::CDT& layer = layers[i];
      if (!layer.Exists("name"))
        continue;

      OpenApiLayer info;
      info.id = layer.At("name").GetString();
      if (layer.Exists("title"))
        info.title = layer.At("title").GetString();

      if (layer.Exists("elevation_dimension"))
        add_all_elevations(layer.At("elevation_dimension"), info.elevations);

      // In the order of the WMTS ResourceURL template
      for (const char* name : {"time_dimension", "elevation_dimension", "member_dimension"})
        if (layer.Exists(name))
          add_dimensions(layer.At(name), info.dimensions);

      // The default style is always available
      info.styles.emplace_back("default");
      auto layer_obj = wmsConfig.getLayer(info.id);
      if (layer_obj)
      {
        auto style_info = layer_obj->getStyleInfo(language, language);
        if (style_info && style_info->Exists("style"))
          add_styles((*style_info)["style"], info.styles);
      }

      ret.push_back(std::move(info));
    }
    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to list the layers for the OpenAPI definition");
  }
}

std::shared_ptr<const Spine::OpenAPI::Definition> OpenApiCache::get(
    const WMS::Config& wmsConfig,
    const Dali::Config& daliConfig,
    const std::optional<std::string>& apikey,
    const Builder& builder)
{
  try
  {
    const std::string key = apikey ? *apikey : std::string();
    const auto modified = wmsConfig.getCapabilitiesModificationTime();

    {
      std::scoped_lock lock(itsMutex);
      auto& entry = itsEntries[key];
      entry.used = ++itsCounter;
      if (entry.definition && (entry.modified == modified || entry.building))
        return entry.definition;
      entry.building = true;
    }

    std::shared_ptr<const Spine::OpenAPI::Definition> definition;
    try
    {
      definition = std::make_shared<const Spine::OpenAPI::Definition>(
          builder(openApiLayers(wmsConfig, daliConfig, apikey)));
    }
    catch (...)
    {
      std::scoped_lock lock(itsMutex);
      auto it = itsEntries.find(key);
      if (it != itsEntries.end())
        it->second.building = false;
      throw;
    }

    std::scoped_lock lock(itsMutex);
    auto& entry = itsEntries[key];
    entry.definition = definition;
    entry.modified = modified;
    entry.building = false;

    // Drop the least recently used definitions, never one being regenerated
    while (itsEntries.size() > itsMaxEntries)
    {
      auto oldest = itsEntries.end();
      for (auto it = itsEntries.begin(); it != itsEntries.end(); ++it)
        if (!it->second.building &&
            (oldest == itsEntries.end() || it->second.used < oldest->second.used))
          oldest = it;
      if (oldest == itsEntries.end())
        break;
      itsEntries.erase(oldest);
    }

    return definition;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to get the OpenAPI definition");
  }
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
