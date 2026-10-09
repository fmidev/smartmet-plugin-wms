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
  std::vector<std::string> values;
  boost::algorithm::split(values, dim.At("value").GetString(), boost::is_any_of(","));
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

}  // namespace

std::vector<OpenApiLayer> openApiLayers(const WMS::Config& wmsConfig,
                                        const Dali::Config& daliConfig)
{
  try
  {
    const auto& language = daliConfig.defaultLanguage();

    // Anonymous capabilities: restricted layers are not listed
    CTPP::CDT layers = wmsConfig.getCapabilities(
        std::nullopt, language, {}, {}, {}, {}, LayerHierarchy::HierarchyType::flat, false, false);

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
      {
        CTPP::CDT& dim = layer.At("elevation_dimension");
        if (dim.GetType() == CTPP::CDT::ARRAY_VAL)
          for (std::size_t j = 0; j < dim.Size(); ++j)
            add_elevations(dim[j], info.elevations);
        else if (dim.GetType() == CTPP::CDT::HASH_VAL)
          add_elevations(dim, info.elevations);
      }

      // In the order of the WMTS ResourceURL template
      for (const char* name : {"time_dimension", "elevation_dimension"})
        if (layer.Exists(name))
          add_dimensions(layer.At(name), info.dimensions);

      // The default style is always available
      info.styles.emplace_back("default");
      auto layer_obj = wmsConfig.getLayer(info.id);
      if (layer_obj)
      {
        auto style_info = layer_obj->getStyleInfo(language, language);
        if (style_info && style_info->Exists("style"))
        {
          CTPP::CDT& styles = (*style_info)["style"];
          if (styles.GetType() == CTPP::CDT::ARRAY_VAL)
            for (std::size_t j = 0; j < styles.Size(); ++j)
              if (styles[j].Exists("name"))
              {
                auto name = styles[j].At("name").GetString();
                if (std::find(info.styles.begin(), info.styles.end(), name) == info.styles.end())
                  info.styles.push_back(name);
              }
        }
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

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
