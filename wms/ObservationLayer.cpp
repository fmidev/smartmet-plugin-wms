#ifndef WITHOUT_OBSERVATION

#include "ObservationLayer.h"
#include "Config.h"
#include "ValueTools.h"
#include "Hash.h"
#include <ctpp2/CDT.hpp>
#include <engines/observation/Engine.h>
#include <timeseries/TimeSeriesInclude.h>

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
		
	json = theJson.get("mindistance", nulljson);
    if (!json.isNull())		
	  mindistance = json.asInt();

	if(keyword.empty() && fmisids.empty())
	  throw Fmi::Exception::Trace(BCP, "Either 'keyword' or 'fmisid' must be defined for observation layer! ");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ObservationLayer init failed!");
  }
}

ResultSet ObservationLayer::getObservations(State& theState, const std::vector<Spine::Parameter>& parameters, const boost::posix_time::ptime& starttime, const boost::posix_time::ptime& endtime) const
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
	  //	  settings.debug_options = 1;
	  
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
	  const TS::TimeSeries& fmisid_vector = values->at(fmisid_index);
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
		ret.insert(std::make_pair(item.first, std::vector<TS::TimeSeries>(settings.parameters.size(), TS::TimeSeries(settings.localTimePool))));
	  
	  // Add results to map
	  for(unsigned int i = 0; i < values->size(); i++)
		{
		  const TS::TimeSeries& source_vector = values->at(i);
		  for(const auto& item : fmisid_ranges)
			{
			  TS::TimeSeries::const_iterator iter_first = source_vector.begin() + item.second.first;
			  TS::TimeSeries::const_iterator iter_last = source_vector.begin() + item.second.second;
			  TS::TimeSeries& target_vector = ret[item.first][i];
			  if(iter_first == iter_last)
				target_vector.insert(target_vector.begin(), *iter_first);
			  else
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

StationSymbolPriorities ObservationLayer::getProcessedData(State& theState) const
{
  try
  {
	if (theState.getConfig().obsEngineDisabled())
	  throw std::runtime_error("Cannot use ObservationLayer when the observation engine is disabled");  
	
	// If time not given take current time and find nearest previous timestep
	auto requested_timestep = (time ? *time : boost::posix_time::second_clock::universal_time());
	int timestep_minutes = *timestep;
	// Revert to previous timestep
	if(timestep_minutes != 0)
	  {
		while(requested_timestep.time_of_day().minutes() % timestep_minutes != 0)
		  {
			requested_timestep -= boost::posix_time::minutes(1);
		  }
	  }
	
	ResultSet result_set = getObservations(theState, requested_timestep);
	
	return processResultSet(theState, result_set, requested_timestep);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

ResultSet ObservationLayer::getObservations(State& theState, const boost::posix_time::ptime& requested_timestep) const
{
  try
  {
	boost::posix_time::ptime starttime;
	boost::posix_time::ptime endtime;
	std::vector<Spine::Parameter> parameters;

	getParameters(requested_timestep, parameters, starttime, endtime);

	return getObservations(theState, parameters, starttime, endtime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gettings observations failed!");
  }
}


void ObservationLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
	{
	  if (theState.getConfig().obsEngineDisabled())
		throw std::runtime_error("Cannot use PresentWeatherObservationLayer when the observation engine is disabled");  
	  
	  // Get data from obsengine, do the aggregation, transform longitude/latitude to screen coordinates, prioritize stations
	  StationSymbolPriorities ssps = getProcessedData(theState);

	  Fmi::NearTree<StationSymbolPriority> stations_to_show;
	  
	  for(const auto& ssp : ssps)
		{
		  // Skip the station if it is too close to some station already on map
		  if (mindistance > 0)
			{
			  auto match = stations_to_show.nearest(ssp, mindistance);

			  if (match)
				continue;

			  stations_to_show.insert(ssp);
			}
		  
		  std::string chr;
		  chr.append(1, ssp.symbol);
		  if(chr.empty())
			{
			  std::cout << "No symbol for station: " << ssp.fmisid << std::endl;
			  continue;
			}
		  
		  // Start generating the hash
		  CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
		  text_cdt["start"] = "<text";
		  text_cdt["end"] = "</text>";
		  text_cdt["cdata"] = ("&#" + Fmi::to_string(ssp.symbol)+";");
		  text_cdt["attributes"]["x"] = Fmi::to_string(lround(ssp.longitude));
		  text_cdt["attributes"]["y"] = Fmi::to_string(lround(ssp.latitude));
		  theState.addAttributes(theGlobals, text_cdt, attributes);
		  theLayersCdt.PushBack(text_cdt);
		}
	}
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Generating template hash for finnish road observations failed!");
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
    Fmi::hash_combine(hash, Fmi::hash_value(mindistance));

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
