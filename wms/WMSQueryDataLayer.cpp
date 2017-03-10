#include "WMSQueryDataLayer.h"
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
void WMSQueryDataLayer::updateLayerMetaData()
{
  try
  {
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
      std::unique_ptr<IntervalTimeDimension> newTimeDimension(new IntervalTimeDimension(
          *(validtimes->begin()), *(--validtimes->end()), first_timestep));
      timeDimension = std::move(newTimeDimension);
    }
    else
    {
      // timesteps
      std::unique_ptr<StepTimeDimension> newTimeDimension(new StepTimeDimension());
      for (auto tim : *validtimes)
        newTimeDimension->addTimestep(tim);
      timeDimension = std::move(newTimeDimension);
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
