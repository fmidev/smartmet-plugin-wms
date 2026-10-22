// ======================================================================
/*!
 * \brief Reading the observations to be gridded
 */
// ======================================================================

#include "Sources.h"
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiEnumConverter.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <timeseries/ParameterTools.h>
#include <cmath>
#include <fstream>
#include <limits>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Gridding
{
namespace
{
const double missing = std::numeric_limits<double>::quiet_NaN();

double to_double(const TS::Value& theValue)
{
  if (const auto* d = std::get_if<double>(&theValue))
    return *d;
  if (const auto* i = std::get_if<int>(&theValue))
    return *i;
  return missing;
}
}  // namespace

std::size_t StationData::hashValue() const
{
  std::size_t hash = Fmi::hash_value(lon);
  Fmi::hash_combine(hash, Fmi::hash_value(lat));
  Fmi::hash_combine(hash, Fmi::hash_value(elevation));
  for (const auto& v : values)
    Fmi::hash_combine(hash, Fmi::hash_value(v));
  return hash;
}

// ----------------------------------------------------------------------
/*!
 * \brief Read "id,elevation" lines, ignoring empty lines and # comments
 */
// ----------------------------------------------------------------------

StationElevations read_station_elevations(const std::string& theFile)
{
  try
  {
    StationElevations result;
    if (theFile.empty())
      return result;

    std::ifstream in(theFile);
    if (!in)
      throw Fmi::Exception(BCP, "Failed to open station elevation file");

    std::string line;
    int lineno = 0;
    while (std::getline(in, line))
    {
      ++lineno;
      if (line.empty() || line[0] == '#')
        continue;
      const auto comma = line.find(',');
      if (comma == std::string::npos)
        throw Fmi::Exception(BCP, "Expected 'id,elevation'")
            .addParameter("Line", std::to_string(lineno));
      result[Fmi::stol(line.substr(0, comma))] = Fmi::stod(line.substr(comma + 1));
    }
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to read station elevations")
        .addParameter("File", theFile);
  }
}

#ifndef WITHOUT_OBSERVATION
// ----------------------------------------------------------------------
/*!
 * \brief Station elevations by WMO number from the observation engine station metadata
 */
// ----------------------------------------------------------------------

StationElevations read_wmo_elevations(const Engine::Observation::Engine& theEngine,
                                      const std::string& theStationType,
                                      const std::map<std::string, double>& theBBox)
{
  try
  {
    auto& engine = const_cast<Engine::Observation::Engine&>(theEngine);
    Engine::Observation::Settings settings;
    settings.stationtype = theStationType;
    settings.boundingBox = theBBox;
    settings.starttime = Fmi::SecondClock::universal_time();
    settings.endtime = settings.starttime;

    Spine::Stations stations;
    engine.getStationsByBoundingBox(stations, settings);

    StationElevations result;
    for (const auto& station : stations)
      if (station.wmo > 0)
        result[station.wmo] = station.elevation;
    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP,
                                "Failed to read station elevations from the observation engine")
        .addParameter("Station type", theStationType);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read observations from the observation engine
 *
 * All stations of the station type inside the bounding box are read. With a time tolerance
 * the observation closest to the requested time is used for each station.
 */
// ----------------------------------------------------------------------

StationData read_observations(const Engine::Observation::Engine& theEngine,
                              const ProducerConfig& theProducer,
                              const std::map<std::string, double>& theBBox,
                              const Fmi::DateTime& theTime)
{
  try
  {
    auto& engine = const_cast<Engine::Observation::Engine&>(theEngine);

    Engine::Observation::Settings settings;
    settings.stationtype = theProducer.sourceProducer;
    settings.timezone = "UTC";
    settings.starttimeGiven = true;
    settings.starttime = theTime - Fmi::Seconds(theProducer.timeTolerance);
    settings.endtime = theTime + Fmi::Seconds(theProducer.timeTolerance);
    settings.wantedtime = theTime;

    const std::size_t nparams = theProducer.parameters.size();
    for (const auto& p : theProducer.parameters)
      settings.parameters.push_back(TS::makeParameter(p.source));
    const std::size_t fmisid_col = settings.parameters.size();
    for (const char* name : {"fmisid", "stationlon", "stationlat", "elevation"})
      settings.parameters.push_back(TS::makeParameter(name));

    Engine::Observation::StationSettings stations;
    stations.bounding_box_settings = theBBox;
    settings.taggedFMISIDs = engine.translateToFMISID(settings, stations);

    StationData data;
    data.values.resize(nparams);
    if (settings.taggedFMISIDs.empty())
      return data;

    auto result = engine.values(settings);
    if (!result || result->empty())
      return data;
    const auto& columns = *result;

    // The row closest to the requested time for each station
    std::map<int, std::pair<long, std::size_t>> best;  // fmisid -> (seconds off, row)
    const std::size_t nrows = columns.at(fmisid_col).size();
    for (std::size_t row = 0; row < nrows; row++)
    {
      const double id = to_double(columns[fmisid_col][row].value);
      if (!std::isfinite(id))
        continue;
      const auto off =
          std::abs((columns[fmisid_col][row].time.utc_time() - theTime).total_seconds());
      auto pos = best.find(static_cast<int>(id));
      if (pos == best.end() || off < pos->second.first)
        best[static_cast<int>(id)] = std::make_pair(off, row);
    }

    for (const auto& item : best)
    {
      const std::size_t row = item.second.second;
      data.lon.push_back(to_double(columns[fmisid_col + 1][row].value));
      data.lat.push_back(to_double(columns[fmisid_col + 2][row].value));
      data.elevation.push_back(to_double(columns[fmisid_col + 3][row].value));
      for (std::size_t k = 0; k < nparams; k++)
        data.values[k].push_back(to_double(columns[k][row].value));
    }
    return data;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to read observations for gridding")
        .addParameter("Producer", theProducer.name)
        .addParameter("Time", Fmi::to_iso_string(theTime));
  }
}

#endif

// ----------------------------------------------------------------------
/*!
 * \brief Read observations from point querydata
 *
 * Point querydata has no station elevations, they come from the optional station file.
 */
// ----------------------------------------------------------------------

StationData read_querydata(const Engine::Querydata::Engine& theEngine,
                           const ProducerConfig& theProducer,
                           const StationElevations& theElevations,
                           const Fmi::DateTime& theTime)
{
  try
  {
    auto q = theEngine.get(theProducer.sourceProducer);
    auto info = q->info();
    if (info->IsGrid())
      throw Fmi::Exception(BCP, "The source querydata is gridded, not point data");

    StationData data;
    const std::size_t nparams = theProducer.parameters.size();
    data.values.resize(nparams);

    if (!info->Time(NFmiMetTime(theTime)))
      return data;

    std::vector<FmiParameterName> ids;
    for (const auto& p : theProducer.parameters)
    {
      static NFmiEnumConverter converter;  // lookups do not modify it
      ids.push_back(static_cast<FmiParameterName>(converter.ToEnum(p.source)));
    }

    info->FirstLevel();
    for (info->ResetLocation(); info->NextLocation();)
    {
      const auto* loc = info->Location();
      data.lon.push_back(loc->GetLongitude());
      data.lat.push_back(loc->GetLatitude());
      const auto pos = theElevations.find(loc->GetIdent());
      data.elevation.push_back(pos != theElevations.end() ? pos->second : missing);

      for (std::size_t k = 0; k < nparams; k++)
      {
        double value = missing;
        if (ids[k] != kFmiBadParameter && info->Param(ids[k]))
        {
          const float v = info->FloatValue();
          if (v != kFloatMissing)
            value = v;
        }
        data.values[k].push_back(value);
      }
    }
    return data;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to read point querydata for gridding")
        .addParameter("Producer", theProducer.name)
        .addParameter("Time", Fmi::to_iso_string(theTime));
  }
}

std::vector<Fmi::DateTime> querydata_times(const Engine::Querydata::Engine& theEngine,
                                           const ProducerConfig& theProducer)
{
  auto q = theEngine.get(theProducer.sourceProducer);
  const auto times = q->validTimes();
  return {times->begin(), times->end()};
}

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
