#include "WMSLayerFactory.h"
#include "StyleSelection.h"
#include "WMSGridDataLayer.h"
#include "WMSNonTemporalLayer.h"
#include "WMSPostGISLayer.h"
#include "WMSQueryDataLayer.h"
#ifndef WITHOUT_OBSERVATION
#include "WMSObservationLayer.h"
#endif
#include <engines/querydata/Engine.h>
#include <grid-files/common/GeneralFunctions.h>
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
  GridDataLayer,
  MapLayer,
  NotWMSLayer
};

static std::set<std::string> querydata_layers = {
    "arrow", "isoband", "isoline", "isolabel", "number", "symbol", "tag", "location"};

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

    auto type_name = layer_type.asString();

    if (type_name == "map")
    {
      return WMSLayerType::MapLayer;
    }

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

#if 0
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
      case WMSLayerType::MapLayer:
        ret = "MapLayer";
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
#endif

WMSLayerType findLayerType(const Json::Value& layers, Json::Value& layerRef)
{
  WMSLayerType layer_type = WMSLayerType::NotWMSLayer;
  Json::Value nulljson;

  Json::Value mapLayers = Json::Value(Json::arrayValue);
  for (const auto& layer : layers)
  {
    if (!layer.isNull())
    {
      Json::Value sublayers = layer.get("layers", nulljson);
      // if sublayers exists examine them
      if (!sublayers.isNull())
      {
        layer_type = findLayerType(sublayers, layerRef);
        if (layer_type == WMSLayerType::MapLayer)
          continue;
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

    layer_type = getWMSLayerType(layer);

    if (layer_type == WMSLayerType::NotWMSLayer)
    {
      // NotWMSLayer means we should proceed to find a more significant layer
      continue;
    }
    if (layer_type == WMSLayerType::MapLayer)
    {
      // Map layer may be part of other layer
      mapLayers.append(layer);
      layerRef = mapLayers;
      continue;
    }

    layerRef = layer;
    return layer_type;
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
      throw Fmi::Exception(BCP, "No views in WMS layer definition");

    const Json::Value& view = views[0];

    Json::Value layers = view.get("layers", nulljson);
    if (layers.isNull())
      throw Fmi::Exception(BCP, "No layers in WMS layer definition");

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
    Json::Value root = theWMSConfig.getJsonCache().get(theFileName);

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

    Json::Value nulljson;
    boost::optional<std::string> source;
    uint geometryId = 0;
    std::string parameter;

    auto json = root.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = root.get("source", nulljson);
    if (!json.isNull())
    {
      source = json.asString();
      if (*source == "grid")
      {
        layerType = WMSLayerType::GridDataLayer;

        std::vector<std::string> partList;
        splitString(producer, ':', partList);
        if (partList.size() == 2)
        {
          producer = partList[0];
          geometryId = toUInt32(partList[1]);
        }

        json = root.get("geometryId", nulljson);
        if (!json.isNull())
          geometryId = json.asInt();
      }
    }

    std::vector<interval_dimension_item> intervals;
    boost::optional<int> interval_default;
    json = root.get("interval", nulljson);
    if (!json.isNull())
      interval_default = json.asInt();
    json = root.get("intervals", nulljson);
    if (!json.isNull())
    {
      if (!json.isArray())
        throw Fmi::Exception(BCP, "WMSLayer intervals settings must be an array");

      for (unsigned int i = 0; i < json.size(); i++)
      {
        if (json[i].isInt())
        {
          int interval_start = json[i].asInt();
          intervals.emplace_back(interval_dimension_item(
              interval_start,
              0,
              (interval_default ? (*interval_default == interval_start ? true : false) : i == 0)));
        }
        else if (json[i].isObject())
        {
          boost::optional<int> interval_start;
          boost::optional<int> interval_end;
          bool interval_default = false;
          auto json_value = json[i].get("interval_start", nulljson);
          if (!json_value.isNull())
            interval_start = json_value.asInt();
          json_value = json[i].get("interval_end", nulljson);
          if (!json_value.isNull())
            interval_end = json_value.asInt();
          json_value = json[i].get("default", nulljson);
          if (!json_value.isNull())
            interval_default = json_value.asBool();

          if (interval_start || interval_end)
            intervals.emplace_back(interval_dimension_item(interval_start ? *interval_start : 0,
                                                           interval_end ? *interval_end : 0,
                                                           interval_default));
        }
      }
    }
    else
    {
      boost::optional<int> interval_start;
      boost::optional<int> interval_end;
      json = root.get("interval_start", nulljson);
      if (!json.isNull())
        interval_start = json.asInt();
      json = root.get("interval_end", nulljson);
      if (!json.isNull())
        interval_end = json.asInt();
      if (interval_start || interval_end)
        intervals.emplace_back(interval_dimension_item(
            interval_start ? *interval_start : 0, interval_end ? *interval_end : 0, true));
    }

    boost::filesystem::path p(theFileName);

    switch (layerType)
    {
      case WMSLayerType::MapLayer:
      {
        layer = boost::make_shared<WMSMapLayer>(theWMSConfig, parsedLayer);
        break;
      }
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
      case WMSLayerType::GridDataLayer:
      {
        // if no producer defined, let's use default producer
        if (producer.empty())
          producer = theWMSConfig.itsDaliConfig.defaultModel();
        layer = boost::make_shared<WMSGridDataLayer>(theWMSConfig, producer, parameter, geometryId);
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

    // Generic variables. The quiet flag must be set before updateLayerMetaData is called.

    layer->quiet = theWMSConfig.itsDaliConfig.quiet();
    layer->setProductFile(theFileName);
    layer->setCustomer(theCustomer);

    // WMS GetCapability settings

    // for hiding the layer from GetCapabilities queries for example if the layer is not public
    json = root.get("hidden", nulljson);
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
        for (const auto& j : json)
          layer->keywords->insert(j.asString());
      }
      else
        throw Fmi::Exception(BCP,
                             p.string() + " keyword value must be an array of strings or a string");
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

    if (!layer->updateLayerMetaData())
      return {};

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

    // Calculate LegedGraphicInfo only once
    layer->initLegendGraphicInfo(root);

    // Add interval dimension
    for (const auto& item : intervals)
      layer->addIntervalDimension(item.interval_start, item.interval_end, item.interval_default);

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
