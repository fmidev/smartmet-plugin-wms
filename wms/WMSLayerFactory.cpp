#include "WMSLayerFactory.h"
#include "CaseInsensitiveComparator.h"
#include "StyleSelection.h"
#include "WMSPostGISLayer.h"
#include "WMSQueryDataLayer.h"
#ifndef WITHOUT_OBSERVATION
#include "WMSObservationLayer.h"
#endif
#include <engines/querydata/Engine.h>
#include <macgyver/Exception.h>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
enum class WMSLayerType
{
  PostGISLayer,    // PostGIS layer
  QueryDataLayer,  // Querydata layer
#ifndef WITHOUT_OBSERVATION
  ObservationLayer,  // Observation layer
#endif
  NotWMSLayer
};

static std::set<std::string> querydata_layers = {
    "arrow", "isoband", "isoline", "isolabel", "map", "number", "symbol", "tag", "location"};

static std::set<std::string> postgis_layers = {"postgis", "icemap"};

namespace
{
WMSLayerType getWMSLayerType(const Json::Value& layer)
{
  try
  {
    Json::Value nulljson;
    auto layer_type = layer.get("layer_type", nulljson);
    if (layer_type.isNull())
    {
      // No layer type defined, not a wms layer
      return WMSLayerType::NotWMSLayer;
    }

    std::string type_name = layer_type.asString();

    if (postgis_layers.find(type_name) != postgis_layers.end())
    {
      return WMSLayerType::PostGISLayer;
    }

    if (querydata_layers.find(type_name) != querydata_layers.end())
    {
      return WMSLayerType::QueryDataLayer;
    }

    // No identified special layers, this is not a WMSLayer;
    return WMSLayerType::NotWMSLayer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to get WMS layer type!");
  }
}

std::string getWMSLayerTypeAsString(WMSLayerType layerType)
{
  try
  {
    std::string ret;

    switch (layerType)
    {
      case WMSLayerType::PostGISLayer:
        ret = "PostGISLayer";
        break;
      case WMSLayerType::QueryDataLayer:
        ret = "QueryDataLayer";
        break;
#ifndef WITHOUT_OBSERVATION
      case WMSLayerType::ObservationLayer:
        ret = "ObservationLayer";
        break;
#endif
      default:
        ret = "NotWMSLayer";
        break;
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to get layer type as string!");
  }
}

WMSLayerType findLayerType(const Json::Value& layers, Json::Value& layerRef)
{
  WMSLayerType layer_type = WMSLayerType::NotWMSLayer;
  Json::Value nulljson;

  for (const auto& layer : layers)
  {
    if (!layer.isNull())
    {
      Json::Value sublayers = layer.get("layers", nulljson);
      // if sublayers exists examine them
      if (!sublayers.isNull())
      {
        layer_type = findLayerType(sublayers, layerRef);
        if (layer_type != WMSLayerType::NotWMSLayer)
          return layer_type;
      }
    }

    // Match first layer which defines "layer_type" - directive
    // Layer can have auxiliary layers (such as taglayers) for clipping and such
    if (layer.isNull() || layer["layer_type"].isNull())
    {
      // This is auxiliary layer, does not count
      continue;
    }
    else
    {
      layer_type = getWMSLayerType(layer);

      if (layer_type == WMSLayerType::NotWMSLayer)
      {
        // NotWMSLayer means we should proceed to find a more significant layer
        continue;
      }
      else
      {
        layerRef = layer;
        return layer_type;
      }
    }
  }

  return layer_type;
}
}  // anonymous namespace

// This function deduces the WMS product type. In case of several layer definitions in the product
// the most significant is chosen
WMSLayerType determineProductType(Json::Value& jsonRoot, Json::Value& layerRef)
{
  try
  {
    Json::Value nulljson;
    Json::Value views = jsonRoot.get("views", nulljson);

    Json::Value title = jsonRoot.get("title", nulljson);

    // browse layers and if 'postgis'-layer is found type is WMSLayerType::PostGISLayer
    // otherwise it is WMSLayerType::QueryDataLayer
    if (views.isNull())
    {
      throw Fmi::Exception(BCP, "No views in WMS layer definition");
    }
    else
    {
      const Json::Value& view = views[0];

      Json::Value layers = view.get("layers", nulljson);
      if (layers.isNull())
      {
        throw Fmi::Exception(BCP, "No layers in WMS layer definition");
      }
      else
      {
        WMSLayerType layer_type = findLayerType(layers, layerRef);  // The resolved layer type

        if (layer_type != WMSLayerType::NotWMSLayer)
          return layer_type;

        Json::Value hidden = jsonRoot.get("hidden", nulljson);
        if (!hidden.isNull() && hidden.asBool())
          return WMSLayerType::QueryDataLayer;

        // If we are here, the most significant encountered layer is a NotWMSLayer.
        // This is currently an error, WMS layer must contain some spatio-temporal data

        // Misconfigured layer is this
        throw Fmi::Exception(BCP, "No valid Dali layer definitions in WMS layer definition");
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to determine layer product type!");
  }
}

SharedWMSLayer WMSLayerFactory::createWMSLayer(const std::string& theFileName,
                                               const std::string& theNamespace,
                                               const std::string& theCustomer,
                                               const WMSConfig& theWMSConfig)
{
  try
  {
    Json::Value root = WMSLayer::readJsonFile(theFileName);

    const bool use_wms = true;
    Spine::JSON::preprocess(
        root,
        theWMSConfig.itsDaliConfig.rootDirectory(use_wms),
        theWMSConfig.itsDaliConfig.rootDirectory(use_wms) + "/customers/" + theCustomer + "/layers",
        theWMSConfig.itsJsonCache);
    Spine::JSON::dereference(root);

    SharedWMSLayer layer;
    Json::Value parsedLayer;
    WMSLayerType layerType = WMSLayerType::NotWMSLayer;

    // producer timestep given in root level
    std::string producer = root["producer"].asString();
    std::string timestep = root["timestep"].asString();

    std::set<std::string> obsProducers = theWMSConfig.getObservationProducers();

    // observation layer is reasoned from producer,
    // other layers from layer type as defined in product file
    if (obsProducers.find(producer) != obsProducers.end())
      layerType = WMSLayerType::ObservationLayer;
    else
      layerType = determineProductType(root, parsedLayer);

    boost::filesystem::path p(theFileName);

    switch (layerType)
    {
      case WMSLayerType::PostGISLayer:
      {
        layer = boost::make_shared<WMSPostGISLayer>(theWMSConfig, parsedLayer);
        break;
      }
      case WMSLayerType::QueryDataLayer:
      {
        // if no producer defined, let's use default producer
        if (producer.empty())
          producer = theWMSConfig.itsDaliConfig.defaultModel();
        layer = boost::make_shared<WMSQueryDataLayer>(theWMSConfig, producer);
        break;
      }
      case WMSLayerType::ObservationLayer:
      {
#ifndef WITHOUT_OBSERVATION
        if (timestep.empty())
          timestep = "-1";
        // timestep -1 indicates that no timestep is given in product-file
        // in that case timestep is read from obsengine configuration file (default value is 1min)
        layer =
            boost::make_shared<WMSObservationLayer>(theWMSConfig, producer, std::stoi(timestep));
#endif
        break;
      }
      default:  // we should never end up here since determineLayerType should always return a
                // valid layer type
        throw Fmi::Exception(BCP, "WMS Layer enum not handled in WMSLayerFactory.");
    };

    // Generic variables. The quiet flag must be set before
    // updateLayerMetaData is called.

    layer->quiet = theWMSConfig.itsDaliConfig.quiet();
    layer->productFile = theFileName;
    layer->setCustomer(theCustomer);

    // WMS GetCapability settings

    Json::Value nulljson;

    // for hiding the layer from GetCapabilities queries for example if the layer is not public
    auto json = root.get("hidden", nulljson);
    if (!json.isNull())
      layer->hidden = json.asBool();

    json = root.get("disable_wms_time_dimension", nulljson);
    if (!json.isNull())
      layer->timeDimensionDisabled = json.asBool();

    json = root.get("name", nulljson);
    if (!json.isNull())
      layer->name = json.asString();
    else
      layer->name = theNamespace + ":" + p.stem().string();

    json = root.get("title", nulljson);
    if (!json.isNull())
      layer->title = json.asString();

    json = root.get("abstract", nulljson);
    if (!json.isNull())
      layer->abstract = json.asString();

    json = root.get("opaque", nulljson);
    if (!json.isNull())
      layer->opaque = json.asBool();

    json = root.get("queryable", nulljson);
    if (!json.isNull())
      layer->queryable = json.asBool();

    json = root.get("cascaded", nulljson);
    if (!json.isNull())
      layer->cascaded = json.asBool();

    json = root.get("no_subsets", nulljson);
    if (!json.isNull())
      layer->no_subsets = json.asBool();

    json = root.get("fixed_width", nulljson);
    if (!json.isNull())
      layer->fixed_width = json.asInt();

    json = root.get("fixed_height", nulljson);
    if (!json.isNull())
      layer->fixed_height = json.asInt();

    json = root.get("keyword", nulljson);
    if (!json.isNull())
    {
      layer->keywords = std::set<std::string>();
      if (json.isString())
        layer->keywords->insert(json.asString());
      else if (json.isArray())
      {
        for (unsigned int i = 0; i < json.size(); i++)
          layer->keywords->insert(json[i].asString());
      }
      else
        throw Fmi::Exception(
            BCP, p.string() + " keyword value must be an array of strings or a string");
    }

    json = root.get("crs", nulljson);
    if (!json.isNull())
    {
      if (!json.isObject())
        throw Fmi::Exception(BCP, "top level crs setting must be a group");

      auto j = json.get("enable", nulljson);
      if (!j.isNull())
        Spine::JSON::extract_set("crs.enable", layer->enabled_refs, j);

      j = json.get("disable", nulljson);
      if (!j.isNull())
        Spine::JSON::extract_set("crs.disable", layer->disabled_refs, j);
    }

    // Update metadata from DB etc

    layer->updateLayerMetaData();

    // handle styles

    // 1) read "legend_url_layer"-parameter and use it to generate styles entry
    json = root.get("legend_url_layer", nulljson);
    if (!json.isNull())
    {
      WMSLayerStyle layerStyle;
      std::string legendURLLayer = json.asString();
      layer->addStyle(legendURLLayer, layerStyle);
    }

    if (layer->itsStyles.size() == 0)
    {
      bool addDefaultStyle = true;
      // 2) read styles vector from configuration and generate layer styles
      json = root.get("styles", nulljson);
      if (!json.isNull())
      {
        if (!json.isArray())
          throw Fmi::Exception(BCP, "WMSLayer styles settings must be an array");

        WMSLayerStyle layerStyle;

        for (unsigned int i = 0; i < json.size(); i++)
        {
          const Json::Value& style_json = json[i];
          auto json_value = style_json.get("name", nulljson);
          if (!json_value.isNull())
            layerStyle.name = json_value.asString();
          json_value = style_json.get("title", nulljson);
          if (!json_value.isNull())
            layerStyle.title = json_value.asString();
          json_value = style_json.get("abstract", nulljson);
          if (!json_value.isNull())
            layerStyle.abstract = json_value.asString();

          auto legend_url_json = style_json.get("legend_url", nulljson);
          if (!legend_url_json.isNull())
          {
            json_value = legend_url_json.get("format", nulljson);
            if (!json_value.isNull())
              layerStyle.legend_url.format = json_value.asString();
            json_value = legend_url_json.get("online_resource", nulljson);
            if (!json_value.isNull())
              layerStyle.legend_url.online_resource = json_value.asString();
            layer->itsStyles.push_back(layerStyle);
          }
          else
          {
            // Modify right style for the product and add getCapabilities Style info
            Json::Value modifiedLayer = root;
            useStyle(modifiedLayer, style_json);
            layer->addStyle(modifiedLayer, layer->name, layerStyle);

            CaseInsensitiveComparator cicomp;
            if (cicomp(layerStyle.name, "default"))
              addDefaultStyle = false;
          }
        }
        if (addDefaultStyle)
        {
          WMSLayerStyle layerStyle;
          layer->addStyle(root, layer->name, layerStyle);
        }
      }
    }

    // 3) generate layer styles automatically from configuration
    if (layer->itsStyles.size() == 0)
    {
      WMSLayerStyle layerStyle;
      layer->addStyle(root, layer->name, layerStyle);
    }

    // Spatial references supported by default, if applicable
    layer->refs = theWMSConfig.supportedWMSReferences();

    // And extra references supported by the layer itself

    Json::Value projection = root["projection"];
    std::string mapcrs = projection["crs"].asString();

#if 0  // What's this for? All native projections should be on the supported list anyway.
    // Validate namespace
    if (mapcrs.substr(0, 5) == "EPSG:" || mapcrs.substr(0, 4) == "CRS:")
      layer->crs.insert(std::make_pair(mapcrs, mapcrs));
#endif

    // Calculate projected bboxes only once to speed up GetCapabilities
    layer->initProjectedBBoxes();

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to create WMS layer!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
