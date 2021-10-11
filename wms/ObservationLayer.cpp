#ifndef WITHOUT_OBSERVATION

#include "ObservationLayer.h"
#include "State.h"
#include "Config.h"
#include "ValueTools.h"
#include "Hash.h"
#include <spine/TimeSeries.h>
#include <engines/observation/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void ObservationLayer::init(const Json::Value& theJson,
							const State& theState,
							const Config& theConfig,
							const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Observation-layer JSON is not a JSON hash");

    Layer::init(theJson, theState, theConfig, theProperties);


	if(!producer)
	  throw Fmi::Exception::Trace(BCP, "Producer must be defined for obervation layer!");
	if(!timestep)
	  throw Fmi::Exception::Trace(BCP, "Timestep must be defined for obervation layer!");

    Json::Value nulljson;

	// Stations of the layer
	Json::Value json = theJson.get("keyword", nulljson);
    if (!json.isNull())
	  {
		keyword = json.asString();
		if(!keyword.empty())
		  {
			const auto& geoengine = theState.getGeoEngine();
			Locus::QueryOptions options;
			auto locations =  geoengine.keywordSearch(options, keyword);
			for(auto loc : locations)
			  stationFMISIDs.push_back(Spine::TaggedFMISID(Fmi::to_string(*loc->fmisid), *loc->fmisid));
		  }
	  }

	// If keyword is missing try 'fmisids'
    if (json.isNull())
	  {
		json = theJson.get("fmisid", nulljson);
		if (!json.isNull())
		  {
			if (json.isString())
			  {
				fmisids = json.asString();
				stationFMISIDs.push_back(Spine::TaggedFMISID(fmisids, Fmi::stoi(fmisids)));
			  }
			else if (json.isArray())
			  {
				for (unsigned int i = 0; i < json.size(); i++)
				  {
					std::string fmisid = json[i].asString();
					stationFMISIDs.push_back(Spine::TaggedFMISID(fmisid, Fmi::stoi(fmisid)));
					fmisids += fmisid;
				  }
			  }
			else
			  throw Fmi::Exception(BCP, "fmisid value must be an array of strings or a string");
		  }
	  }
		
	if(keyword.empty() && fmisids.empty())
	  throw Fmi::Exception::Trace(BCP, "Either 'keyword' or 'fmisid' must be defined for observation layer! ");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ObservationLayer init failed!");
  }
}

ResultSet ObservationLayer::getObservations(State& theState, const std::vector<SmartMet::Spine::Parameter>& parameters, const boost::posix_time::ptime& starttime, const boost::posix_time::ptime& endtime) const
{
  try
	{
	  if (theState.getConfig().obsEngineDisabled())
		throw std::runtime_error("Cannot use ObservationLayer when the observation engine is disabled");  
	  
	  ResultSet ret;
	  
	  Engine::Observation::Settings settings;
	  
	  settings.stationtype = *producer;
	  settings.timezone = "UTC";
	  settings.localTimePool = theState.getLocalTimePool();
	  settings.timestep = 0; // All timesteps are fetched, after postprocessing only requested timestep(s) are used
	  settings.starttimeGiven = true;  
	  settings.starttime = starttime;
	  settings.endtime = endtime;
	  settings.taggedFMISIDs = stationFMISIDs;
	  settings.parameters = parameters;
	  
	  // Get values from Observation engine
	  auto& obsengine = theState.getObsEngine();
	  
	  auto values = obsengine.values(settings);
	  
	  if(values->empty())
		return ret;
	  
	  // Find FMISID index in result set
	  unsigned int fmisid_index = 0;
	  for(unsigned int i = 0; i < settings.parameters.size(); i++)
		{
		  if(Fmi::ascii_tolower_copy(settings.parameters.at(i).name()) == "fmisid")
			break;
		  fmisid_index = i;
		}
	  
	  if(values->at(fmisid_index).empty())
		return ret;
	  
	  // Find out FMISIDs their data in result set
	  std::map<int, std::pair<int, int>> fmisid_ranges;
	  const Spine::TimeSeries::TimeSeries& fmisid_vector = values->at(fmisid_index);
	  for(unsigned int i = 0; i < fmisid_vector.size(); i++)
		{
		  int fmisid = get_fmisid(fmisid_vector.at(i).value);
		  if(fmisid_ranges.find(fmisid) == fmisid_ranges.end())
			{
			  fmisid_ranges[fmisid] = std::pair<int,int>(i,i);
			}
		  else
			{
			  fmisid_ranges[fmisid].second = i;
			}
		}
	  
	  // Allocate map for results of valid fmisids  
	  for(const auto& item : fmisid_ranges)
		ret.insert(std::make_pair(item.first, std::vector<Spine::TimeSeries::TimeSeries>(settings.parameters.size(), Spine::TimeSeries::TimeSeries(settings.localTimePool))));
	  
	  // Add results to map
	  for(unsigned int i = 0; i < values->size(); i++)
		{
		  const Spine::TimeSeries::TimeSeries& source_vector = values->at(i);
		  for(const auto& item : fmisid_ranges)
			{
			  Spine::TimeSeries::TimeSeries::const_iterator iter_first = source_vector.begin() + item.second.first;
			  Spine::TimeSeries::TimeSeries::const_iterator iter_last = source_vector.begin() + item.second.second;
			  Spine::TimeSeries::TimeSeries& target_vector = ret[item.first][i];
			  target_vector.insert(target_vector.begin(), iter_first, iter_last); 
			}
		}
	  
	  return ret;
	}
  catch (...)
	{
	  throw Fmi::Exception::Trace(BCP, "Getting observations for ObservationLayer failed!");
	}
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t ObservationLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    Fmi::hash_combine(hash, Fmi::hash_value(keyword));
    Fmi::hash_combine(hash, Fmi::hash_value(fmisids));

    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
