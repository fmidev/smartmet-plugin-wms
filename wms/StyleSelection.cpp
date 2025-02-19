#include "StyleSelection.h"
#include "WMSLayerStyle.h"
#include <macgyver/Exception.h>
#include <spine/Convenience.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
const std::string STARTING_TAGS_KEY = "_TAGS_BEFORE_ANY_LAYER_";

// Use layer qid as a key
std::string getKey(const Json::Value& json)
{
  Json::Value nulljson;

  auto qid = json.get("qid", nulljson);
  if (qid.isNull())
    return "";

#if 0
  // Why repeat layer_type in styles? Just keep the old one
  auto layer_type = json.get("layer_type", nulljson);
  if (layer_type.isNull())
    return "";
  std::string layerTypeString = layer_type.asString();
  if (layerTypeString.empty())
    return "";
#endif

  return qid.asString();
}

// Replace view layer attributes with style layer attributes
void addOrReplace(Json::Value& viewLayerJson,
                  const Json::Value& styleLayerJson,
                  const std::string& key)
{
  // Note: We allow replacements with null value on purpose so that a style can disable settings
  if (!key.empty() && styleLayerJson.isMember(key))
    viewLayerJson[key] = styleLayerJson[key];
}

void handleStyles(const std::map<std::string, Json::Value*>& viewLayers,
                  std::map<std::string, const Json::Value*>& styleLayers)
{
  Json::Value nulljson;

  for (const auto& item : viewLayers)
  {
    std::string key = item.first;

    if (styleLayers.find(key) == styleLayers.end())
      continue;

    auto& viewLayerJson = *item.second;

    if (viewLayerJson.isNull())
      continue;

    const auto& styleLayerJson = *(styleLayers.at(key));

    if (styleLayerJson.isNull())
      continue;

    if (!viewLayerJson.isMember("layer_type"))
      return;

    auto layerType = viewLayerJson.get("layer_type", nulljson);
    if (layerType.isNull())
      return;

    // If the layer type changes, we remove all old settings first since the
    // settings are most likely not compatible, and a complete layer change is wanted.
    // An exception is a null-layer which is never rendered, and hence changing the layer
    // type implies we want to keep any existing settings as default ones.

    auto newLayerType = styleLayerJson.get("layer_type", nulljson);
    if (!newLayerType.isNull() && layerType != newLayerType && layerType != "null")
      viewLayerJson = Json::Value(Json::objectValue);

    // Then insert new style settings over the old ones, including the qid since the above
    // code may have deleted it
    Json::Value::Members styleLayerMembers = styleLayerJson.getMemberNames();

    for (const auto& name : styleLayerMembers)
      addOrReplace(viewLayerJson, styleLayerJson, name);
  }
}

void handleTagStyles(Json::Value& views,
                     std::map<std::string, std::vector<const Json::Value*>>& styleLTags)
{
  if (styleLTags.empty())
    return;

  Json::Value nulljson;

  for (auto& viewJson : views)
  {
    auto viewLayers = viewJson.get("layers", nulljson);
    if (!viewLayers.isNull() && viewLayers.isArray())
    {
      auto& refViewLayers = viewJson["layers"];
      refViewLayers.clear();

      // Add in the beginning
      if (styleLTags.find(STARTING_TAGS_KEY) != styleLTags.end())
      {
        const auto& tag_layers = styleLTags.at(STARTING_TAGS_KEY);
        for (const auto& tag_layer : tag_layers)
          refViewLayers.append(*tag_layer);
        styleLTags.erase(STARTING_TAGS_KEY);
      }

      for (const auto& viewLayer : viewLayers)
      {
        auto qid_json = viewLayer.get("qid", nulljson);
        std::string key = qid_json.asString();
        refViewLayers.append(viewLayer);

        // Tag layer(s) is inserted in the same place where it was in the styles-section
        if (styleLTags.find(key) != styleLTags.end())
        {
          const auto& tag_layers = styleLTags.at(key);
          for (const auto& tag_layer : tag_layers)
            refViewLayers.append(*tag_layer);
        }
      }
    }
  }
}

void handleViewLayer(Json::Value& viewLayer, std::map<std::string, Json::Value*>& viewL)
{
  if (viewLayer.isNull())
    return;

  std::string key = getKey(viewLayer);

  if (!key.empty())
    viewL.insert(std::make_pair(key, &viewLayer));

  // Handle sublayers
  Json::Value nulljson;
  auto subLayers = viewLayer.get("layers", nulljson);
  if (!subLayers.isNull() && subLayers.isArray())
    for (auto& subLayer : viewLayer["layers"])
      handleViewLayer(subLayer, viewL);
}

void collect_styles(const Json::Value& styles,
                    std::map<std::string, const Json::Value*>& styleL,
                    std::map<std::string, std::vector<const Json::Value*>>& styleLTags)
{
  Json::Value nulljson;

  const auto& styleLayers = styles.get("layers", nulljson);
  std::string previous_key = STARTING_TAGS_KEY;
  if (styleLayers.isArray())
  {
    for (const auto& styleLayer : styles["layers"])
    {
      std::string key = getKey(styleLayer);
      if (key.empty())
      {
        auto tag = styleLayer.get("tag", nulljson);
        if (!tag.isNull())
        {
          if (styleLTags.find(previous_key) != styleLTags.end())
            styleLTags.at(previous_key).push_back(&styleLayer);
          else
          {
            std::vector<const Json::Value*> layer_vector;
            layer_vector.push_back(&styleLayer);
            styleLTags.insert(std::make_pair(previous_key, layer_vector));
          }
        }
      }
      else
      {
        previous_key = key;
        styleL.insert(std::make_pair(key, &styleLayer));
      }
    }
  }
}

void collect_views(Json::Value& views, std::map<std::string, Json::Value*>& viewL)
{
  if (views.isNull())
    return;

  Json::Value nulljson;

  for (auto& viewJson : views)
  {
    auto viewLayers = viewJson.get("layers", nulljson);
    if (!viewLayers.isNull() && viewLayers.isArray())
    {
      for (auto& viewLayer : viewJson["layers"])
        handleViewLayer(viewLayer, viewL);
    }
  }
}

}  // namespace

// --- Visible functions ---

void useStyle(Json::Value& root, const Json::Value& styles)
{
  if (!styles.isMember("layers"))
    return;

  Json::Value nulljson;

  std::map<std::string, const Json::Value*> data_styles;
  std::map<std::string, std::vector<const Json::Value*>> tag_styles;

  collect_styles(styles, data_styles, tag_styles);

  std::map<std::string, Json::Value*> viewL;
  auto& views = root["views"];
  collect_views(views, viewL);

  // Data-layers (supportedStyleLayers) are handled here
  handleStyles(viewL, data_styles);

  // Tag-layers area handled here
  handleTagStyles(views, tag_styles);
}

void useStyle(Json::Value& root, const std::string& styleName)
{
  if (styleName.empty())
    return;

  Json::Value nulljson;

  auto json = root.get("styles", nulljson);
  if (!json.isNull())
  {
    if (!json.isArray())
      throw Fmi::Exception(BCP, "WMSLayer styles settings must be an array");

    for (const auto& styleJson : json)
    {
      const auto& nameJson = styleJson.get("name", nulljson);

      if (!nameJson.isNull() && nameJson.asString() == styleName)
      {
        useStyle(root, styleJson);
        break;
      }
    }
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
