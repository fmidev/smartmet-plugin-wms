#include "WMSQueryDataLayer.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
  void apply_timestep(Engine::Querydata::ValidTimeList& timelist, std::optional<int> timestep)
  {
    if(!timestep || timestep <= 0)
      return;

    timelist.remove_if([&](const Fmi::DateTime& t) {
        // Anchor to UTC midnight of that day
	// auto midnight = floor<Fmi::Days>(t);
	auto midnight = Fmi::DateTime(t.date(), Fmi::Minutes(0));

        auto since_midnight = (t - midnight).minutes();
        return (since_midnight % *timestep) != 0;
    });
  }
}
  
bool WMSQueryDataLayer::updateLayerMetaData()
{
  try
  {
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
          std::make_shared<WMSElevationDimension>(level_name, level_type, elevations);

    // bounding box from metadata
    Engine::Querydata::MetaData metaData(q->metaData());
    geographicBoundingBox.xMin = metaData.wgs84Envelope.getRangeLon().getMin();
    geographicBoundingBox.xMax = metaData.wgs84Envelope.getRangeLon().getMax();
    geographicBoundingBox.yMin = metaData.wgs84Envelope.getRangeLat().getMin();
    geographicBoundingBox.yMax = metaData.wgs84Envelope.getRangeLat().getMax();

    // time dimension is sniffed from querydata
    auto queryDataConf = itsQEngine->getProducerConfig(itsProducer);

    std::map<Fmi::DateTime, std::shared_ptr<WMSTimeDimension>> newTimeDimensions;

    // We do not want a reference time for multifiles
    if (queryDataConf.ismultifile)
    {
      std::shared_ptr<Engine::Querydata::ValidTimeList> validtimes = q->validTimes();

      if (!validtimes->empty())
      {
	apply_timestep(*validtimes, timestep);
        std::shared_ptr<WMSTimeDimension> timeDimension;
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
        std::shared_ptr<Engine::Querydata::ValidTimeList> vt = q->validTimes();
	if(vt)
	  apply_timestep(*vt, timestep);

        std::shared_ptr<WMSTimeDimension> timeDimension;
        time_intervals intervals = get_intervals(*vt);
        if (!intervals.empty())
          timeDimension = std::make_shared<IntervalTimeDimension>(intervals);
        else
          timeDimension = std::make_shared<StepTimeDimension>(*vt);

        newTimeDimensions.insert(std::make_pair(t, timeDimension));
      }
    }
    if (!newTimeDimensions.empty())
      timeDimensions = std::make_shared<WMSTimeDimensions>(newTimeDimensions);
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

const Fmi::DateTime& WMSQueryDataLayer::modificationTime() const
{
  return std::max(itsModificationTime, itsProductFileModificationTime);
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
