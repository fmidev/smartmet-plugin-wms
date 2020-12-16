#include "WMSQueryDataLayer.h"
#include <boost/move/make_unique.hpp>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
void WMSQueryDataLayer::updateLayerMetaData()
{
  try
  {
    auto q = itsQEngine->get(itsProducer);

	std::string level_name = q->levelName();
	FmiLevelType level_type = q->levelType();
	std::set<int> elevations;
	
	if(q->firstLevel())
	  elevations.insert( q->levelValue());
	while(q->nextLevel())
	  elevations.insert( q->levelValue());
	if(!elevations.empty())
	  elevationDimension = boost::make_shared<WMSElevationDimension>(level_name, level_type, elevations); 

    // bounding box from metadata
    Engine::Querydata::MetaData metaData(q->metaData());
    geographicBoundingBox.xMin = std::min(metaData.ullon, metaData.bllon);
    geographicBoundingBox.xMax = std::max(metaData.urlon, metaData.brlon);
    geographicBoundingBox.yMin = std::min(metaData.bllat, metaData.brlat);
    geographicBoundingBox.yMax = std::max(metaData.ullat, metaData.urlat);

    // time dimension is sniffed from querydata
    auto queryDataConf = itsQEngine->getProducerConfig(itsProducer);

	std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>> newTimeDimensions;
	// If multifile observation we want to have one timeseries and one reference time
	if(queryDataConf.ismultifile && !queryDataConf.isforecast)
	  {
		boost::shared_ptr<Engine::Querydata::ValidTimeList> validtimes = q->validTimes();
		
		if(!validtimes->empty())
		  {
			boost::shared_ptr<WMSTimeDimension> timeDimension;
			if (even_timesteps(*validtimes))
			  {
				// interval
				boost::posix_time::time_duration first_timestep =
				  (*(++validtimes->begin()) - *(validtimes->begin()));
				timeDimension = boost::make_shared<IntervalTimeDimension>(*(validtimes->begin()), *(--validtimes->end()), first_timestep);
			  }
			else
			  {
				timeDimension = boost::make_shared<StepTimeDimension>(*validtimes);
			  }
			newTimeDimensions.insert(std::make_pair(validtimes->back(), timeDimension));		
		  }
	  }
	else
	  {
		Engine::Querydata::OriginTimes origintimes = itsQEngine->origintimes(itsProducer);
		for(const auto& t : origintimes)
		  {
			q =  itsQEngine->get(itsProducer, t);
			boost::shared_ptr<Engine::Querydata::ValidTimeList> vt = q->validTimes();
			boost::shared_ptr<WMSTimeDimension> timeDimension;
			
			if (even_timesteps(*vt))
			  {
				// interval
				boost::posix_time::time_duration first_timestep =
				  (*(++vt->begin()) - *(vt->begin()));
				timeDimension =  boost::make_shared<IntervalTimeDimension>(*(vt->begin()), *(--vt->end()), first_timestep);
			  }
			else
			  {
				timeDimension = boost::make_shared<StepTimeDimension>(*vt);
			  }
			
			newTimeDimensions.insert(std::make_pair(t, timeDimension));
		  }
	  }
	if(!newTimeDimensions.empty())
	  timeDimensions = boost::make_shared<WMSTimeDimensions>(newTimeDimensions);
	else
	  timeDimensions = nullptr;

    metadataTimestamp = boost::posix_time::second_clock::universal_time();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update querydata layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
