#include "WMSObservationLayer.h"
#include <macgyver/DateTime.h>
#include <boost/move/make_unique.hpp>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
bool WMSObservationLayer::updateLayerMetaData()
{
  try
  {
    // bounding box from metadata
    Engine::Observation::MetaData metaData(itsObsEngine->metaData(itsProducer));
    if (itsTimestep > -1)
      metaData.timestep = itsTimestep;
    geographicBoundingBox = metaData.bbox;

    metadataTimestamp = Fmi::SecondClock::universal_time();

    std::map<Fmi::DateTime, boost::shared_ptr<WMSTimeDimension>> newTimeDimensions;

    tag_interval interval(metaData.period.begin(),
                          metaData.period.end(),
                          Fmi::Minutes(metaData.timestep));
    time_intervals timeintervals{interval};
    auto newTimeDimension = boost::make_shared<IntervalTimeDimension>(timeintervals);

    Fmi::DateTime origintime(boost::posix_time::not_a_date_time);
    newTimeDimensions.insert(std::make_pair(origintime, newTimeDimension));
    timeDimensions = boost::make_shared<WMSTimeDimensions>(newTimeDimensions);
	timeDimensions->useWallClockTimeAsEndTime(true);

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update observation layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
