#include "WMSGridDataLayer.h"
#include <boost/move/make_unique.hpp>
#include <spine/Exception.h>
#include <grid-files/identification/GridDef.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{

WMSGridDataLayer::WMSGridDataLayer(const WMSConfig& config, const std::string& producer,uint geometryId)
    : WMSLayer(config), itsGridEngine(config.gridEngine()), itsProducer(producer),itsGeometryId(geometryId)
{
}




void WMSGridDataLayer::updateLayerMetaData()
{
  try
  {
    if (itsGeometryId > 0)
    {
      GRIB2::GridDef_ptr def = Identification::gridDef.getGrib2DefinitionByGeometryId(itsGeometryId);
      if (def != nullptr)
      {
        T::Coordinate topLeft,topRight,bottomLeft,bottomRight;
        if (def->getGridLatLonArea(topLeft,topRight,bottomLeft,bottomRight))
        {
          geographicBoundingBox.xMin = std::min(topLeft.x(), bottomLeft.x());
          geographicBoundingBox.xMax = std::max(topRight.x(), bottomRight.x());
          geographicBoundingBox.yMin = std::min(bottomLeft.y(), bottomRight.y());
          geographicBoundingBox.yMax = std::max(topLeft.y(),topRight.y());
        }
      }
    }

#if 0
    auto q = itsQEngine->get(itsProducer);

    // bounding box from metadata
    Engine::Querydata::MetaData metaData(q->metaData());
    geographicBoundingBox.xMin = std::min(metaData.ullon, metaData.bllon);
    geographicBoundingBox.xMax = std::max(metaData.urlon, metaData.brlon);
    geographicBoundingBox.yMin = std::min(metaData.bllat, metaData.brlat);
    geographicBoundingBox.yMax = std::max(metaData.ullat, metaData.urlat);

    // time dimension is sniffed from querydata
    boost::shared_ptr<Engine::Querydata::ValidTimeList> validtimes = q->validTimes();

    if (even_timesteps(*validtimes))
    {
      // interval
      boost::posix_time::time_duration first_timestep =
          (*(++validtimes->begin()) - *(validtimes->begin()));
      auto newTimeDimension = boost::movelib::make_unique<IntervalTimeDimension>(
          *(validtimes->begin()), *(--validtimes->end()), first_timestep);
      timeDimension = std::move(newTimeDimension);
    }
    else
    {
      // timesteps
      auto newTimeDimension = boost::movelib::make_unique<StepTimeDimension>();
      for (auto tim : *validtimes)
        newTimeDimension->addTimestep(tim);
      timeDimension = std::move(newTimeDimension);
    }
    metadataTimestamp = boost::posix_time::second_clock::universal_time();
#endif
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to update querydata layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
