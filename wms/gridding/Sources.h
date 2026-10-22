// ======================================================================
/*!
 * \brief Reading the observations to be gridded
 */
// ======================================================================

#pragma once

#include "Config.h"
#include <engines/querydata/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <macgyver/DateTime.h>
#include <map>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace Gridding
{
// Observations of all the parameters of a producer at one time
struct StationData
{
  std::vector<double> lon;
  std::vector<double> lat;
  std::vector<double> elevation;  // NaN if unknown
  // values[k][i] is parameter k of station i, NaN if missing
  std::vector<std::vector<double>> values;

  std::size_t size() const { return lon.size(); }
  std::size_t hashValue() const;
};

// Station elevations for point querydata by station number
using StationElevations = std::map<long, double>;

StationElevations read_station_elevations(const std::string& theFile);

#ifndef WITHOUT_OBSERVATION
// Elevations of the stations of a station type in the bounding box by WMO number
StationElevations read_wmo_elevations(const Engine::Observation::Engine& theEngine,
                                      const std::string& theStationType,
                                      const std::map<std::string, double>& theBBox);

StationData read_observations(const Engine::Observation::Engine& theEngine,
                              const ProducerConfig& theProducer,
                              const std::map<std::string, double>& theBBox,
                              const Fmi::DateTime& theTime);

#endif

StationData read_querydata(const Engine::Querydata::Engine& theEngine,
                           const ProducerConfig& theProducer,
                           const StationElevations& theElevations,
                           const Fmi::DateTime& theTime);

std::vector<Fmi::DateTime> querydata_times(const Engine::Querydata::Engine& theEngine,
                                           const ProducerConfig& theProducer);

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
