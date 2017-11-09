#include "WMSPostGISLayer.h"
#include "WMSConfig.h"
#include <spine/Exception.h>

#include <gis/Host.h>
#include <gis/OGR.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
WMSPostGISLayer::WMSPostGISLayer(const WMSConfig& config, const Json::Value& json)
    : WMSLayer(config), itsGisEngine(config.gisEngine()), hasTemporalDimension(false)
{
  try
  {
    wmsConfig.getLibconfig().lookupValue("wms.get_capabilities.postgis_layer.update_interval",
                                         metadataUpdateInterval);

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
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to initializa PostGIS layer!", NULL);
  }
}

bool WMSPostGISLayer::mustUpdateLayerMetaData()
{
  try
  {
    const libconfig::Config& config = wmsConfig.getLibconfig();

    // if 'wms.postgis_metadata_update' section does not exist -> update metadata every time
    if (!config.exists("wms.get_capabilities.postgis_layer"))
      return true;

    std::string pgname, schema, table, field, where;
    config.lookupValue("wms.get_capabilities.postgis_layer.pgname", pgname);
    config.lookupValue("wms.get_capabilities.postgis_layer.schema", schema);
    config.lookupValue("wms.get_capabilities.postgis_layer.table", table);
    config.lookupValue("wms.get_capabilities.postgis_layer.field", field);
    config.lookupValue("wms.get_capabilities.postgis_layer.where", where);

    // if 'pgname' and 'schema' does not mach or fields are empty -> dont contimue
    if (mdq_options.pgname != pgname || mdq_options.schema != schema || table.empty() ||
        field.empty() || where.empty())
      return true;

    Engine::Gis::MapOptions moptions;
    moptions.pgname = mdq_options.pgname;
    moptions.schema = mdq_options.schema;
    moptions.table = table;
    moptions.fieldnames.insert(field);
    moptions.where = where;
    OGRSpatialReference* sr = nullptr;

    boost::posix_time::ptime mostCurrentTimestamp = mostCurrentTime();
    // fetch the latest publicationdate of icemap
    Fmi::Features features = itsGisEngine->getFeatures(sr, moptions);
    if (!features.empty())
    {
      Fmi::FeaturePtr feature = features[0];
      boost::posix_time::ptime timestamp =
          boost::get<boost::posix_time::ptime>(feature->attributes[field]);
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
      metadata = itsGisEngine->getMetaData(mdq_options);
    }
    catch (...)
    {
      if (!quiet)
      {
        Spine::Exception exception(BCP, "WMS layer metadata update failure!", NULL);
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

    if (hasTemporalDimension)
    {
      if (even_timesteps(metadata.timesteps))
      {
        boost::posix_time::ptime first_time(metadata.timesteps[0]);
        boost::posix_time::ptime last_time(metadata.timesteps[metadata.timesteps.size() - 1]);
        boost::posix_time::time_duration resolution =
            (metadata.timesteps[1] - metadata.timesteps[0]);

        std::unique_ptr<IntervalTimeDimension> newTimeDimension(
            new IntervalTimeDimension(first_time, last_time, resolution));
        timeDimension = std::move(newTimeDimension);
      }
      else
      {
        std::unique_ptr<StepTimeDimension> newTimeDimension(new StepTimeDimension());
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
    throw Spine::Exception(BCP, "Failed to update PostGIS layer metadata!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
