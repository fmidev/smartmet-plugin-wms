#include "WMSLayerFactory.h"
#include "JsonTools.h"
#include "StyleSelection.h"
#include "WMSGridDataLayer.h"
#include "WMSNonTemporalLayer.h"
#include "WMSPostGISLayer.h"
#include "WMSQueryDataLayer.h"
#ifndef WITHOUT_OBSERVATION
#include "WMSObservationLayer.h"
#endif
#include <engines/querydata/Engine.h>
#include <fmt/format.h>
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>
#include <spine/Json.h>
#include <set>

using namespace SmartMet::Plugin::Dali::JsonTools;

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
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

std::set<std::string> querydata_layers = {"arrow",
                                          "isoband",
                                          "isoline",
                                          "isolabel",
                                          "null",
                                          "number",
                                          "symbol",
                                          "tag",
                                          "location",
                                          "stream"};

std::set<std::string> postgis_layers = {"postgis", "icemap"};

WMSLayerType get_wms_layer_type(const Json::Value& layer)
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
      return WMSLayerType::MapLayer;

    if (postgis_layers.find(type_name) != postgis_layers.end())
      return WMSLayerType::PostGISLayer;

    if (querydata_layers.find(type_name) != querydata_layers.end())
      return WMSLayerType::QueryDataLayer;

    // No identified special layers, this is not a WMSLayer;
    return WMSLayerType::NotWMSLayer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to get WMS layer type!");
  }
}

WMSLayerType find_layer_type(const Json::Value& layers, Json::Value& layerRef)
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
        layer_type = find_layer_type(sublayers, layerRef);
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

    layer_type = get_wms_layer_type(layer);

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

// This function deduces the WMS product type. In case of several layer definitions in the product
// the most significant is chosen
WMSLayerType determine_product_type(const WMSConfig& wmsconfig,
                                    const std::string& producer,
                                    const Json::Value& root,
                                    Json::Value& layerRef)
{
  try
  {
#ifndef WITHOUT_OBSERVATION
    const auto obsProducers = wmsconfig.getObservationProducers();
    if (obsProducers.find(producer) != obsProducers.end())
      return WMSLayerType::ObservationLayer;
#endif

    Json::Value nulljson;
    Json::Value views = root.get("views", nulljson);

    // browse layers and if 'postgis'-layer is found type is WMSLayerType::PostGISLayer
    // otherwise it is WMSLayerType::QueryDataLayer
    if (views.isNull())
      throw Fmi::Exception(BCP, "No views in WMS layer definition");

    const Json::Value& view = views[0];

    Json::Value layers = view.get("layers", nulljson);
    if (layers.isNull())
      throw Fmi::Exception(BCP, "No layers in WMS layer definition");

    WMSLayerType layer_type = find_layer_type(layers, layerRef);  // The resolved layer type

    if (layer_type != WMSLayerType::NotWMSLayer)
      return layer_type;

    Json::Value hidden = root.get("hidden", nulljson);
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

std::vector<interval_dimension_item> extract_intervals(Json::Value& root)
{
  try
  {
    std::optional<int> interval_default;
    remove_int(interval_default, root, "interval");

    auto json = remove(root, "intervals");
    std::vector<interval_dimension_item> intervals;

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
              (interval_default ? (*interval_default == interval_start) : i == 0)));
        }
        else if (json[i].isObject())
        {
          std::optional<int> interval_start;
          std::optional<int> interval_end;
          bool interval_default = false;
          remove_int(interval_start, json[i], "interval_start");
          remove_int(interval_end, json[i], "interval_end");
          remove_bool(interval_default, json[i], "default");

          if (interval_start || interval_end)
            intervals.emplace_back(interval_dimension_item(interval_start ? *interval_start : 0,
                                                           interval_end ? *interval_end : 0,
                                                           interval_default));
        }
      }
    }
    else
    {
      std::optional<int> interval_start;
      std::optional<int> interval_end;
      remove_int(interval_start, root, "interval_start");
      remove_int(interval_end, root, "interval_end");
      if (interval_start || interval_end)
        intervals.emplace_back(interval_dimension_item(
            interval_start ? *interval_start : 0, interval_end ? *interval_end : 0, true));
    }
    return intervals;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract layer interval settings!");
  }
}

