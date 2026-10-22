#include "ObsGridLayer.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
// ----------------------------------------------------------------------
/*!
 * \brief Update the bounding box and the time dimension
 *
 * The times are the observation times offered for gridding. Like other
 * observations, the default time is the latest one.
 */
// ----------------------------------------------------------------------

bool ObsGridLayer::updateLayerMetaData()
{
  try
  {
    if (itsGridder == nullptr)
      throw Fmi::Exception(BCP, "Gridded observations not configured");

    const auto bbox = itsGridder->boundingBox(itsProducer);
    geographicBoundingBox.xMin = bbox.at("minx");
    geographicBoundingBox.xMax = bbox.at("maxx");
    geographicBoundingBox.yMin = bbox.at("miny");
    geographicBoundingBox.yMax = bbox.at("maxy");

    const auto times = itsGridder->validTimes(itsProducer);
    std::list<Fmi::DateTime> validtimes(times.begin(), times.end());

    // Apply the timestep of the layer, if any, counted from UTC midnight
    if (timestep && *timestep > 0)
      validtimes.remove_if(
          [this](const Fmi::DateTime& t)
          { return (t - Fmi::DateTime(t.date())).total_minutes() % *timestep != 0; });

    if (validtimes.empty())
      timeDimensions = nullptr;
    else
    {
      std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>> dimensions;
      dimensions.insert(std::make_pair(Fmi::DateTime::NOT_A_DATE_TIME,
                                       std::make_shared<StepTimeDimension>(validtimes)));
      timeDimensions = std::make_shared<TimeDimensions>(dimensions);
      timeDimensions->useLatestTimeAsDefault(true);
    }

    // New observation times become available as time passes
    itsModificationTime = validtimes.empty() ? Fmi::date_time::from_time_t(0) : validtimes.back();
    metadataTimestamp = Fmi::SecondClock::universal_time();
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update gridded observations layer metadata!")
        .addParameter("Producer", itsProducer);
  }
}

const Fmi::DateTime& ObsGridLayer::modificationTime() const
{
  return std::max(itsModificationTime, itsProductFileModificationTime);
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
