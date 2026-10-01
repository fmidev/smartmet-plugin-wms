#include "QueryDataLayer.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{
  // Return the valid times which are multiples of the timestep counted from UTC midnight.
  // The input list must not be modified: for a single model it is the engine's shared
  // Model::validTimes() list, and pruning it in place would change the time dimension of
  // every other layer using the same producer (and race with concurrent readers).
  std::shared_ptr<Engine::Querydata::ValidTimeList> apply_timestep(
      const std::shared_ptr<Engine::Querydata::ValidTimeList>& timelist,
      std::optional<int> timestep)
  {
    if (!timelist || !timestep || *timestep <= 0)
      return timelist;

    auto ret = std::make_shared<Engine::Querydata::ValidTimeList>();
    for (const auto& t : *timelist)
    {
      auto midnight = Fmi::DateTime(t.date(), Fmi::Minutes(0));
      auto since_midnight = (t - midnight).total_minutes();
      if (since_midnight % *timestep == 0)
        ret->push_back(t);
    }
    return ret;
  }
}
  
bool QueryDataLayer::updateLayerMetaData()
{
  try
  {
    auto queryDataConf = itsQEngine->getProducerConfig(itsProducer);

    // Lazy radar producer: build the time dimension from the engine's catalogue
    // (header-only) without decoding any frame, so GetCapabilities stays complete
    // and cheap for a cold (unloaded) producer. Engine::get() would decode the
    // whole servable window of every lazy producer on each metadata pass.
    if (queryDataConf.islazy)
    {
      auto md = itsQEngine->getRadarLayerMetaData(itsProducer);
      if (md.valid && md.validtimes && !md.validtimes->empty())
      {
        itsModificationTime = md.modificationTime;
        geographicBoundingBox.xMin = md.west;
        geographicBoundingBox.xMax = md.east;
        geographicBoundingBox.yMin = md.south;
        geographicBoundingBox.yMax = md.north;

        std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>> newTimeDimensions;
        auto validtimes = apply_timestep(md.validtimes, timestep);
        time_intervals intervals = get_intervals(*validtimes);
        std::shared_ptr<TimeDimension> timeDimension;
        if (!intervals.empty())
          timeDimension = std::make_shared<IntervalTimeDimension>(intervals);
        else
          timeDimension = std::make_shared<StepTimeDimension>(*validtimes);
        newTimeDimensions.insert(std::make_pair(Fmi::DateTime::NOT_A_DATE_TIME, timeDimension));
        timeDimensions = std::make_shared<TimeDimensions>(newTimeDimensions);
        timeDimensions->useLatestTimeAsDefault(!queryDataConf.isforecast);
        metadataTimestamp = Fmi::SecondClock::universal_time();
        return true;
      }
      // Catalogue empty (e.g. before the first directory scan): fall through to
      // the normal path, which decodes on access.
    }

    auto q = itsQEngine->get(itsProducer);
    itsModificationTime = q->modificationTime();

    std::string level_name = q->levelName();
    FmiLevelType level_type = q->levelType();
    std::set<int> elevations;

    if (q->firstLevel())
      elevations.insert(q->levelValue());
    while (q->nextLevel())
      elevations.insert(q->levelValue());
    if (!elevations.empty())
      elevationDimension =
          std::make_shared<ElevationDimension>(level_name, level_type, elevations);

    // bounding box from metadata
    Engine::Querydata::MetaData metaData(q->metaData());
    geographicBoundingBox.xMin = metaData.wgs84Envelope.getRangeLon().getMin();
    geographicBoundingBox.xMax = metaData.wgs84Envelope.getRangeLon().getMax();
    geographicBoundingBox.yMin = metaData.wgs84Envelope.getRangeLat().getMin();
    geographicBoundingBox.yMax = metaData.wgs84Envelope.getRangeLat().getMax();

    // time dimension is sniffed from querydata

    std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>> newTimeDimensions;

    // We do not want a reference time for multifiles
    if (queryDataConf.ismultifile)
    {
      auto validtimes = apply_timestep(q->validTimes(), timestep);

      if (validtimes && !validtimes->empty())
      {
        std::shared_ptr<TimeDimension> timeDimension;
        time_intervals intervals = get_intervals(*validtimes);
        if (!intervals.empty())
          timeDimension = std::make_shared<IntervalTimeDimension>(intervals);
        else
          timeDimension = std::make_shared<StepTimeDimension>(*validtimes);

        newTimeDimensions.insert(std::make_pair(Fmi::DateTime::NOT_A_DATE_TIME, timeDimension));
      }
    }
    else
    {
      Engine::Querydata::OriginTimes origintimes = itsQEngine->origintimes(itsProducer);
      for (const auto& t : origintimes)
      {
        q = itsQEngine->get(itsProducer, t);
        auto vt = apply_timestep(q->validTimes(), timestep);
        if (!vt)
          continue;

        std::shared_ptr<TimeDimension> timeDimension;
        time_intervals intervals = get_intervals(*vt);
        if (!intervals.empty())
          timeDimension = std::make_shared<IntervalTimeDimension>(intervals);
        else
          timeDimension = std::make_shared<StepTimeDimension>(*vt);

        newTimeDimensions.insert(std::make_pair(t, timeDimension));
      }
    }
    if (!newTimeDimensions.empty())
    {
      timeDimensions = std::make_shared<TimeDimensions>(newTimeDimensions);
      // Producers configured with forecast = false (radar, analyses) are
      // observations: their default time is the latest one available.
      timeDimensions->useLatestTimeAsDefault(!queryDataConf.isforecast);
    }
    else
      timeDimensions = nullptr;

    metadataTimestamp = Fmi::SecondClock::universal_time();

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update querydata layer metadata!");
  }
}

const Fmi::DateTime& QueryDataLayer::modificationTime() const
{
  return std::max(itsModificationTime, itsProductFileModificationTime);
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
