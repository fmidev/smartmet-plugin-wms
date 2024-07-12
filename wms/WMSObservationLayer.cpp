#include "WMSObservationLayer.h"
#include <macgyver/DateTime.h>
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

    std::map<Fmi::DateTime, std::shared_ptr<WMSTimeDimension>> newTimeDimensions;

    tag_interval interval(metaData.period.begin(),
                          metaData.period.end(),
                          Fmi::Minutes(metaData.timestep));
    time_intervals timeintervals{interval};
    auto newTimeDimension = std::make_shared<IntervalTimeDimension>(timeintervals);

    Fmi::DateTime origintime(Fmi::DateTime::NOT_A_DATE_TIME);
    newTimeDimensions.insert(std::make_pair(origintime, newTimeDimension));
    timeDimensions = std::make_shared<WMSTimeDimensions>(newTimeDimensions);
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
