#include "StyleSelection.h"
#include <macgyver/Exception.h>
#include <spine/Convenience.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
void handleLayerStyle(Json::Value& layerJson,
                      const Json::Value& qid,
                      const Json::Value& layer_type,
                      const Json::Value& isoLB,
                      const Json::Value& css)
{
  Json::Value nulljson;
  auto layerQid = layerJson.get("qid", nulljson);
  if (!layerQid.isNull() && qid == layerQid)
  {
    auto layerType = layerJson.get("layer_type", nulljson);
    if (layerType == layer_type && !layerType.isNull())
    {
      if (layerType.asString() == "isoline" && !isoLB.isNull())
      {
        Json::Value& isolines = layerJson["isolines"];
        isolines = isoLB;
      }
      else if (layerType.asString() == "isoband" && !isoLB.isNull())
      {
        Json::Value& isobands = layerJson["isobands"];
        isobands = isoLB;
      }
    }
    Json::Value& cssJson = layerJson["css"];
    if (!cssJson.isNull() && !css.isNull())
      cssJson = css;
  }

  auto subLayers = layerJson.get("layers", nulljson);
  if (!subLayers.isNull() && subLayers.isArray())
    for (auto& subLayer : layerJson["layers"])
      handleLayerStyle(subLayer, qid, layer_type, isoLB, css);
}

void useStyle(Json::Value& root, const Json::Value& styles)
{
  Json::Value nulljson;

  auto nameJson = styles.get("name", nulljson);
  auto titleJson = styles.get("name", nulljson);

  auto styleLayers = styles.get("layers", nulljson);
  if (!styleLayers.isNull() && styleLayers.isArray())
  {
    for (const auto& styleLayer : styleLayers)
    {
      auto qid = styleLayer.get("qid", nulljson);
      if (qid.isNull())
        continue;

      auto layer_type = styleLayer.get("layer_type", nulljson);
      std::string layerTypeString = (!layer_type.isNull() ? layer_type.asString() : "");
      auto isolines = styleLayer.get("isolines", nulljson);
      auto isobands = styleLayer.get("isobands", nulljson);
      auto css = styleLayer.get("css", nulljson);
      auto viewsJson = root.get("views", nulljson);

      if (!viewsJson.isNull() && viewsJson.isArray())
      {
        for (auto& viewJson : root["views"])
        {
          auto viewLayers = viewJson.get("layers", nulljson);
          if (!viewLayers.isNull() && viewLayers.isArray())
          {
            for (auto& layerJson : viewJson["layers"])
              handleLayerStyle(layerJson,
                               qid,
                               layer_type,
                               (layerTypeString == "isoline" ? isolines : isobands),
                               css);
          }
        }
      }
    }
  }
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
