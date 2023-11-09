#include "WMSQueryDataLayer.h"
#include <boost/move/make_unique.hpp>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
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
          boost::make_shared<WMSElevationDimension>(level_name, level_type, elevations);

    // bounding box from metadata
    Engine::Querydata::MetaData metaData(q->metaData());
    geographicBoundingBox.xMin = metaData.wgs84Envelope.getRangeLon().getMin();
    geographicBoundingBox.xMax = metaData.wgs84Envelope.getRangeLon().getMax();
    geographicBoundingBox.yMin = metaData.wgs84Envelope.getRangeLat().getMin();
    geographicBoundingBox.yMax = metaData.wgs84Envelope.getRangeLat().getMax();

    // time dimension is sniffed from querydata
    auto queryDataConf = itsQEngine->getProducerConfig(itsProducer);

    std::map<Fmi::DateTime, boost::shared_ptr<WMSTimeDimension>> newTimeDimensions;
    // If multifile observation we want to have one timeseries and one reference time
    if (queryDataConf.ismultifile && !queryDataConf.isforecast)
    {
      boost::shared_ptr<Engine::Querydata::ValidTimeList> validtimes = q->validTimes();

      if (!validtimes->empty())
      {
        boost::shared_ptr<WMSTimeDimension> timeDimension;
        time_intervals intervals = get_intervals(*validtimes);
        if (!intervals.empty())
          timeDimension = boost::make_shared<IntervalTimeDimension>(intervals);
        else
          timeDimension = boost::make_shared<StepTimeDimension>(*validtimes);

        newTimeDimensions.insert(std::make_pair(validtimes->back(), timeDimension));
      }
    }
    else
    {
      Engine::Querydata::OriginTimes origintimes = itsQEngine->origintimes(itsProducer);
      for (const auto& t : origintimes)
      {
        q = itsQEngine->get(itsProducer, t);
        boost::shared_ptr<Engine::Querydata::ValidTimeList> vt = q->validTimes();
        boost::shared_ptr<WMSTimeDimension> timeDimension;

        time_intervals intervals = get_intervals(*vt);
        if (!intervals.empty())
          timeDimension = boost::make_shared<IntervalTimeDimension>(intervals);
        else
          timeDimension = boost::make_shared<StepTimeDimension>(*vt);

        newTimeDimensions.insert(std::make_pair(t, timeDimension));
      }
    }
    if (!newTimeDimensions.empty())
      timeDimensions = boost::make_shared<WMSTimeDimensions>(newTimeDimensions);
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
