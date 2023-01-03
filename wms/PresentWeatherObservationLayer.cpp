#ifndef WITHOUT_OBSERVATION

#include "PresentWeatherObservationLayer.h"
#include "Config.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include "ValueTools.h"
#include <timeseries/ParameterTools.h>
#include <array>

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

float convert_wawa_2_ww(float theValue)
{
  // clang-format off
  // See https://hav.fmi.fi/hav/havainnot/index.php?page=wcodes&mode=wawa_to_ww
  std::array<float, 100> wwCodeArray = {
      0,  1,  2,  3,  4,  5,  0,  0,  0,  0,
      10, 0,  13, 0,  0,  0,  0,  0,  18, 0,
      28, 21, 20, 21, 22, 24, 29, 0,  0,  0,
      42, 41, 43, 45, 47, 48, 0,  0,  0,  0,
      61, 63, 65, 61, 65, 71, 75, 66, 67, 0,
      50, 51, 53, 55, 56, 57, 57, 58, 59, 0,
      60, 61, 63, 65, 66, 67, 67, 68, 69, 0,
      70, 71, 73, 75, 79, 79, 79, 77, 78, 0,
      80, 80, 81, 81, 82, 85, 86, 86, 0, 89,
      92, 17, 93, 96, 17, 97, 99, 0,  0,  8};
  // clang-format on

  if (theValue <= 99)
    return wwCodeArray[static_cast<int>(theValue)];
  return theValue;
}

}  // namespace

void PresentWeatherObservationLayer::getParameters(
    const boost::posix_time::ptime& requested_timestep,
    std::vector<Spine::Parameter>& parameters,
    boost::posix_time::ptime& starttime,
    boost::posix_time::ptime& endtime) const
{
  try
  {
    endtime = requested_timestep;
    starttime = requested_timestep;

    parameters.push_back(TS::makeParameter("fmisid"));
    parameters.push_back(TS::makeParameter("stationlongitude"));
    parameters.push_back(TS::makeParameter("stationlatitude"));
    parameters.push_back(TS::makeParameter("ww_aws"));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gettings parameters for finnish road observations failed!");
  }
}

StationSymbolPriorities PresentWeatherObservationLayer::processResultSet(
    const State& theState,
    const ResultSet& theResultSet,
    const boost::posix_time::ptime& requested_timestep) const
{
  try
  {
    auto zone = theState.getGeoEngine().getTimeZones().time_zone_from_string("UTC");
    auto requested_local_time = boost::local_time::local_date_time(requested_timestep, zone);

    // Transformation object
    auto transformation = LonLatToXYTransformation(projection);

    StationSymbolPriorities ssps;
    TS::Value none = TS::None();
    for (const auto& result_set_item : theResultSet)
    {
      const auto& wawa_result_set_vector = result_set_item.second.at(3);
      auto wawa_val = get_double(wawa_result_set_vector.at(0).value);

      // Symbol
      StationSymbolPriority ssp;
      ssp.symbol = get_symbol(wawa_val);
      if (ssp.symbol <= 0)
        continue;

      // Priority
      ssp.priority = get_symbol_priority(ssp.symbol);

      ssp.fmisid = result_set_item.first;
      // FMISID: data independent
      const auto& longitude_result_set_vector = result_set_item.second.at(1);
      if (longitude_result_set_vector.empty())
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
        throw Fmi::Exception::Trace(
            BCP, "Transforming longitude, latitude to screen coordinates failed!");
      }

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

int PresentWeatherObservationLayer::get_symbol(float wawa) const
{
  float value = convert_wawa_2_ww(wawa);
  if (value != kFloatMissing && value > 3)
  {
    // the last value 255 is synop value 98, luckily also 99 is available
    // but at location 48
    if (value == 99)
      return 48;

    return (157 + value);
  }
  return missing_symbol;  // Missing symbol 'Z'
}

int PresentWeatherObservationLayer::get_symbol_priority(int symbol) const
{
  if (symbol == missing_symbol)
    return 0;

  switch (symbol)
  {
    case 170:
    case 171:
    case 172:
    case 173:
      return 1;
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
  }

  return 0;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
