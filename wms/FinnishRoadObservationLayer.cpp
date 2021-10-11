#ifndef WITHOUT_OBSERVATION

#include "FinnishRoadObservationLayer.h"
#include "State.h"
#include "Config.h"
#include "ValueTools.h"
#include "LonLatToXYTransformation.h"
#include "Hash.h"
#include <macgyver/NearTree.h>
#include <spine/ParameterTools.h>
#include <spine/TimeSeriesAggregator.h>
#include <ctpp2/CDT.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{

bool priority_sort(const StationSymbolPriority& rh1, const StationSymbolPriority& rh2)
{
  return (rh1.priority > rh2.priority);
}

int get_symbol(int r, int rform)
{
  /*--------------------------------------------------------------------------
   * r: 0 pouta, 1 heikko, 2 kohtalainen, 3 runsas, 4 heikko lumisade, 
   *    5 kohtalainen lumisade, 6 runsas lumisade
   *--------------------------------------------------------------------------
   * rform: 7 pouta, 8 hyvin heikko, 9 tihkusade, 10 vesisade, 11 lumisade, 
   *        12 märkä räntä, 13 räntäsade, 14 rakeita, 15 jääkiteitä, 
   *        16 lumijyväsiä, 17 lumirakeita, 18 jäätävä tihku, 19 jäätävä sade
   ---------------------------------------------------------------------------*/

  if (r == 1)
    {
      if (rform == 9) return 51;         // drizzle, not freezing, intermittent, slight
      if (rform == 10) return 52;        // rain, not freezing, intermittent, slight
      if (rform == 11) return 53;        // intermittent fall of snowflakes, slight
      if (rform == 18) return 213;       // drizzle, freezing, slight
      if (rform == 19) return 223;       // rain, freezing, slight
    }
  if (r == 2)
    {
      if (rform == 9) return 209;        // drizzle, not freezing, intermittent, moderate
      if (rform == 10) return 219;       // rain, not freezing, intermittent, moderate
      if (rform == 11) return 229;       // intermittent fall of snowflakes, moderate
      if (rform == 18) return 214;       // drizzle, freezing, moderate or heavy
      if (rform == 19) return 224;       // rain, freezing, moderate or heavy
    }
  if (r == 3)
    {
      if (rform == 9) return 212;       // drizzle, not freezing, continous, heavy
      if (rform == 10) return 222;      // rain, not freezing, continous, heavy
      if (rform == 11) return 232;      // continous flall of snowflakes, heavy
      if (rform == 18) return 214;      // drizzle, freezing, moderate or heavy
      if (rform == 19) return 224;      // rain, freezing, moderate or heavy
    }
  if (rform == 13) return 225;          // rain or drizzle and snow, slight
  if (rform == 14) return 244;          // shower(s) of snow pellets or snow hail
  if (rform == 15) return 233;          // ice needles (with or without fog) 
  if (rform == 16) return 234;          // snow grains (with or without fog)
  if (rform == 17) return 236;          // ice pellets (sleet)

  // r 4,5,6 are redundant but re-check in case there is no match in the the cases above
  if (r == 4) return 53;        // intermittent fall of snowflakes, slight
  if (r == 5) return 229;       // intermittent fall of snowflakes, moderate
  if (r == 6) return 232;      // continous flall of snowflakes, heavy
  
  return 106;

}

int get_symbol_priority(int symbol, double t2m)
{		     
  if(t2m <= 0)
	{
	  // WINTER: t2m < 0
	  switch (symbol)
		{
		case 106:
		  return 0;
		  break;
		case 53:
		case 229:
		  return 1; 
		  break;
		case 232:
		case 233:
		case 234:
		case 236:
		case 244:
		  return 2; 
		  break;
		case 225:
		  return 5;
		  break;
		case 51:
		case 213:
		  return 6;
		  break;
		case 209:
		  return 7;
		  break;
		case 52:
		case 212:
		case 214:
		case 223:
		  return 8;
		  break;
		case 219:
		  return 9;
		  break;
		case 222:
		case 224:
		  return 10;
		  break;
		}

	}
  else if(t2m < 10)
	{
	  // SPRING OR AUTUMN: 0 > t2m < 10
	  switch (symbol)
		{
		case 106:
		  return 0;
		  break;
		case 51:
		case 52:
		case 53:
		case 213:
		case 223:
		case 225:
		  return 1;        // slight
		  break;
		case 209:
		case 219:
		case 229:
		case 233:
		case 234:
		case 236:
		case 244:
		  return 2;     // moderate
		  break;
		case 212:
		case 214:
		case 222:
		case 224:
		case 232:
		  return 4;      // heavy
		  break;
		}
	}
  else
	{
	  // SUMMER: t2m >= 10
	  switch (symbol)
		{
		case 51:
		case 52:
		case 106:
		  return 0;
		  break;
		case 209:
		case 219:
		  return 1;
		  break;
		case 212:
		case 222:
		  return 2;
		  break;
		case 213:
		  return 3;
		  break;
		case 214:
		  return 4;
		  break;
		case 225:
		  return 5;
		  break;
		case 53:
		case 223:
		  return 6;
		  break;
		case 224:
		case 229:
		  return 7;
		  break;
		case 232:
		  return 8;
		  break;
		case 233:
		case 234:
		case 236:
		case 244:
		  return 9;
		  break;
		}
	}

  return 0;
}

}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void FinnishRoadObservationLayer::init(const Json::Value& theJson,
							const State& theState,
							const Config& theConfig,
							const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "FinnishRoadObservation-layer JSON is not a JSON hash");

    ObservationLayer::init(theJson, theState, theConfig, theProperties);

    Json::Value nulljson;

	Json::Value json = theJson.get("mindistance", nulljson);
    if (!json.isNull())		
	  mindistance = json.asInt();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "FinnishRoadObservationLayer init failed!");
  }
}

