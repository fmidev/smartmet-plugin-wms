// ======================================================================
/*!
 * \brief Gridded observations configuration
 */
// ======================================================================

#pragma once

#include <gis/PointGridding.h>
#include <newbase/NFmiParameterName.h>
#include <libconfig.h++>
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
// One gridded parameter of a producer
struct ParameterConfig
{
  std::string name;                        // newbase name of the gridded parameter
  FmiParameterName id = kFmiBadParameter;  // and its number
  std::string source;                      // name of the parameter at the source
  Fmi::PointGridding::Options options;
};

enum class SourceType
{
  Observations,  // observation engine
  Querydata      // point querydata from the querydata engine
};

// One gridded producer: an area, a source of observations and parameters
struct ProducerConfig
{
  std::string name;
  SourceType source = SourceType::Observations;
  std::string sourceProducer;  // observation engine station type or querydata producer

  // Terrain heights (m) as a north-up raster with square cells. The raster defines the grid
  // and its coordinate system, which should be equal-area.
  std::string dem;
  double tpiSigma = 10e3;  // m

  // Optional CSV file of station elevations for point querydata, which has none: lines of
  // "id,elevation" where id is the station number of the querydata. Stations without an
  // elevation get the height of the terrain.
  std::string stationElevations;

  // Optional observation engine station type whose station metadata gives the elevations of
  // point querydata stations by WMO number. The file above takes precedence.
  std::string elevationStationType;

  // Times offered for observations: every timestep minutes up to the current time minus
  // delay minutes, history minutes back. Point querydata offers its own times.
  int timestep = 60;
  int delay = 10;
  int history = 24 * 60;

  // Seconds an observation may be away from the requested time
  int timeTolerance = 0;

  // Maximum number of times gridded for one request of a time period
  int maxTimes = 24;

  std::vector<ParameterConfig> parameters;
};

class Config
{
 public:
  Config() = delete;
  // Parse the gridding settings group of the plugin configuration
  explicit Config(const libconfig::Setting& theSetting);

  const std::map<std::string, ProducerConfig>& producers() const { return itsProducers; }

  // Number of gridded datasets kept in memory
  std::size_t cacheSize() const { return itsCacheSize; }

  // Threads used for gridding one dataset, 0 for all cores
  unsigned int threads() const { return itsThreads; }

 private:
  void parseProducer(const libconfig::Setting& theSetting);

  std::map<std::string, ProducerConfig> itsProducers;
  std::size_t itsCacheSize = 50;
  unsigned int itsThreads = 0;
};

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
