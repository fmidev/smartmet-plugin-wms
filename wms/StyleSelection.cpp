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
// Use layer qid as a key
std::string getKey(const Json::Value& json)
{
  Json::Value nulljson;

  auto qid = json.get("qid", nulljson);
  if (qid.isNull())
    return "";

  auto layer_type = json.get("layer_type", nulljson);
  if (layer_type.isNull())
    return "";
  std::string layerTypeString = layer_type.asString();
  if (layerTypeString.empty() ||
      supportedStyleLayers.find(layerTypeString) == supportedStyleLayers.end())
    return "";

  return qid.asString();
}

// Replace view layer attributes with style layer attributes
void addOrReplace(Json::Value& viewLayerJson,
                  const Json::Value& styleLayerJson,
                  const std::string& key)
{
  Json::Value nulljson;
  if (!key.empty())
  {
    auto styleJson = styleLayerJson.get(key, nulljson);
    if (!styleJson.isNull())
      viewLayerJson[key] = styleJson;
  }
}

void handleStyles(std::map<std::string, Json::Value*> viewLayers,
                  std::map<std::string, const Json::Value*> styleLayers)
{
  Json::Value nulljson;

  for (auto& item : viewLayers)
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

	Json::Value::Members styleLayerMembers = styleLayerJson.getMemberNames();

	for (const auto& name : styleLayerMembers)
	  {
		if(name != "qid")
		  {
			addOrReplace(viewLayerJson, styleLayerJson, name);
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

void useStyle(Json::Value& root, const Json::Value& styles)
{
  Json::Value nulljson;

  std::map<std::string, const Json::Value*> styleL;
  std::map<std::string, Json::Value*> viewL;

  if (!styles.isMember("layers"))
    return;
  const auto& styleLayers = styles.get("layers", nulljson);
  if (!styleLayers.isNull() && styleLayers.isArray())
  {
    for (const auto& styleLayer : styles["layers"])
    {
      std::string key = getKey(styleLayer);
      if (key.empty())
        continue;

      styleL.insert(std::make_pair(key, &styleLayer));
    }
  }

  auto& views = root["views"];
  if (views.isNull())
    return;
  for (auto& viewJson : views)
  {
    auto viewLayers = viewJson.get("layers", nulljson);
    if (!viewLayers.isNull() && viewLayers.isArray())
    {
      for (auto& viewLayer : viewJson["layers"])
        handleViewLayer(viewLayer, viewL);
    }
  }

  handleStyles(viewL, styleL);
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