StationSymbolPriorities FinnishRoadObservationLayer::getProcessedData(State& theState) const
{
  try
  {
	if (theState.getConfig().obsEngineDisabled())
	  throw std::runtime_error("Cannot use FinnishRoadObservationLayer when the observation engine is disabled");  
	
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


ResultSet FinnishRoadObservationLayer::getObservations(State& theState, const boost::posix_time::ptime& requested_timestep) const
{
  try
  {
	auto endtime = requested_timestep;
	auto starttime = (endtime - boost::posix_time::hours(24)); //  // We must calculate mean temperature from last 24 hours
	
	std::vector<SmartMet::Spine::Parameter> parameters;
	parameters.push_back(Spine::makeParameter("fmisid"));
	parameters.push_back(Spine::makeParameter("longitude"));
	parameters.push_back(Spine::makeParameter("latitude"));
	parameters.push_back(Spine::makeParameter("ILMA"));
	parameters.push_back(Spine::makeParameter("SADE"));
	parameters.push_back(Spine::makeParameter("RST"));
	
	return ObservationLayer::getObservations(theState, parameters, starttime, endtime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gettings finnish road observations failed!");
  }
}

StationSymbolPriorities FinnishRoadObservationLayer::processResultSet(State& theState, const ResultSet& theResultSet, const boost::posix_time::ptime& requested_timestep) const
{
  try
  {
	auto zone = theState.getGeoEngine().getTimeZones().time_zone_from_string("UTC");
	auto requested_local_time = boost::local_time::local_date_time(requested_timestep, zone);
	
	// Do the aggregation and transform longitude/latitude to screen coordinates
	
	// Functions for aggregation
	Spine::ParameterFunction ilma_func(Spine::FunctionId::Mean, Spine::FunctionType::TimeFunction);
	ilma_func.setAggregationIntervalBehind(1440); // 1440 minutes = 24h
	ilma_func.setAggregationIntervalAhead(0);
	Spine::ParameterFunction sade_rst_func(Spine::FunctionId::Nearest, Spine::FunctionType::TimeFunction);
	sade_rst_func.setAggregationIntervalBehind(20); // 20 minutes behind
	sade_rst_func.setAggregationIntervalAhead(20); // 20 minutes ahead
	// Transformation object
	auto transformation = LonLatToXYTransformation(projection);
	
	StationSymbolPriorities ssps;
	Spine::TimeSeries::Value none = Spine::TimeSeries::None();  
	for(auto& result_set_item : theResultSet)
	  {
		StationSymbolPriority ssp;
		ssp.fmisid = result_set_item.first;
		// FMISID: data independent
		auto& longitude_result_set_vector = result_set_item.second.at(1);
		auto lon = get_double(longitude_result_set_vector.at(0).value);
		// Latitude: data independent
		auto& latitude_result_set_vector = result_set_item.second.at(2);
		auto lat = get_double(latitude_result_set_vector.at(0).value);
		// Transform to screen coordinates
		if (transformation.transform(lon, lat))
		  {
			ssp.longitude = lon;
			ssp.latitude = lat;
		  }
		else
		  {
			throw Fmi::Exception::Trace(BCP, "Transforming longitude, latitude to screen coordinates failed!");
		  }
		// ILMA: get mean of previous 24 hours
		auto& ilma_result_set_vector = result_set_item.second.at(3);
		auto agg_val = Spine::TimeSeries::time_aggregate(ilma_result_set_vector, ilma_func, requested_local_time);
		if(agg_val.value == none)
		  {
			continue;
		  }
		auto t2m = get_double(agg_val.value);
		// SADE: get nearest value of previous or next 20 minutes
		auto& sade_result_set_vector = result_set_item.second.at(4);
		auto sade = Spine::TimeSeries::time_aggregate(sade_result_set_vector, sade_rst_func, requested_local_time);
		// RST: get nearest value of previous or next 20 minutes
		auto& rst_result_set_vector = result_set_item.second.at(5);
		auto rst = Spine::TimeSeries::time_aggregate(rst_result_set_vector, sade_rst_func, requested_local_time);
		
		if(get_double(sade.value) == kFloatMissing || get_double(rst.value) == kFloatMissing)
		  {
			continue;
		  }
		
		// Symbol
		ssp.symbol = get_symbol(get_double(sade.value), get_double(rst.value));
		// Priority
		ssp.priority = get_symbol_priority(ssp.symbol, t2m);
				
		ssps.push_back(ssp);
	  } 
	
	// Sort according to proirity
	std::sort(ssps.begin(), ssps.end(), priority_sort);
	
	return ssps;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Processing result set of finnish road observations failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 *
 */
// ----------------------------------------------------------------------

void FinnishRoadObservationLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
	{
	  if (theState.getConfig().obsEngineDisabled())
		throw std::runtime_error("Cannot use FinnishRoadObservationLayer when the observation engine is disabled");  
	  
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
		  text_cdt["cdata"] = ("&#" + Fmi::to_string(ssp.symbol)+";");//cdata;
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

std::size_t FinnishRoadObservationLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = ObservationLayer::hash_value(theState);

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
