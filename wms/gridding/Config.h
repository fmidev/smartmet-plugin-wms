// ======================================================================
/*!
 * \brief Gridded observations configuration
 */
// ======================================================================

#pragma once

#include <gis/PointGridding.h>
#include <json/json.h>
#include <newbase/NFmiParameterName.h>
#include <cstddef>
#include <libconfig.h++>
#include <map>
#include <memory>
#include <optional>
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
// A background such as a forecast which the observations correct. The newest model run of
// the producer containing the time is used, and the background is interpolated to the grid.
struct BackgroundConfig
{
  std::string producer;                           // gridded querydata producer
  FmiParameterName parameter = kFmiBadParameter;  // the background parameter
  // The model orography in metres. Without it the model is assumed to see the terrain of the
  // gridding area, and the background is not moved to the station heights.
  FmiParameterName orography = kFmiBadParameter;
};

// The index of the sea fraction among the terrain covariates of an area
constexpr std::size_t kSeaFraction = 0;

// One gridded parameter of a producer
struct ParameterConfig
{
  std::string name;                        // newbase name of the gridded parameter
  FmiParameterName id = kFmiBadParameter;  // and its number
  std::string source;                      // name of the parameter at the source
  Fmi::PointGridding::Options options;
  std::optional<BackgroundConfig> background;
};

enum class SourceType
{
  Observations,  // observation engine
  Querydata      // point querydata from the querydata engine
};

// A gridding area: the grid, its terrain and the station elevations of point querydata. The
// areas are set up when the plugin starts. A producer defined in the configuration file has an
// area of its own, named after the producer.
struct AreaConfig
{
  std::string name;

  // Terrain heights (m) as a north-up raster with square cells. The raster defines the grid
  // and its coordinate system, which should be equal-area.
  std::string dem;
  double tpiSigma = 10e3;  // m

  // Optional raster of the fraction of sea (0-1) in any coordinate system and resolution. It
  // is averaged onto the grid, smoothed with a Gaussian of seaSigma and used as an additional
  // trend predictor: the terrain covariate kSeaFraction of the area.
  std::string seaFraction;
  double seaSigma = 10e3;  // m

  // Optional CSV file of station elevations for point querydata, which has none: lines of
  // "id,elevation" where id is the station number of the querydata. Stations without an
  // elevation get the height of the terrain.
  std::string stationElevations;

  // Optional observation engine station type whose station metadata gives the elevations of
  // point querydata stations by WMO number. The file above takes precedence.
  std::string elevationStationType;
};

// One gridded producer: an area, a source of observations and parameters
struct ProducerConfig
{
  std::string name;  // as requested: "obs_europe" or "obs_europe@variant"
  std::string area;  // the name of the area
  SourceType source = SourceType::Observations;
  std::string sourceProducer;  // observation engine station type or querydata producer

  // Identifies the settings that determine the gridded values, for the cache: the name of a
  // producer of the configuration file, the resolved definition of a definition file. The
  // default variant and the variant it names have the same hash.
  std::size_t hash = 0;

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

// The producers of one definition file: the default and every named variant
struct Definition
{
  std::shared_ptr<const ProducerConfig> defaultVariant;
  std::map<std::string, std::shared_ptr<const ProducerConfig>> variants;
};

class Config
{
 public:
  Config() = delete;
  // Parse the gridding settings group of the plugin configuration
  explicit Config(const libconfig::Setting& theSetting);

  // The producers of the configuration file
  const std::map<std::string, ProducerConfig>& producers() const { return itsProducers; }

  // The areas, also those of the producers of the configuration file
  const std::map<std::string, AreaConfig>& areas() const { return itsAreas; }

  // The directory of the definition files <name>.json, empty if none
  const std::string& directory() const { return itsDirectory; }

  // Limits of the definition files
  std::size_t maxParameters() const { return itsMaxParameters; }
  const std::vector<std::string>& allowedSources() const { return itsAllowedSources; }

  // Parse a definition file of the directory (strict: unknown settings are errors)
  Definition parseDefinition(const Json::Value& theJson, const std::string& theName) const;

  // Number of gridded datasets kept in memory
  std::size_t cacheSize() const { return itsCacheSize; }

  // Threads used for gridding one dataset, 0 for all cores
  unsigned int threads() const { return itsThreads; }

  // Datasets gridded at the same time, further griddings wait for their turn
  unsigned int maxConcurrent() const { return itsMaxConcurrent; }

  // Seconds an observation read is reused by other requests for the same time, 0 to disable
  unsigned int observationCacheSeconds() const { return itsObservationCacheSeconds; }

 private:
  void parseProducer(const libconfig::Setting& theSetting);
  void parseArea(const libconfig::Setting& theSetting);

  std::map<std::string, ProducerConfig> itsProducers;
  std::map<std::string, AreaConfig> itsAreas;
  std::string itsDirectory;
  std::size_t itsMaxParameters = 20;
  std::vector<std::string> itsAllowedSources;
  std::size_t itsCacheSize = 50;
  unsigned int itsThreads = 0;
  unsigned int itsMaxConcurrent = 10;
  unsigned int itsObservationCacheSeconds = 10;
};

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
