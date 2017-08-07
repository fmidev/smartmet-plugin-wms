#include "WMSObservationLayer.h"
#include <boost/date_time/posix_time/posix_time.hpp>
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

    std::unique_ptr<IntervalTimeDimension> newTimeDimension(
        new IntervalTimeDimension(metaData.period.begin(),
                                  metaData.period.end(),
                                  boost::posix_time::minutes(metaData.timestep)));
    timeDimension = std::move(newTimeDimension);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to update observation layer metadata!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
