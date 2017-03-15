#include "WMSPostGISLayer.h"
#include <spine/Exception.h>

#include <gis/Host.h>
#include <gis/OGR.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
WMSPostGISLayer::WMSPostGISLayer(const Engine::Gis::Engine* gisengine, const Json::Value& json)
    : itsGisEngine(gisengine), hasTemporalDimension(false)
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
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
        exception.addParameter("Layer name", getName());
        if (!exception.stackTraceDisabled())
          std::cerr << exception.getStackTrace();
        else if (!exception.loggingDisabled())
          std::cerr << "Error: " << exception.what() << std::endl;

        // std::cout << "WMS layer metadata update failure for layer '" + getName() + "' -> " +
        // e.what()
        //          << std::endl;
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
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