void extract_crs(Json::Value& root,
                 std::set<std::string>& enabled_refs,
                 std::set<std::string>& disabled_refs)
{
  try
  {
    auto json = remove(root, "crs");
    if (!json.isNull())
    {
      if (!json.isObject())
        throw Fmi::Exception(BCP, "top level crs setting must be a group");

      auto j = remove(json, "enable");
      if (!j.isNull())
        extract_set("crs.enable", enabled_refs, j);

      j = remove(json, "disable");
      if (!j.isNull())
        extract_set("crs.disable", disabled_refs, j);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract CRS settings!");
  }
}

void extract_keyword(Json::Value& root, std::optional<std::set<std::string>>& keywords)
{
  try
  {
    auto json = remove(root, "keyword");
    if (!json.isNull())
    {
      keywords = std::set<std::string>();
      if (json.isString())
        keywords->insert(json.asString());
      else if (json.isArray())
      {
        for (auto& j : json)
          keywords->insert(j.asString());
      }
      else
        throw Fmi::Exception(BCP, "Keyword value must be a string or an array of strings");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to extract keyword settings!");
  }
}

// Read producer info from
//  1) layer
//  2) view
//  3) product
//  4) config file

std::string determine_producer(const WMSConfig& theWMSConfig, Json::Value& root)
{
  try
  {
    std::string producer;

    // From product
    remove_string(producer, root, "producer");

    // Iterate views and layers
    Json::Value nulljson;
    auto views = root.get("views", nulljson);
    if (!views.isNull())
    {
      // Iterate views
      for (unsigned int i = 0; i < views.size(); i++)
      {
        auto view = views[i];
        remove_string(producer, view, "producer");

        auto layers = view.get("layers", nulljson);
        if (!layers.isNull())
        {
          // Iterate layers
          for (unsigned int k = 0; k < layers.size(); k++)
          {
            auto layer = layers[k];
            remove_string(producer, layer, "producer");
            if (!producer.empty())
              return producer;
          }
        }
      }
    }

    if (!producer.empty())
      return producer;

    // If no producer defined, let's use default producer
    return theWMSConfig.getDaliConfig().defaultModel();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to determine producer!");
  }
}

std::optional<std::string> determine_source(Json::Value& root)
{
  try
  {
    std::optional<std::string> source;

    // From product
    remove_string(source, root, "source");

    // Iterate views and layers
    Json::Value nulljson;
    auto views = root.get("views", nulljson);
    if (!views.isNull())
    {
      // Iterate views
      for (unsigned int i = 0; i < views.size(); i++)
      {
        auto view = views[i];
        remove_string(source, view, "source");

        auto layers = view.get("layers", nulljson);
        if (!layers.isNull())
        {
          // Iterate layers
          for (unsigned int k = 0; k < layers.size(); k++)
          {
            auto layer = layers[k];
            remove_string(source, layer, "source");
            if (source)
              return source;
          }
        }
      }
    }

    return source;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to determine producer!");
  }
}



std::string determine_elevation_unit(Json::Value& root)
{
  try
  {
    std::string elevation_unit;

    // From product
    remove_string(elevation_unit, root, "elevation_unit");

    // Iterate views and layers
    Json::Value nulljson;
    auto views = root.get("views", nulljson);
    if (!views.isNull())
    {
      // Iterate views
      for (unsigned int i = 0; i < views.size(); i++)
      {
        auto view = views[i];
        remove_string(elevation_unit, view, "elevation_unit");

        auto layers = view.get("layers", nulljson);
        if (!layers.isNull())
        {
          // Iterate layers
          for (unsigned int k = 0; k < layers.size(); k++)
          {
            auto layer = layers[k];
            remove_string(elevation_unit, layer, "elevation_unit");
            if (!elevation_unit.empty())
              return elevation_unit;
          }
        }
      }
    }

    return elevation_unit;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to determine producer!");
  }
}


std::string determine_parameter(Json::Value& root)
{
  try
  {
    std::string parameter;

    // From product
    remove_string(parameter, root, "parameter");

    // Iterate views and layers
    Json::Value nulljson;
    auto views = root.get("views", nulljson);
    if (!views.isNull())
    {
      // Iterate views
      for (unsigned int i = 0; i < views.size(); i++)
      {
        auto view = views[i];
        remove_string(parameter, view, "parameter");

        auto layers = view.get("layers", nulljson);
        if (!layers.isNull())
        {
          // Iterate layers
          for (unsigned int k = 0; k < layers.size(); k++)
          {
            auto layer = layers[k];
            remove_string(parameter, layer, "parameter");
            if (!parameter.empty())
              return parameter;
          }
        }
      }
    }

    return parameter;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to determine producer!");
  }
}


uint determine_geometryId(Json::Value& root)
{
  try
  {
    uint geometryId = 0;

    // From product
    remove_uint(geometryId, root, "geometryId");

    // Iterate views and layers
    Json::Value nulljson;
    auto views = root.get("views", nulljson);
    if (!views.isNull())
    {
      // Iterate views
      for (unsigned int i = 0; i < views.size(); i++)
      {
        auto view = views[i];
        remove_uint(geometryId, view, "geometryId");

        auto layers = view.get("layers", nulljson);
        if (!layers.isNull())
        {
          // Iterate layers
          for (unsigned int k = 0; k < layers.size(); k++)
          {
            auto layer = layers[k];
            remove_uint(geometryId, layer, "geometryId");
            if (geometryId != 0)
              return geometryId;
          }
        }
      }
    }

    return geometryId;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to determine geometryId!");
  }
}

SharedWMSLayer create_wms_layer(const WMSConfig& theWMSConfig, Json::Value& root)
{
  try
  {
    auto producer = determine_producer(theWMSConfig, root);
    uint geometryId = determine_geometryId(root);
    std::optional<std::string> source = determine_source(root);

    Json::Value parsedLayer;
    WMSLayerType layerType = determine_product_type(theWMSConfig, producer, root, parsedLayer);

    if (source == std::string("grid"))
    {
      layerType = WMSLayerType::GridDataLayer;

      std::vector<std::string> partList;
      splitString(producer, ':', partList);
      if (partList.size() == 2)
      {
        producer = partList[0];
        geometryId = toUInt32(partList[1]);
      }
    }

    SharedWMSLayer layer;

    switch (layerType)
    {
      case WMSLayerType::MapLayer:
      {
        layer = std::make_shared<WMSMapLayer>(theWMSConfig, parsedLayer);
        break;
      }
      case WMSLayerType::PostGISLayer:
      {
        layer = std::make_shared<WMSPostGISLayer>(theWMSConfig, parsedLayer);
        break;
      }
      case WMSLayerType::QueryDataLayer:
      {
        layer = std::make_shared<WMSQueryDataLayer>(theWMSConfig, producer);
        break;
      }
      case WMSLayerType::GridDataLayer:
      {
        auto parameter = determine_parameter(root);
        auto elevation_unit = determine_elevation_unit(root);
        layer = std::make_shared<WMSGridDataLayer>(theWMSConfig, producer, parameter, geometryId, elevation_unit);
        break;
      }
      case WMSLayerType::ObservationLayer:
      {
#ifndef WITHOUT_OBSERVATION
        std::string timestep;
        remove_string(timestep, root, "timestep");
        if (timestep.empty())
          timestep = "-1";
        // timestep -1 indicates that no timestep is given in product-file
        // in that case timestep is read from obsengine configuration file (default value is 1min)
        layer =
            std::make_shared<WMSObservationLayer>(theWMSConfig, producer, std::stoi(timestep));
#endif
        break;
      }
      default:  // we should never end up here
        throw Fmi::Exception(BCP, "WMS Layer enum not handled in WMSLayerFactory.");
    }

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to create WMS layer!");
  }
}

}  // anonymous namespace

SharedWMSLayer WMSLayerFactory::createWMSLayer(Json::Value& root,
                                               const std::string& theFileName,
                                               const std::string& theFullLayerName,
                                               const std::string& theNamespace,
                                               const std::string& theCustomer,
                                               const WMSConfig& theWMSConfig)
{
  try
  {
    SharedWMSLayer layer = create_wms_layer(theWMSConfig, root);

    // Generic variables.

    // Note how this is given externally. It may be filename based or generated
    // from JSON settings.
    layer->name = theFullLayerName;

    // The quiet flag must be set before updateLayerMetaData is called.
    layer->quiet = theWMSConfig.getDaliConfig().quiet();

    layer->setProductFile(theFileName);
    layer->setCustomer(theCustomer);

    std::vector<interval_dimension_item> intervals = extract_intervals(root);

    // WMS GetCapability settings

    Json::Value nulljson;

    // for hiding the layer from GetCapabilities queries for example if the layer is not public
    remove_bool(layer->hidden, root, "hidden");

    remove_bool(layer->timeDimensionDisabled, root, "disable_wms_time_dimension");
    remove_string(layer->name, root, "name");
    remove_int(layer->opaque, root, "opaque");
    remove_int(layer->queryable, root, "queryable");
    remove_int(layer->cascaded, root, "cascaded");
    remove_int(layer->no_subsets, root, "no_subsets");
    remove_int(layer->fixed_width, root, "fixed_width");
    remove_int(layer->fixed_height, root, "fixed_height");

    auto json = remove(root, "title");
    if (!json.isNull())
    {
      layer->title = Dali::Text("WMS layer title");
      layer->title->init(json, theWMSConfig.itsDaliConfig);
    }

    json = remove(root, "abstract");
    if (!json.isNull())
    {
      layer->abstract = Dali::Text("WMS layer abstract");
      layer->abstract->init(json, theWMSConfig.itsDaliConfig);
    }

    extract_keyword(root, layer->keywords);

    extract_crs(root, layer->enabled_refs, layer->disabled_refs);

    // Update metadata from DB etc

    if (!layer->updateLayerMetaData())
      return {};

    // Spatial references supported by default, if applicable
    layer->refs = theWMSConfig.supportedWMSReferences();

    // And extra references supported by the layer itself

    auto projection = remove(root, "projection");

    std::string mapcrs;
    remove_string(mapcrs, projection, "crs");

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

std::list<SharedWMSLayer> WMSLayerFactory::createWMSLayers(const std::string& theFileName,
                                                           const std::string& theFullLayerName,
                                                           const std::string& theNamespace,
                                                           const std::string& theCustomer,
                                                           const WMSConfig& theWMSConfig)
{
  try
  {
    std::list<SharedWMSLayer> ret;

    // First process the JSON

    Json::Value root = theWMSConfig.getJsonCache().get(theFileName);

    const bool use_wms = true;
    const auto root_dir = theWMSConfig.getDaliConfig().rootDirectory(use_wms);
    const auto customer_layers_dir = root_dir + "/customers/" + theCustomer + "/layers";

    Spine::JSON::preprocess(root, root_dir, customer_layers_dir, theWMSConfig.itsJsonCache);
    Spine::JSON::dereference(root);

    // If there are no variants, process the layer as is

    auto variants_j = remove(root, "variants");

    if (variants_j.isNull())
    {
      auto layer = createWMSLayer(
          root, theFileName, theFullLayerName, theNamespace, theCustomer, theWMSConfig);
      if (layer)
        ret.emplace_back(layer);
      return ret;
    }

    // Process variants as if the settings were overridden by a query string.
    // Variants must define atleast name and title, all other settings are optional.

    if (!variants_j.isArray())
      throw Fmi::Exception(
          BCP, "WMS layer " + theFullLayerName + " variants setting must be a JSON array");

    for (auto& settings_j : variants_j)
    {
      if (!settings_j.isObject())
        throw Fmi::Exception(BCP,
                             "WMS layer " + theFullLayerName +
                                 " variants setting must be a JSON array containing JSON objects");

      Json::Value variant_j = root;  // deep copy for applying the changes

      // Obligatory settings
      std::string name;
      std::string title;
      remove_string(name, settings_j, "name");
      remove_string(title, settings_j, "title");

      if (name.empty())
        throw Fmi::Exception(BCP, "WMS layer " + theFullLayerName + " variants must all be named");
      if (title.empty())
        throw Fmi::Exception(BCP,
                             "WMS layer " + theFullLayerName + " variants must all have titles");

      variant_j["title"] = title;

      // Process optional settings (abstract etc)

      std::map<std::string, Json::Value> substitutions;
      const auto members = settings_j.getMemberNames();
      for (const auto& name : members)
        substitutions.insert({name, settings_j[name]});

      SmartMet::Spine::JSON::expand(variant_j, substitutions);

      // Note: using variant name instead of theFullLayerName
      auto layer =
          createWMSLayer(variant_j, theFileName, name, theNamespace, theCustomer, theWMSConfig);
      if (layer)
        ret.emplace_back(layer);
    }

    return ret;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to create WMS layers!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
