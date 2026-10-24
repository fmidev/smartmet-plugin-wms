// ======================================================================
/*!
 * \brief Gridded observations
 *
 * Interpolates point observations onto a fixed grid per area (Europe, Africa...) with the help
 * of the terrain, see Fmi::PointGridding of the gis library. The result is gridded querydata,
 * so the isoline, isoband and isolabel layers draw it like any other querydata, and WMS, WMTS
 * and OGC Tiles (including GeoTIFF tiles) serve it. A whole area is gridded at once and shared
 * by all requests for the same time, which keeps isobands seamless across map tiles.
 *
 * The observations come from the observation engine or from point querydata served by the
 * querydata engine.
 */
// ======================================================================

#pragma once

#include "Area.h"
#include "Config.h"
#include "Sources.h"
#include <engines/querydata/Model.h>
#include <engines/querydata/Q.h>
#include <macgyver/Cache.h>
#include <macgyver/DateTime.h>
#include <chrono>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Engine
{
namespace Observation
{
class Engine;
}  // namespace Observation
}  // namespace Engine

namespace Plugin
{
namespace Dali
{
namespace Gridding
{
class Gridder
{
 public:
  // The observation engine may be null if no producer reads observations
  Gridder(const libconfig::Setting& theSetting,
          const Engine::Querydata::Engine* theQEngine,
          Engine::Observation::Engine* theObsEngine);

  Gridder() = delete;
  Gridder(const Gridder&) = delete;
  Gridder& operator=(const Gridder&) = delete;
  Gridder(Gridder&&) = delete;
  Gridder& operator=(Gridder&&) = delete;
  ~Gridder();

  // The gridded producers, sorted
  std::vector<std::string> producers() const;

  // Whether the name is a gridded producer
  bool hasProducer(const std::string& theProducer) const;

  // The newbase names of the parameters of a producer
  std::vector<std::string> parameters(const std::string& theProducer) const;

  // WGS84 bounding box of the grid with keys minx, miny, maxx, maxy
  std::map<std::string, double> boundingBox(const std::string& theProducer) const;

  // The times offered for gridding, sorted. For observations these are the recent times (the
  // history setting), but any time can be gridded.
  std::vector<Fmi::DateTime> validTimes(const std::string& theProducer) const;

  // Gridded data for one time with all the parameters of the producer. Every call returns a
  // new Q, they must not be shared between threads.
  Engine::Querydata::Q get(const std::string& theProducer, const Fmi::DateTime& theTime) const;

  // Hash value of the data get() would return, for ETag calculation. The observations are
  // read, but they are not gridded.
  std::size_t hashValue(const std::string& theProducer, const Fmi::DateTime& theTime) const;

  Fmi::Cache::CacheStatistics getCacheStats() const;

 private:
  // The background models of the parameters, null where there is none
  using Backgrounds = std::vector<Engine::Querydata::Q>;
  using SharedStationData = std::shared_ptr<const StationData>;

  const ProducerConfig& producer(const std::string& theProducer) const;
  SharedStationData read(const ProducerConfig& theProducer, const Fmi::DateTime& theTime) const;
  Backgrounds backgrounds(const ProducerConfig& theProducer, const Fmi::DateTime& theTime) const;
  std::size_t dataHash(const ProducerConfig& theProducer,
                       const Fmi::DateTime& theTime,
                       const StationData& theData,
                       const Backgrounds& theBackgrounds) const;
  Engine::Querydata::SharedModel grid(const ProducerConfig& theProducer,
                                      const Fmi::DateTime& theTime,
                                      const StationData& theData,
                                      const Backgrounds& theBackgrounds) const;

  Config itsConfig;
  std::map<std::string, std::unique_ptr<Area>> itsAreas;
  std::map<std::string, StationElevations> itsStationElevations;

  const Engine::Querydata::Engine* itsQEngine = nullptr;
  Engine::Observation::Engine* itsObsEngine = nullptr;

  // Finished datasets, and the ones being gridded so that simultaneous requests for the same
  // dataset (map tiles) wait for one gridding instead of starting their own. The models are
  // cached instead of Q objects, since a Q holds an iterator to the data and must not be
  // shared between threads.
  mutable Fmi::Cache::Cache<std::size_t, Engine::Querydata::SharedModel> itsCache;
  mutable std::mutex itsPendingMutex;
  mutable std::map<std::size_t, std::shared_future<Engine::Querydata::SharedModel>> itsPending;

  // Recent observation reads by producer and time, so that the requests for the map tiles of
  // a time read the observations once (observation_cache_seconds)
  struct RecentRead
  {
    std::chrono::steady_clock::time_point time;
    std::shared_future<SharedStationData> data;
  };
  mutable std::mutex itsRecentReadsMutex;
  mutable std::map<std::string, RecentRead> itsRecentReads;

  // Number of griddings running, limited to max_concurrent
  mutable std::mutex itsRunningMutex;
  mutable std::condition_variable itsRunningCondition;
  mutable unsigned int itsRunning = 0;
};

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
