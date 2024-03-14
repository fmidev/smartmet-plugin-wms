#include "WMSPostGISLayer.h"
#include "JsonTools.h"
#include "WMSConfig.h"
#include <boost/move/make_unique.hpp>
#include <gis/Host.h>
#include <gis/OGR.h>
#include <macgyver/Exception.h>

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
      throw Fmi::Exception(BCP, "wms.get_capabilities.postgis_layer must be a list of groups");

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

WMSPostGISLayer::WMSPostGISLayer(const WMSConfig& config, Json::Value& json)
    : WMSLayer(config), itsGisEngine(config.gisEngine()), hasTemporalDimension(false)
{
  try
  {
    using namespace Dali::JsonTools;

    remove_string(mdq_options.pgname, json, "pgname");
    remove_string(mdq_options.schema, json, "schema");
    remove_string(mdq_options.table, json, "table");
    remove_string(mdq_options.geometry_column, json, "geometry_column");

    if (mdq_options.pgname.empty())
      throw Fmi::Exception(BCP, "'pgname' must be defined for postgis layer");
    if (mdq_options.schema.empty())
      throw Fmi::Exception(BCP, "'schema' must be defined for postgis layer");
    if (mdq_options.table.empty())
      throw Fmi::Exception(BCP, "'table' must be defined for postgis layer");
    if (mdq_options.geometry_column.empty())
      throw Fmi::Exception(BCP, "'geometry_column' must be defined for postgis layer");

    hasTemporalDimension = json.isMember("time_column");
    remove_string(mdq_options.time_column, json, "time_column");  // may be empty for map layers

    itsMetaDataSettings = get_postgis_metadata_settings(
        wmsConfig.getDaliConfig().getConfig(), mdq_options.pgname, mdq_options.schema);

    metadataUpdateInterval = itsMetaDataSettings.update_interval;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to initialize PostGIS layer!");
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

    const boost::optional<Fmi::DateTime> reference_time;
    Fmi::DateTime mostCurrentTimestamp = mostCurrentTime(reference_time);

    // fetch the latest publicationdate of icemap
    Fmi::Features features = itsGisEngine->getFeatures(moptions);
    if (!features.empty())
    {
      Fmi::FeaturePtr feature = features[0];
      Fmi::DateTime timestamp =
          boost::get<Fmi::DateTime>(feature->attributes[itsMetaDataSettings.field]);
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
    throw Fmi::Exception(
        BCP, "WMSPostGISLayer::mustUpdateLayerMetaData() function call failed!", nullptr);
  }
}

bool WMSPostGISLayer::updateLayerMetaData()
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
        Fmi::Exception exception(BCP, "WMS layer metadata update failure!", nullptr);
        exception.addParameter("Layer name", name);
        exception.printError();
      }
      return false;
    }

    // bounding box
    geographicBoundingBox.xMin = metadata.xmin;
    geographicBoundingBox.yMin = metadata.ymin;
    geographicBoundingBox.xMax = metadata.xmax;
    geographicBoundingBox.yMax = metadata.ymax;

    if (hasTemporalDimension && !timeDimensionDisabled)
    {
      std::map<Fmi::DateTime, boost::shared_ptr<WMSTimeDimension>> newTimeDimensions;
      boost::shared_ptr<WMSTimeDimension> timeDimension = nullptr;
      if (metadata.timeinterval)
      {
        tag_interval interval(metadata.timeinterval->starttime,
                              metadata.timeinterval->endtime,
                              metadata.timeinterval->timestep);
        time_intervals timeintervals{interval};
        timeDimension = boost::make_shared<IntervalTimeDimension>(timeintervals);
      }
      else
      {
        time_intervals intervals = get_intervals(metadata.timesteps);
        if (!intervals.empty())
        {
          timeDimension = boost::make_shared<IntervalTimeDimension>(intervals);
        }
        else
        {
          timeDimension = boost::make_shared<StepTimeDimension>(metadata.timesteps);
        }
      }
      Fmi::DateTime origintime(Fmi::DateTime::NOT_A_DATE_TIME);
      newTimeDimensions.insert(std::make_pair(origintime, timeDimension));
      timeDimensions = boost::make_shared<WMSTimeDimensions>(newTimeDimensions);
    }
    else
    {
      timeDimensions = nullptr;
    }

    metadataTimestamp = Fmi::SecondClock::universal_time();

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update PostGIS layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
