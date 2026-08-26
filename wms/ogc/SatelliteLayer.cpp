#include "SatelliteLayer.h"
#include "TimeDimension.h"
#include <macgyver/Exception.h>
#include <algorithm>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{
// Drop the times which are not multiples of the timestep requested in
// the product definition
void apply_timestep(std::vector<Fmi::DateTime>& timelist, std::optional<int> timestep)
{
  if (!timestep || *timestep <= 0)
    return;

  auto pos = std::remove_if(timelist.begin(),
                            timelist.end(),
                            [&](const Fmi::DateTime& t)
                            {
                              // Anchor to UTC midnight of that day
                              auto midnight = Fmi::DateTime(t.date(), Fmi::Minutes(0));
                              auto since_midnight = (t - midnight).minutes();
                              return (since_midnight % *timestep) != 0;
                            });

  timelist.erase(pos, timelist.end());
}
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Update the bounding box and the time dimension
 */
// ----------------------------------------------------------------------

bool SatelliteLayer::updateLayerMetaData()
{
  try
  {
    if (itsSatelliteEngine == nullptr)
      return false;  // The engine is not loaded, hence the layer is not available

    if (!itsSatelliteEngine->hasProduct(itsProducer, itsParameter))
      return false;

    auto info = itsSatelliteEngine->productInfo(itsProducer, itsParameter);

    // Without a bounding box the layer cannot be advertised. The engine
    // estimates it from the newest image, hence this can only happen
    // when no images have been found yet.
    if (!info.bbox)
      return false;

    geographicBoundingBox.xMin = (*info.bbox)[0];
    geographicBoundingBox.yMin = (*info.bbox)[1];
    geographicBoundingBox.xMax = (*info.bbox)[2];
    geographicBoundingBox.yMax = (*info.bbox)[3];

    auto times = itsSatelliteEngine->times(itsProducer, itsParameter);

    std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>> newTimeDimensions;

    if (!times.empty())
    {
      apply_timestep(times, timestep);

      std::shared_ptr<TimeDimension> timeDimension;
      time_intervals intervals = get_intervals(times);
      if (!intervals.empty())
        timeDimension = std::make_shared<IntervalTimeDimension>(intervals);
      else
        timeDimension = std::make_shared<StepTimeDimension>(times);

      // Satellite images have no reference time
      newTimeDimensions.insert({Fmi::DateTime::NOT_A_DATE_TIME, timeDimension});

      itsModificationTime = times.back();
    }

    timeDimensions =
        newTimeDimensions.empty() ? nullptr : std::make_shared<TimeDimensions>(newTimeDimensions);

    metadataTimestamp = Fmi::SecondClock::universal_time();

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update satellite layer metadata!")
        .addParameter("Producer", itsProducer)
        .addParameter("Parameter", itsParameter);
  }
}

// ----------------------------------------------------------------------

const Fmi::DateTime& SatelliteLayer::modificationTime() const
{
  return std::max(itsModificationTime, itsProductFileModificationTime);
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
