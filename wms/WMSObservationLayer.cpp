#include "WMSObservationLayer.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/move/make_unique.hpp>
#include <macgyver/Exception.h>

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

    metadataTimestamp = boost::posix_time::second_clock::universal_time();

    std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>> newTimeDimensions;

    auto newTimeDimension =
        boost::make_shared<IntervalTimeDimension>(metaData.period.begin(),
                                                  metaData.period.end(),
                                                  boost::posix_time::minutes(metaData.timestep));

    boost::posix_time::ptime origintime(boost::posix_time::not_a_date_time);
    newTimeDimensions.insert(std::make_pair(origintime, newTimeDimension));
    timeDimensions = boost::make_shared<WMSTimeDimensions>(newTimeDimensions);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update observation layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
