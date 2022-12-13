#ifndef WITHOUT_OBSERVATION

#include "FinnishRoadObservationLayer.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include "ValueTools.h"
#include <macgyver/StringConversion.h>
#include <timeseries/ParameterTools.h>
#include <timeseries/TimeSeriesInclude.h>

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

}  // namespace

void FinnishRoadObservationLayer::getParameters(const boost::posix_time::ptime& requested_timestep,
                                                std::vector<SmartMet::Spine::Parameter>& parameters,
                                                boost::posix_time::ptime& starttime,
                                                boost::posix_time::ptime& endtime) const
{
  try
  {
    endtime = requested_timestep;
    starttime = (endtime - boost::posix_time::hours(
                               24));  //  // We must calculate mean temperature from last 24 hours

    parameters.push_back(TS::makeParameter("fmisid"));
    parameters.push_back(TS::makeParameter("stationlongitude"));
    parameters.push_back(TS::makeParameter("stationlatitude"));
    parameters.push_back(TS::makeParameter("ILMA"));
    parameters.push_back(TS::makeParameter("SADE"));
    parameters.push_back(TS::makeParameter("RST"));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gettings parameters for finnish road observations failed!");
  }
}

StationSymbolPriorities FinnishRoadObservationLayer::processResultSet(
    const State& theState,
    const ResultSet& theResultSet,
    const boost::posix_time::ptime& requested_timestep) const
{
  try
  {
    auto zone = theState.getGeoEngine().getTimeZones().time_zone_from_string("UTC");
    auto requested_local_time = boost::local_time::local_date_time(requested_timestep, zone);

    // Do the aggregation and transform longitude/latitude to screen coordinates

    // Functions for aggregation
    TS::DataFunction ilma_func(TS::FunctionId::Mean, TS::FunctionType::TimeFunction);
    ilma_func.setAggregationIntervalBehind(1440);  // 1440 minutes = 24h
    ilma_func.setAggregationIntervalAhead(0);
    TS::DataFunction sade_rst_func(TS::FunctionId::Nearest, TS::FunctionType::TimeFunction);
    sade_rst_func.setAggregationIntervalBehind(20);  // 20 minutes behind
    sade_rst_func.setAggregationIntervalAhead(20);   // 20 minutes ahead
    // Transformation object
    auto transformation = LonLatToXYTransformation(projection);

    StationSymbolPriorities ssps;
    TS::Value none = TS::None();
    for (const auto& result_set_item : theResultSet)
    {
      StationSymbolPriority ssp;
      ssp.fmisid = result_set_item.first;
      // FMISID: data independent
      const auto& longitude_result_set_vector = result_set_item.second.at(1);
      if (longitude_result_set_vector.empty())
        continue;
      const auto lon = get_double(longitude_result_set_vector.at(0).value);
      // Latitude: data independent
      const auto& latitude_result_set_vector = result_set_item.second.at(2);
      const auto lat = get_double(latitude_result_set_vector.at(0).value);
      // Transform to screen coordinates
      double x = lon;
      double y = lat;
      if (transformation.transform(x, y))
      {
        ssp.longitude = x;
        ssp.latitude = y;
      }
      else
      {
        throw Fmi::Exception::Trace(
            BCP, "Transforming longitude, latitude to screen coordinates failed!")
            .addParameter("fmisid", Fmi::to_string(ssp.fmisid))
            .addParameter("lon", Fmi::to_string(lon))
            .addParameter("lat", Fmi::to_string(lat));
      }
      // ILMA: get mean of previous 24 hours
      const auto& ilma_result_set_vector = result_set_item.second.at(3);
      auto agg_val =
          TS::Aggregator::time_aggregate(ilma_result_set_vector, ilma_func, requested_local_time);
      if (agg_val.value == none)
      {
        continue;
      }
      auto t2m = get_double(agg_val.value);
      // SADE: get nearest value of previous or next 20 minutes
      const auto& sade_result_set_vector = result_set_item.second.at(4);
      auto sade = TS::Aggregator::time_aggregate(
          sade_result_set_vector, sade_rst_func, requested_local_time);
      // RST: get nearest value of previous or next 20 minutes
      const auto& rst_result_set_vector = result_set_item.second.at(5);
      auto rst = TS::Aggregator::time_aggregate(
          rst_result_set_vector, sade_rst_func, requested_local_time);

      if (get_double(sade.value) == kFloatMissing || get_double(rst.value) == kFloatMissing)
      {
        continue;
      }

      // Symbol
      ssp.symbol = get_symbol(get_double(sade.value), get_double(rst.value));
      if (ssp.symbol <= 0)
        continue;

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

int FinnishRoadObservationLayer::get_symbol(int r, int rform) const
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
    if (rform == 9)
      return 51;  // drizzle, not freezing, intermittent, slight
    if (rform == 10)
      return 52;  // rain, not freezing, intermittent, slight
    if (rform == 11)
      return 53;  // intermittent fall of snowflakes, slight
    if (rform == 18)
      return 213;  // drizzle, freezing, slight
    if (rform == 19)
      return 223;  // rain, freezing, slight
  }
  if (r == 2)
  {
    if (rform == 9)
      return 209;  // drizzle, not freezing, intermittent, moderate
    if (rform == 10)
      return 219;  // rain, not freezing, intermittent, moderate
    if (rform == 11)
      return 229;  // intermittent fall of snowflakes, moderate
    if (rform == 18)
      return 214;  // drizzle, freezing, moderate or heavy
    if (rform == 19)
      return 224;  // rain, freezing, moderate or heavy
  }
  if (r == 3)
  {
    if (rform == 9)
      return 212;  // drizzle, not freezing, continous, heavy
    if (rform == 10)
      return 222;  // rain, not freezing, continous, heavy
    if (rform == 11)
      return 232;  // continous flall of snowflakes, heavy
    if (rform == 18)
      return 214;  // drizzle, freezing, moderate or heavy
    if (rform == 19)
      return 224;  // rain, freezing, moderate or heavy
  }
  if (rform == 13)
    return 225;  // rain or drizzle and snow, slight
  if (rform == 14)
    return 244;  // shower(s) of snow pellets or snow hail
  if (rform == 15)
    return 233;  // ice needles (with or without fog)
  if (rform == 16)
    return 234;  // snow grains (with or without fog)
  if (rform == 17)
    return 236;  // ice pellets (sleet)

  // r 4,5,6 are redundant but re-check in case there is no match in the the cases above
  if (r == 4)
    return 53;  // intermittent fall of snowflakes, slight
  if (r == 5)
    return 229;  // intermittent fall of snowflakes, moderate
  if (r == 6)
    return 232;  // continous flall of snowflakes, heavy

  return missing_symbol;
}

int FinnishRoadObservationLayer::get_symbol_priority(int symbol, double t2m) const
{
  if (symbol == missing_symbol)
    return 0;

  if (t2m <= 0)
  {
    // WINTER: t2m < 0
    switch (symbol)
    {
      case 53:
      case 229:
        return 1;
      case 232:
      case 233:
      case 234:
      case 236:
      case 244:
        return 2;
      case 225:
        return 5;
      case 51:
      case 213:
        return 6;
      case 209:
        return 7;
      case 52:
      case 212:
      case 214:
      case 223:
        return 8;
      case 219:
        return 9;
      case 222:
      case 224:
        return 10;
    }
  }
  else if (t2m < 10)
  {
    // SPRING OR AUTUMN: 0 > t2m < 10
    switch (symbol)
    {
      case 51:
      case 52:
      case 53:
      case 213:
      case 223:
      case 225:
        return 1;  // slight
      case 209:
      case 219:
      case 229:
      case 233:
      case 234:
      case 236:
      case 244:
        return 2;  // moderate
      case 212:
      case 214:
      case 222:
      case 224:
      case 232:
        return 4;  // heavy
    }
  }
  else
  {
    // SUMMER: t2m >= 10
    switch (symbol)
    {
      case 51:
      case 52:
        return 0;
      case 209:
      case 219:
        return 1;
      case 212:
      case 222:
        return 2;
      case 213:
        return 3;
      case 214:
        return 4;
      case 225:
        return 5;
      case 53:
      case 223:
        return 6;
      case 224:
      case 229:
        return 7;
      case 232:
        return 8;
      case 233:
      case 234:
      case 236:
      case 244:
        return 9;
    }
  }

  return 0;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
