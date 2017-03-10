#include "WMSLayerFactory.h"

#include "WMSQueryDataLayer.h"
#include "WMSPostGISLayer.h"

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

#ifndef WITHOUT_OBSERVATION
static std::set<std::string> observation_layers = {"windrose"};
#endif

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

#ifndef WITHOUT_OBSERVATION
// TODO!!! Handle ObservationLayer
#endif

    // No identified special layers, this is not a WMSLayer;
    return WMSLayerType::NotWMSLayer;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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

        // If we are here, the most significant encountered layer is a NotWMSLayer.
        // This is currently an error, WMS layer must contain some spatio-temporal data

        // Misconfigured layer is this
        throw Spine::Exception(BCP, "No valid Dali layer definitions in WMS layer definition");
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    WMSLayerType layerType = determineProductType(root, parsedLayer);

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
        // producer given in root level
        std::string producer = root["producer"].asString();
        // if no producer defined, let's use default producer
        if (producer.empty())
          producer = theWMSConfig.itsDaliConfig.defaultModel();

        layer = boost::make_shared<WMSQueryDataLayer>(theWMSConfig.itsQEngine, producer);
        break;
      }

#ifndef WITHOUT_OBSERVATION
// TODO!!! Handle ObservationLayer
#endif

      default:  // we should never en up here since determineLayerType should always return a valid
                // layer type
        throw Spine::Exception(BCP, "WMS Layer enum not handled in WMSLayerFactory.");
    };

    layer->updateLayerMetaData();

    layer->queryable = false;
    layer->quiet = theWMSConfig.itsDaliConfig.quiet();
    layer->name = theNamespace + ":" + p.stem().string();
    layer->title = root["title"].asString();
    ;
    layer->abstract = root["abstract"].asString();
    layer->productFile = theFileName;
    layer->customer = theCustomer;

    // always supported CRS
    layer->crs.insert("EPSG:2393");  // YKJ
    layer->crs.insert("EPSG:3035");  // ETRS89 / ETRS-LAEA
    layer->crs.insert("EPSG:3047");  // ETRS-TM35 (Recommended for Finland)
    layer->crs.insert("EPSG:3067");  // ETRS-TM35FIN
    layer->crs.insert("EPSG:3857");  // Web-Mercator
    layer->crs.insert("EPSG:4258");  // ETRS89 (for Europe instead of WGS84)
    layer->crs.insert("EPSG:4326");  // WGS84

    Json::Value projection = root["projection"];
    std::string mapcrs = projection["crs"].asString();
    if (mapcrs.substr(0, 5) == "EPSG:")  // accept only CRS from EPSG namespace
      layer->crs.insert(mapcrs);

    return layer;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
