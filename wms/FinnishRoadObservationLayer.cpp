#ifndef WITHOUT_OBSERVATION

#include "FinnishRoadObservationLayer.h"
#include "LonLatToXYTransformation.h"
#include "PointData.h"
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

std::vector<std::string> FinnishRoadObservationLayer::getParameters() const
{
  return {
      "fmisid",
      "stationlongitude",
      "stationlatitude",
      "ILMA",
      "SADE",
      "RST",
  };
}

StationSymbolPriorities FinnishRoadObservationLayer::processResultSet(
    const std::vector<PointData>& theResultSet) const
{
  try
  {
    // Transformation object
    auto transformation = LonLatToXYTransformation(projection);

    StationSymbolPriorities ssps;

    for (const auto& item : theResultSet)
    {
      auto fmisid = item[0];
      auto lon = item[1];
      auto lat = item[2];
      auto t2m = item[3];
      auto rain = item[4];
      auto rform = item[5];

      if (lon == kFloatMissing || lat == kFloatMissing || t2m == kFloatMissing ||
          rain == kFloatMissing || rform == kFloatMissing)
        continue;

      StationSymbolPriority ssp;
      ssp.fmisid = fmisid;

      // Transform to screen coordinates
      if (transformation.transform(lon, lat))
      {
        ssp.longitude = lon;
        ssp.latitude = lat;
      }
      else
      {
        throw Fmi::Exception::Trace(
            BCP, "Transforming longitude, latitude to screen coordinates failed!")
            .addParameter("fmisid", Fmi::to_string(ssp.fmisid))
            .addParameter("lon", Fmi::to_string(lon))
            .addParameter("lat", Fmi::to_string(lat));
      }

      // Symbol
      ssp.symbol = get_symbol(rain, rform);
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

/*--------------------------------------------------------------------------
 * r: 0 pouta, 1 heikko, 2 kohtalainen, 3 runsas, 4 heikko lumisade,
 *    5 kohtalainen lumisade, 6 runsas lumisade
 *--------------------------------------------------------------------------
 * rform: 7 pouta, 8 hyvin heikko, 9 tihkusade, 10 vesisade, 11 lumisade,
 *        12 märkä räntä, 13 räntäsade, 14 rakeita, 15 jääkiteitä,
 *        16 lumijyväsiä, 17 lumirakeita, 18 jäätävä tihku, 19 jäätävä sade
 ---------------------------------------------------------------------------*/

int FinnishRoadObservationLayer::get_symbol(int r, int rform) const
{
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
