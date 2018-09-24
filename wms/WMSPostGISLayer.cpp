#include "WMSPostGISLayer.h"
#include "WMSConfig.h"
#include <boost/move/make_unique.hpp>
#include <gis/Host.h>
#include <gis/OGR.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
PostGISMetaDataSettings get_postgis_metadata_settings(const libconfig::Config& config,
                                                      const std::string& pgname,
                                                      const std::string& schema)
{
  PostGISMetaDataSettings ret;

  // first read the common update interval
  config.lookupValue("wms.get_capabilities.update_interval", ret.update_interval);

  if (!config.exists("wms.get_capabilities.postgis_layer"))
    return ret;

  const auto& postgis_layer_settings = config.lookup("wms.get_capabilities.postgis_layer");

  // in old version  postgis_layer-setting was not a list, so lets accept it also for backward
  // compatibility
  if (!postgis_layer_settings.isList())
  {
    postgis_layer_settings.lookupValue("update_interval", ret.update_interval);
    postgis_layer_settings.lookupValue("pgname", ret.pgname);
    postgis_layer_settings.lookupValue("schema", ret.schema);
    postgis_layer_settings.lookupValue("table", ret.table);
    postgis_layer_settings.lookupValue("field", ret.field);
    postgis_layer_settings.lookupValue("where", ret.where);
    // match found -> return
    if (ret.pgname == pgname && ret.schema == schema && !ret.table.empty() && !ret.field.empty() &&
        !ret.where.empty())
      ret.found = true;

    return ret;
  }

  for (int i = 0; i < postgis_layer_settings.getLength(); i++)
  {
    ret.pgname.clear();
    ret.schema.clear();
    ret.table.clear();
    ret.field.clear();
    ret.where.clear();

    const auto& group = postgis_layer_settings[i];

    if (!group.isGroup())
      throw Spine::Exception(BCP, "wms.get_capabilities.postgis_layer must be a list of groups");

    // layer specific interval
    group.lookupValue("update_interval", ret.update_interval);
    group.lookupValue("pgname", ret.pgname);
    group.lookupValue("schema", ret.schema);
    group.lookupValue("table", ret.table);
    group.lookupValue("field", ret.field);
    group.lookupValue("where", ret.where);

    // if match found return
    if (ret.pgname == pgname && ret.schema == schema && !ret.table.empty() && !ret.field.empty() &&
        !ret.where.empty())
    {
      ret.found = true;
      break;
    }
  }

  return ret;
}
}  // anonymous namespace

WMSPostGISLayer::WMSPostGISLayer(const WMSConfig& config, const Json::Value& json)
    : WMSLayer(config), itsGisEngine(config.gisEngine()), hasTemporalDimension(false)
{
  try
  {
    if (json["pgname"].isNull())
      throw Spine::Exception(BCP, "'pgname' must be defined for postgis layer");
    if (json["schema"].isNull())
      throw Spine::Exception(BCP, "'schema' must be defined for postgis layer");
    if (json["table"].isNull())
      throw Spine::Exception(BCP, "'table' must be defined for postgis layer");
    if (json["geometry_column"].isNull())
      throw Spine::Exception(BCP, "'geometry_column' must be defined for postgis layer");

    mdq_options.pgname = json["pgname"].asString();
    mdq_options.schema = json["schema"].asString();
    mdq_options.table = json["table"].asString();

    if (!json["time_column"].isNull())
    {
      mdq_options.time_column = json["time_column"].asString();  // May be empty for map layers
      hasTemporalDimension = true;
    }

    mdq_options.geometry_column = json["geometry_column"].asString();

    itsMetaDataSettings = get_postgis_metadata_settings(
        wmsConfig.getDaliConfig().getConfig(), mdq_options.pgname, mdq_options.schema);

    metadataUpdateInterval = itsMetaDataSettings.update_interval;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to initializa PostGIS layer!");
  }
}

bool WMSPostGISLayer::mustUpdateLayerMetaData()
{
  try
  {
    if (timeDimensionDisabled)
      return false;

    if (!itsMetaDataSettings.found)
      return true;

    Engine::Gis::MapOptions moptions;
    moptions.pgname = itsMetaDataSettings.pgname;
    moptions.schema = itsMetaDataSettings.schema;
    moptions.table = itsMetaDataSettings.table;
    moptions.fieldnames.insert(itsMetaDataSettings.field);
    moptions.where = itsMetaDataSettings.where;

    OGRSpatialReference* sr = nullptr;
    boost::posix_time::ptime mostCurrentTimestamp = mostCurrentTime();

    // fetch the latest publicationdate of icemap
    Fmi::Features features = itsGisEngine->getFeatures(sr, moptions);
    if (!features.empty())
    {
      Fmi::FeaturePtr feature = features[0];
      boost::posix_time::ptime timestamp =
          boost::get<boost::posix_time::ptime>(feature->attributes[itsMetaDataSettings.field]);
      if (timestamp.is_not_a_date_time() || mostCurrentTimestamp.is_not_a_date_time())
        return true;
      // if the timestamp is older or the same as previous time, there is no need to update metadata
      if (timestamp <= mostCurrentTimestamp)
        return false;
    }

    return true;
  }
  catch (...)
  {
    throw Spine::Exception(
        BCP, "WMSPostGISLayer::mustUpdateLayerMetaData() function call failed!", nullptr);
  }
}

void WMSPostGISLayer::updateLayerMetaData()
{
  try
  {
    Engine::Gis::MetaData metadata;

    try
    {
      // If timeDimensionDisabled dont fetch timesteps from database
      if (timeDimensionDisabled)
        mdq_options.time_column.reset();

      metadata = itsGisEngine->getMetaData(mdq_options);
    }
    catch (...)
    {
      if (!quiet)
      {
        Spine::Exception exception(BCP, "WMS layer metadata update failure!", nullptr);
        exception.addParameter("Layer name", name);
        exception.printError();
      }
      return;
    }

    // bounding box
    geographicBoundingBox.xMin = metadata.xmin;
    geographicBoundingBox.yMin = metadata.ymin;
    geographicBoundingBox.xMax = metadata.xmax;
    geographicBoundingBox.yMax = metadata.ymax;

    if (hasTemporalDimension && !timeDimensionDisabled)
    {
      if (even_timesteps(metadata.timesteps))
      {
        boost::posix_time::ptime first_time(metadata.timesteps[0]);
        boost::posix_time::ptime last_time(metadata.timesteps[metadata.timesteps.size() - 1]);
        boost::posix_time::time_duration resolution =
            (metadata.timesteps[1] - metadata.timesteps[0]);

        auto newTimeDimension =
            boost::movelib::make_unique<IntervalTimeDimension>(first_time, last_time, resolution);
        timeDimension = std::move(newTimeDimension);
      }
      else
      {
        auto newTimeDimension = boost::movelib::make_unique<StepTimeDimension>();
        for (const auto& tim : metadata.timesteps)
          newTimeDimension->addTimestep(tim);
        timeDimension = std::move(newTimeDimension);
      }
    }
    else
      timeDimension = nullptr;

    metadataTimestamp = boost::posix_time::second_clock::universal_time();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to update PostGIS layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
