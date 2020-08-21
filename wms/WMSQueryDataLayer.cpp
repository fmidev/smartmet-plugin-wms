#include "WMSQueryDataLayer.h"
#include <boost/move/make_unique.hpp>
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
      auto newTimeDimension = boost::movelib::make_unique<IntervalTimeDimension>(
          *(validtimes->begin()), *(--validtimes->end()), first_timestep);
      timeDimension = std::move(newTimeDimension);
    }
    else
    {
      timeDimension = boost::make_shared<StepTimeDimension>(*validtimes);
    }
    metadataTimestamp = boost::posix_time::second_clock::universal_time();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to update querydata layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
