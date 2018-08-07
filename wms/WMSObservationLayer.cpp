#include "WMSObservationLayer.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/move/make_unique.hpp>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
void WMSObservationLayer::updateLayerMetaData()
{
  try
  {
    // bounding box from metadata
    Engine::Observation::MetaData metaData(itsObsEngine->metaData(itsProducer));
    if (itsTimestep > -1)
      metaData.timestep = itsTimestep;
    geographicBoundingBox = metaData.bbox;

    auto newTimeDimension = boost::movelib::make_unique<IntervalTimeDimension>(
        metaData.period.begin(),
        metaData.period.end(),
        boost::posix_time::minutes(metaData.timestep));
    timeDimension = std::move(newTimeDimension);
    metadataTimestamp = boost::posix_time::second_clock::universal_time();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to update observation layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
