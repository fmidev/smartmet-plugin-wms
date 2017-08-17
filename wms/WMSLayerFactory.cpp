#include "WMSLayerFactory.h"

#include "WMSPostGISLayer.h"
#include "WMSQueryDataLayer.h"
#ifndef WITHOUT_OBSERVATION
#include "WMSObservationLayer.h"
#endif
#include <engines/querydata/Engine.h>
#include <spine/Exception.h>
#include <spine/Json.h>

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

static std::set<std::string> querydata_layers = {"arrow", "isoband", "isoline", "number", "symbol"};

static std::set<std::string> postgis_layers = {"postgis", "icemap"};

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
    throw Spine::Exception(BCP, "Failed to get WMS layer type!", NULL);
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
    throw Spine::Exception(BCP, "Failed to get layer type as string!", NULL);
  }
}

// This function deduces the WMS product type. In case of several layer definitions in the product
// the most significant is chosen
WMSLayerType determineProductType(Json::Value& jsonRoot, Json::Value& layerRef)
{
  try
  {
    Json::Value nulljson;
    Json::Value views = jsonRoot.get("views", nulljson);

    // browse layers and if 'postgis'-layer is found type is WMSLayerType::PostGISLayer
    // otherwise it is WMSLayerType::QueryDataLayer
    if (views.isNull())
    {
      throw Spine::Exception(BCP, "No views in WMS layer definition");
    }
    else
    {
      const Json::Value& view = views[0];

      Json::Value layers = view.get("layers", nulljson);
      if (layers.isNull())
      {
        throw Spine::Exception(BCP, "No layers in WMS layer definition");
      }
      else
      {
        WMSLayerType layer_type;  // The resolved layer type

        for (const auto& layer : layers)
        {
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

        Json::Value hidden = jsonRoot.get("hidden", nulljson);
        if (!hidden.isNull() && hidden.asBool())
          return WMSLayerType::QueryDataLayer;

        // If we are here, the most significant encountered layer is a NotWMSLayer.
        // This is currently an error, WMS layer must contain some spatio-temporal data

        // Misconfigured layer is this
        throw Spine::Exception(BCP, "No valid Dali layer definitions in WMS layer definition");
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to determine layer product type!", NULL);
  }
}

SharedWMSLayer WMSLayerFactory::createWMSLayer(const std::string& theFileName,
                                               const std::string& theNamespace,
                                               const std::string& theCustomer,
                                               const WMSConfig& theWMSConfig)
{
  try
  {
    std::string content;
    std::ifstream in(theFileName.c_str());

    if (!in)
      throw Spine::Exception(BCP, "Failed to open '" + theFileName + "' for reading!");

    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(content, root);
    if (!parsingSuccessful)
    {
      // report to the user the failure
      throw Spine::Exception(BCP, reader.getFormattedErrorMessages());
    }

    const bool use_wms = true;
    Spine::JSON::preprocess(
        root,
        theWMSConfig.itsDaliConfig.rootDirectory(use_wms),
        theWMSConfig.itsDaliConfig.rootDirectory(use_wms) + "/customers/" + theCustomer + "/layers",
        theWMSConfig.itsFileCache);
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
        layer = boost::make_shared<WMSPostGISLayer>(theWMSConfig.itsGisEngine, parsedLayer);
        break;
      }
      case WMSLayerType::QueryDataLayer:
      {
        // if no producer defined, let's use default producer
        if (producer.empty())
          producer = theWMSConfig.itsDaliConfig.defaultModel();
        layer = boost::make_shared<WMSQueryDataLayer>(theWMSConfig.itsQEngine, producer);
        break;
      }
      case WMSLayerType::ObservationLayer:
      {
#ifndef WITHOUT_OBSERVATION
        if (timestep.empty())
          timestep = "-1";
        // timestep -1 indicates that no timestep is given in product-file
        // in that case timestep is read from obsengine configuration file (default value is 1min)
        layer = boost::make_shared<WMSObservationLayer>(
            theWMSConfig.itsObsEngine, producer, std::stoi(timestep));
#endif
        break;
      }
      default:  // we should never en up here since determineLayerType should always return a valid
                // layer type
        throw Spine::Exception(BCP, "WMS Layer enum not handled in WMSLayerFactory.");
    };

    layer->updateLayerMetaData();

    // WMS GetCapability settings

    Json::Value nulljson;

    // for hiding the layer from GetCapabilities queries for example if the layer is not public
    auto json = root.get("hidden", nulljson);
    if (!json.isNull())
      layer->hidden = json.asBool();

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
        throw Spine::Exception(
            BCP, p.string() + " keyword value must be an array of strings or a string");
    }

    // Other variables

    layer->quiet = theWMSConfig.itsDaliConfig.quiet();
    layer->productFile = theFileName;
    layer->customer = theCustomer;

    // handle styles

    // 1) read "legend_url_layer"-parameter and use it to generate styles entry
    json = root.get("legend_url_layer", nulljson);
    if (!json.isNull())
    {
      std::string legendURLLayer = json.asString();
      layer->addStyle(legendURLLayer);
    }

    if (layer->styles.size() == 0)
    {
      // 2) read styles vector from configuration and generate layer styles
      json = root.get("styles", nulljson);
      if (!json.isNull())
      {
        if (!json.isArray())
          throw Spine::Exception(BCP, "WMSLayer style settings must be an array");

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
            json_value = legend_url_json.get("width", nulljson);
            if (!json_value.isNull())
              layerStyle.legend_url.width = json_value.asInt();
            json_value = legend_url_json.get("height", nulljson);
            if (!json_value.isNull())
              layerStyle.legend_url.height = json_value.asInt();
            json_value = legend_url_json.get("format", nulljson);
            if (!json_value.isNull())
              layerStyle.legend_url.format = json_value.asString();
            json_value = legend_url_json.get("online_resource", nulljson);
            if (!json_value.isNull())
              layerStyle.legend_url.online_resource = json_value.asString();
          }
          layer->styles.push_back(layerStyle);
        }
      }
    }

    // 3) generate layer styles automatically from configuration
    if (layer->styles.size() == 0)
      layer->addStyles(root, layer->name);

    // Spatial references supported by default, if applicable
    layer->crs = theWMSConfig.supportedWMSReferences();
    layer->crs_bbox = theWMSConfig.WMSBBoxes();

    // And extra references supported by the layer itself

    Json::Value projection = root["projection"];
    std::string mapcrs = projection["crs"].asString();
    if (mapcrs.substr(0, 5) == "EPSG:")  // accept only CRS from EPSG namespace
      layer->crs.insert(std::make_pair(mapcrs, mapcrs));

    return layer;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to create WMS layer!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
