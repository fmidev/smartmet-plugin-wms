// ======================================================================
/*!
 * \brief Gridded observations
 */
// ======================================================================

#include "Gridder.h"
#include <engines/observation/Engine.h>
#include <engines/querydata/Engine.h>
#include <engines/querydata/Model.h>
#include <fmt/format.h>
#include <gis/SpatialReference.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiProducerName.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>
#include <spine/Convenience.h>
#include <algorithm>
#include <cmath>
#include <cpl_error.h>
#include <iostream>
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
// Waits until fewer than max griddings run, and holds a place until destroyed
class RunningSlot
{
 public:
  RunningSlot(std::mutex& theMutex,
              std::condition_variable& theCondition,
              unsigned int& theRunning,
              unsigned int theMax)
      : itsMutex(theMutex), itsCondition(theCondition), itsRunning(theRunning)
  {
    std::unique_lock<std::mutex> lock(itsMutex);
    itsCondition.wait(lock, [&] { return itsRunning < theMax; });
    ++itsRunning;
  }

  ~RunningSlot()
  {
    {
      std::lock_guard<std::mutex> lock(itsMutex);
      --itsRunning;
    }
    itsCondition.notify_one();
  }

  RunningSlot() = delete;
  RunningSlot(const RunningSlot&) = delete;
  RunningSlot& operator=(const RunningSlot&) = delete;
  RunningSlot(RunningSlot&&) = delete;
  RunningSlot& operator=(RunningSlot&&) = delete;

 private:
  std::mutex& itsMutex;
  std::condition_variable& itsCondition;
  unsigned int& itsRunning;
};

// Gridding options use the configured thread count unless the parameter sets its own
Fmi::PointGridding::Options with_threads(Fmi::PointGridding::Options options, unsigned int threads)
{
  if (options.threads == 0)
    options.threads = threads;
  return options;
}

// ----------------------------------------------------------------------
/*!
 * \brief Create empty querydata on the area grid with the parameters of the producer
 *
 * Querydata grid points lie on the edges of the area, hence the area spans from the first
 * cell centre to the last one.
 */
// ----------------------------------------------------------------------

std::shared_ptr<NFmiQueryData> create_querydata(const Area& theArea,
                                                const ProducerConfig& theProducer,
                                                const Fmi::DateTime& theTime)
{
  const auto& g = theArea.grid();
  const NFmiPoint bottomleft(g.x0 + 0.5 * g.dx, g.y0 + 0.5 * g.dx);
  const NFmiPoint topright(g.x0 + (static_cast<double>(g.nx) - 0.5) * g.dx,
                           g.y0 + (static_cast<double>(g.ny) - 0.5) * g.dx);
  // NFmiArea::CreateFromBBox starts from a placeholder area whose corners are not valid in
  // every projection, and PROJ reports them although the final area is fine
  CPLPushErrorHandler(CPLQuietErrorHandler);
  std::shared_ptr<NFmiArea> area;
  try
  {
    area.reset(
        NFmiArea::CreateFromBBox(Fmi::SpatialReference(theArea.crsWkt()), bottomleft, topright));
  }
  catch (...)
  {
    CPLPopErrorHandler();
    throw;
  }
  CPLPopErrorHandler();
  area->SetGridSize(g.nx, g.ny);
  NFmiGrid grid(area.get(), g.nx, g.ny);
  NFmiHPlaceDescriptor hdesc(grid);

  NFmiProducer producer(kFmiSYNOP, theProducer.name);
  NFmiParamBag pbag;
  for (const auto& p : theProducer.parameters)
  {
    NFmiParam param(p.id,
                    p.name,
                    kFloatMissing,
                    kFloatMissing,
                    kFloatMissing,
                    kFloatMissing,
                    "%.1f",
                    kLinearly);
    pbag.Add(NFmiDataIdent(param, producer));
  }
  NFmiParamDescriptor pdesc(pbag);

  NFmiTimeList tlist;
  tlist.Add(new NFmiMetTime(theTime));  // NOLINT(cppcoreguidelines-owning-memory)
  NFmiTimeDescriptor tdesc(NFmiMetTime(theTime), tlist);

  NFmiLevelBag lbag(kFmiGroundSurface, 0, 0, 0);
  NFmiVPlaceDescriptor vdesc(lbag);

  NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc);
  std::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
  if (!data)
    throw Fmi::Exception(BCP, "Failed to create querydata for the gridded observations");
  return data;
}

// Stations of one parameter in the grid coordinate system. Unknown station elevations are
// replaced by the terrain height, which underestimates summits but is the best we have.
std::vector<Fmi::PointGridding::Station> stations_for(const Area& theArea,
                                                      const StationData& theData,
                                                      const std::vector<double>& theX,
                                                      const std::vector<double>& theY,
                                                      std::size_t theParam)
{
  std::vector<Fmi::PointGridding::Station> stations;
  const auto& dem = theArea.terrain().elevation();
  for (std::size_t i = 0; i < theData.size(); i++)
  {
    const double value = theData.values[theParam][i];
    if (!std::isfinite(value) || !std::isfinite(theX[i]) || !std::isfinite(theY[i]))
      continue;
    double h = theData.elevation[i];
    if (!std::isfinite(h))
      h = dem.sample(theX[i], theY[i]);
    stations.push_back(Fmi::PointGridding::Station{theX[i], theY[i], h, value});
  }
  return stations;
}

// ----------------------------------------------------------------------
/*!
 * \brief The newest model run of a producer which contains the time, null if none does
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q select_background(const Engine::Querydata::Engine& theQEngine,
                                       const BackgroundConfig& theBackground,
                                       const Fmi::DateTime& theTime)
{
  const auto origintimes = theQEngine.origintimes(theBackground.producer);
  for (auto it = origintimes.rbegin(); it != origintimes.rend(); ++it)
  {
    auto q = theQEngine.get(theBackground.producer, *it);
    if (!q || !q->isGrid() || !q->firstTime())
      continue;
    const Fmi::DateTime first = q->validTime();
    q->lastTime();
    const Fmi::DateTime last = q->validTime();
    if (theTime >= first && theTime <= last)
      return q;
  }
  return {};
}

// One parameter of the background interpolated to the grid cells, NaN where missing
Fmi::PointGridding::Field background_field(Engine::Querydata::Q& theQ,
                                           FmiParameterName theParam,
                                           const Area& theArea,
                                           const Fmi::DateTime& theTime)
{
  if (!theQ->param(theParam))
    throw Fmi::Exception(BCP, "Parameter missing from the background")
        .addParameter("Parameter", Fmi::to_string(static_cast<int>(theParam)));
  theQ->firstLevel();
  const auto values = theQ->values(theArea.lonlats(), NFmiMetTime(theTime));

  const auto& g = theArea.grid();
  Fmi::PointGridding::Field field(g, std::numeric_limits<double>::quiet_NaN());
  if (values.NX() != g.nx || values.NY() != g.ny)
    return field;  // time outside the data
  for (std::size_t j = 0; j < g.ny; j++)
    for (std::size_t i = 0; i < g.nx; i++)
    {
      const float v = values[i][j];
      if (v != kFloatMissing && std::isfinite(v))
        field(i, j) = v;
    }
  return field;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Read the configuration and the terrain of each area
 */
// ----------------------------------------------------------------------

Gridder::Gridder(const libconfig::Setting& theSetting,
                 const Engine::Querydata::Engine* theQEngine,
                 Engine::Observation::Engine* theObsEngine)
    : itsConfig(theSetting),
      itsQEngine(theQEngine),
      itsObsEngine(theObsEngine),
      itsCache(itsConfig.cacheSize())
{
  try
  {
    for (const auto& item : itsConfig.producers())
    {
      const auto& p = item.second;
      itsAreas[p.name] = std::make_unique<Area>(p.dem, p.tpiSigma);
      if ((p.source == SourceType::Observations || !p.elevationStationType.empty()) &&
          itsObsEngine == nullptr)
        throw Fmi::Exception(BCP, "Gridded producer needs the observation engine")
            .addParameter("Producer", p.name);
      if (p.source == SourceType::Querydata)
        itsStationElevations[p.name] = read_station_elevations(p.stationElevations);
    }

#ifndef WITHOUT_OBSERVATION
    // Point querydata station elevations from the observation engine, the file overrides
    for (const auto& item : itsConfig.producers())
    {
      const auto& p = item.second;
      if (p.source != SourceType::Querydata || p.elevationStationType.empty())
        continue;
      auto elevations = read_wmo_elevations(
          *itsObsEngine, p.elevationStationType, itsAreas.at(p.name)->lonlatBBox());
      auto& known = itsStationElevations[p.name];
      for (const auto& wmo : elevations)
        known.insert(wmo);  // does not replace the values from the file
      std::cout << Spine::log_time_str() << " WMS gridding: " << elevations.size()
                << " station elevations from station type " << p.elevationStationType
                << " for producer " << p.name << '\n';
    }
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gridded observations initialization failed");
  }
}

Gridder::~Gridder() = default;

// ----------------------------------------------------------------------

const ProducerConfig& Gridder::producer(const std::string& theProducer) const
{
  const auto& producers = itsConfig.producers();
  auto pos = producers.find(theProducer);
  if (pos == producers.end())
    throw Fmi::Exception(BCP, "Unknown gridded observations producer")
        .addParameter("Producer", theProducer);
  return pos->second;
}

std::vector<std::string> Gridder::producers() const
{
  std::vector<std::string> names;
  for (const auto& item : itsConfig.producers())
    names.push_back(item.first);
  return names;  // already sorted by the map
}

bool Gridder::hasProducer(const std::string& theProducer) const
{
  return itsConfig.producers().count(theProducer) > 0;
}

std::vector<std::string> Gridder::parameters(const std::string& theProducer) const
{
  std::vector<std::string> names;
  for (const auto& p : producer(theProducer).parameters)
    names.push_back(p.name);
  return names;
}

std::map<std::string, double> Gridder::boundingBox(const std::string& theProducer) const
{
  return itsAreas.at(producer(theProducer).name)->lonlatBBox();
}

// ----------------------------------------------------------------------
/*!
 * \brief Times available for gridding
 */
// ----------------------------------------------------------------------

std::vector<Fmi::DateTime> Gridder::validTimes(const std::string& theProducer) const
{
  try
  {
    const auto& p = producer(theProducer);
    if (p.source == SourceType::Querydata)
      return querydata_times(*itsQEngine, p);

    // Every timestep minutes up to now minus the delay
    const auto now = Fmi::SecondClock::universal_time() - Fmi::Minutes(p.delay);
    const auto midnight = Fmi::DateTime(now.date());
    const long step = p.timestep;
    const long minutes = (now - midnight).total_seconds() / 60;
    const auto last = midnight + Fmi::Minutes(minutes / step * step);

    std::vector<Fmi::DateTime> times;
    for (long m = p.history / step * step; m >= 0; m -= step)
      times.push_back(last - Fmi::Minutes(m));
    return times;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to list gridded observation times")
        .addParameter("Producer", theProducer);
  }
}

// ----------------------------------------------------------------------

Gridder::SharedStationData Gridder::read(const ProducerConfig& theProducer,
                                         const Fmi::DateTime& theTime) const
{
#ifndef WITHOUT_OBSERVATION
  if (theProducer.source == SourceType::Observations)
  {
    auto read_now = [&]()
    {
      return std::make_shared<const StationData>(read_observations(
          *itsObsEngine, theProducer, itsAreas.at(theProducer.name)->lonlatBBox(), theTime));
    };

    const auto maxage = std::chrono::seconds(itsConfig.observationCacheSeconds());
    if (maxage.count() == 0)
      return read_now();

    // Reuse a recent or ongoing read of the same time, or start one
    const auto key = theProducer.name + ' ' + Fmi::to_iso_string(theTime);
    const auto now = std::chrono::steady_clock::now();
    std::promise<SharedStationData> promise;
    std::shared_future<SharedStationData> future;
    bool owner = false;
    {
      std::lock_guard<std::mutex> lock(itsRecentReadsMutex);
      for (auto it = itsRecentReads.begin(); it != itsRecentReads.end();)
      {
        if (now - it->second.time > maxage)
          it = itsRecentReads.erase(it);
        else
          ++it;
      }
      auto pos = itsRecentReads.find(key);
      if (pos != itsRecentReads.end())
        future = pos->second.data;
      else
      {
        future = promise.get_future().share();
        itsRecentReads[key] = RecentRead{now, future};
        owner = true;
      }
    }

    if (owner)
    {
      try
      {
        promise.set_value(read_now());
      }
      catch (...)
      {
        promise.set_exception(std::current_exception());
        std::lock_guard<std::mutex> lock(itsRecentReadsMutex);
        itsRecentReads.erase(key);  // the next request tries again
      }
    }
    return future.get();
  }
#endif
  return std::make_shared<const StationData>(read_querydata(
      *itsQEngine, theProducer, itsStationElevations.at(theProducer.name), theTime));
}

// ----------------------------------------------------------------------
/*!
 * \brief The background models for the time
 */
// ----------------------------------------------------------------------

Gridder::Backgrounds Gridder::backgrounds(const ProducerConfig& theProducer,
                                          const Fmi::DateTime& theTime) const
{
  Backgrounds result;
  for (const auto& param : theProducer.parameters)
    result.push_back(param.background ? select_background(*itsQEngine, *param.background, theTime)
                                      : Engine::Querydata::Q());
  return result;
}

std::size_t Gridder::dataHash(const ProducerConfig& theProducer,
                              const Fmi::DateTime& theTime,
                              const StationData& theData,
                              const Backgrounds& theBackgrounds) const
{
  std::size_t hash = Fmi::hash_value(theProducer.name);
  Fmi::hash_combine(hash, Fmi::hash_value(theTime));
  Fmi::hash_combine(hash, itsAreas.at(theProducer.name)->hashValue());
  Fmi::hash_combine(hash, theData.hashValue());
  // A new model run changes the result
  for (const auto& q : theBackgrounds)
    Fmi::hash_combine(hash, q ? q->hashValue() : 0);
  return hash;
}

std::size_t Gridder::hashValue(const std::string& theProducer, const Fmi::DateTime& theTime) const
{
  try
  {
    const auto& p = producer(theProducer);
    return dataHash(p, theTime, *read(p, theTime), backgrounds(p, theTime));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to establish gridded observations hash value")
        .addParameter("Producer", theProducer)
        .addParameter("Time", Fmi::to_iso_string(theTime));
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Gridded observations for one time
 *
 * The observations are read for every call so that new and corrected observations are
 * noticed. The gridded data is cached by the hash of the observations.
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q Gridder::get(const std::string& theProducer,
                                  const Fmi::DateTime& theTime) const
{
  try
  {
    const auto& p = producer(theProducer);
    return std::make_shared<Engine::Querydata::QImpl>(
        grid(p, theTime, *read(p, theTime), backgrounds(p, theTime)));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to grid observations")
        .addParameter("Producer", theProducer)
        .addParameter("Time", Fmi::to_iso_string(theTime));
  }
}

Engine::Querydata::SharedModel Gridder::grid(const ProducerConfig& p,
                                             const Fmi::DateTime& theTime,
                                             const StationData& theData,
                                             const Backgrounds& theBackgrounds) const
{
  const auto hash = dataHash(p, theTime, theData, theBackgrounds);

  if (auto cached = itsCache.find(hash))
    return *cached;

  // Join an ongoing gridding of the same data, or start one
  std::promise<Engine::Querydata::SharedModel> promise;
  std::shared_future<Engine::Querydata::SharedModel> future;
  bool owner = false;
  {
    std::lock_guard<std::mutex> lock(itsPendingMutex);
    auto pos = itsPending.find(hash);
    if (pos != itsPending.end())
      future = pos->second;
    else
    {
      future = promise.get_future().share();
      itsPending[hash] = future;
      owner = true;
    }
  }
  if (!owner)
    return future.get();

  try
  {
    // Distinct times are gridded separately, so limit the CPU they can take
    RunningSlot slot(itsRunningMutex, itsRunningCondition, itsRunning, itsConfig.maxConcurrent());

    const Area& area = *itsAreas.at(p.name);
    auto data = create_querydata(area, p, theTime);

    std::vector<double> x = theData.lon;
    std::vector<double> y = theData.lat;
    area.project(x, y);

    NFmiFastQueryInfo info(data.get());
    info.First();
    for (std::size_t k = 0; k < p.parameters.size(); k++)
    {
      const auto& param = p.parameters[k];
      const auto stations = stations_for(area, theData, x, y, k);
      if (!info.Param(param.id))
        throw Fmi::Exception(BCP, "Internal error: gridded parameter missing from querydata");
      if (stations.empty())
        continue;  // all missing

      const auto options = with_threads(param.options, itsConfig.threads());
      auto bg = theBackgrounds.at(k);
      Fmi::PointGridding::Field field;
      if (bg)
      {
        // Without a model orography the model is assumed to see the terrain of the area
        Fmi::PointGridding::Background background{
            background_field(bg, param.background->parameter, area, theTime),
            (param.background->orography != kFmiBadParameter
                 ? background_field(bg, param.background->orography, area, theTime)
                 : area.terrain().elevation())};
        field = Fmi::PointGridding::Analysis(area.terrain(), stations, background, options).grid();
      }
      else
        field = Fmi::PointGridding::Analysis(area.terrain(), stations, options).grid();

      // Both grids start from the south-west corner, x running fastest
      const auto& values = field.values();
      for (std::size_t n = 0; n < values.size(); n++)
      {
        info.LocationIndex(static_cast<unsigned long>(n));
        info.FloatValue(static_cast<float>(values[n]));
      }
    }

    auto model = Engine::Querydata::Model::create(data, hash);
    itsCache.insert(hash, model);
    promise.set_value(model);
  }
  catch (...)
  {
    promise.set_exception(std::current_exception());
  }

  {
    std::lock_guard<std::mutex> lock(itsPendingMutex);
    itsPending.erase(hash);
  }
  try
  {
    return future.get();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gridding observations failed")
        .addParameter("Producer", p.name);
  }
}

// ----------------------------------------------------------------------

Fmi::Cache::CacheStatistics Gridder::getCacheStats() const
{
  Fmi::Cache::CacheStatistics stats;
  stats["WMS::gridded_observations"] = itsCache.statistics();
  return stats;
}

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
