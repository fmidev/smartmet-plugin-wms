#ifndef WITHOUT_OBSERVATION

#include "PresentWeatherObservationLayer.h"
#include "State.h"
#include "Config.h"
#include "ValueTools.h"
#include <timeseries/ParameterTools.h>
#include "LonLatToXYTransformation.h"

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

float convert_possible_wawa_2_ww(float theValue)
{
    static const float wwCodeArray[100] =   
{0, 1, 2, 3, 4, 5, 0, 0, 0, 0,
 10, 0,13, 0, 0, 0, 0, 0,18, 0,
 28,21,20,21,22,24,29, 0, 0, 0,
 42,41,43,45,47,48, 0, 0, 0, 0,
 61,63,65,61,65,71,75,66,67, 0,
 50,51,53,55,56,57,57,58,59, 0,
 60,61,63,65,66,67,67,68,69, 0,
 70,71,73,75,79,79,79,77,78, 0,
 80,80,81,81,82,85,86,86, 0,89,
 92,17,93,96,17,97,99, 0, 0, 8};

    if(theValue >= 100 && theValue <= 199)
        return wwCodeArray[static_cast<int>(theValue-100)];
    return theValue;
}

int get_symbol(float wawa)
{
  float value = convert_possible_wawa_2_ww(wawa);
  if(value != kFloatMissing && value > 3)
	{
	  if(value == 99)
		return 48; // synop fontin viimeinen arvo 255 osuu synop arvolle 98, onneksi myös 99 löytyy fontista, mutta sen sijainti on vain 48
	  else
		return (157 + value);
	}
  return 106; // Missing symbol 'Z'
}

int get_symbol_priority(int symbol)
{
  switch (symbol)
	{
	case 106:
	  return 0;
	  break;
	case 170:
	case 171:
	case 172:
	case 173:
	  return 1;
	  break;
	case 162:
	case 163:
	case 164:
	case 165:
	case 167:
	case 168:
	case 169:
	case 174:
	case 177:
	case 178:
	case 179:
	case 180:
	case 181:
	case 182:
	case 183:
	case 184:
	case 185:
	case 186:
	case 198:
	case 207:
	case 208:
	case 213:
	case 215:
	case 217:
	case 218:
	case 227:
	case 235:
	case 242:
	case 244:
	case 246:
	case 248:
	case 249:
	case 250:
	case 251:
	  return 5;
	  break;
	case 187:
	case 188:
	case 189:
	case 193:
	case 195:
	case 197:
	case 199:
	case 200:
	case 209:
	case 210:
	case 219:
	case 220:
	case 225:
	case 228:
	case 229:
	case 233:
	case 234:
	case 236:
	case 237:
	case 240:
	  return 7;
	  break;
	case 161:
	case 175:
	case 211:
	case 216:
	case 221:
	case 223:
	case 230:
	case 231:
	case 238:
	case 252:
	case 253:
	  return 8;
	  break;
	case 201:
	case 202:
	case 203:
	case 204:
	case 205:
	case 206:
	case 212:
	case 239:
	case 241:
	case 243:
	case 245:
	case 247:
	  return 9;
	  break;
	case 166:
	case 176:
	case 190:
	case 191:
	case 192:
	case 194:
	case 196:
	case 214:
	case 222:
	case 224:
	case 226:
	case 232:
	case 254:
	case 255:
	case 48:
	  return 10;
	  break;
	}
  
  return 0;
}

}

void PresentWeatherObservationLayer::getParameters(const boost::posix_time::ptime& requested_timestep, std::vector<Spine::Parameter>& parameters,  boost::posix_time::ptime& starttime,  boost::posix_time::ptime&endtime) const
{
  try
  {
	endtime = requested_timestep;
	starttime = requested_timestep;

	parameters.push_back(TS::makeParameter("fmisid"));
	parameters.push_back(TS::makeParameter("longitude"));
	parameters.push_back(TS::makeParameter("latitude"));
	parameters.push_back(TS::makeParameter("ww_aws"));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gettings parameters for finnish road observations failed!");
  }
}

StationSymbolPriorities PresentWeatherObservationLayer::processResultSet(const State& theState, const ResultSet& theResultSet, const boost::posix_time::ptime& requested_timestep) const
{
  try
  {
	auto zone = theState.getGeoEngine().getTimeZones().time_zone_from_string("UTC");
	auto requested_local_time = boost::local_time::local_date_time(requested_timestep, zone);
	
	// Transformation object
	auto transformation = LonLatToXYTransformation(projection);
	
	StationSymbolPriorities ssps;
	TS::Value none = TS::None();  
	for(auto& result_set_item : theResultSet)
	  {
		StationSymbolPriority ssp;
		ssp.fmisid = result_set_item.first;
		// FMISID: data independent
		const auto& longitude_result_set_vector = result_set_item.second.at(1);
		if(longitude_result_set_vector.size() == 0)
		  continue;
		auto lon = get_double(longitude_result_set_vector.at(0).value);
		// Latitude: data independent
		const auto& latitude_result_set_vector = result_set_item.second.at(2);
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

		const auto& wawa_result_set_vector = result_set_item.second.at(3);
		auto wawa_val = get_double(wawa_result_set_vector.at(0).value);

		// Symbol
		ssp.symbol = get_symbol(wawa_val);
		// Priority
		ssp.priority = get_symbol_priority(ssp.symbol);

		ssps.push_back(ssp);
	  } 
	
	// Sort according to priority
	std::sort(ssps.begin(), ssps.end(), priority_sort);
	
	return ssps;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Processing result set of wawa failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION