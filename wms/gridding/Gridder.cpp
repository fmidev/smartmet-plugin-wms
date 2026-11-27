// ======================================================================
/*!
 * \brief Gridded observations
 */
// ======================================================================

#include "Gridder.h"
#include "Area.h"
#include "Config.h"
#include "Sources.h"
#include <boost/thread/thread.hpp>
#include <engines/querydata/Engine.h>
#include <engines/querydata/Model.h>
#include <gis/PointGridding.h>
#include <gis/SpatialReference.h>
#include <macgyver/Cancellation.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiDataIdent.h>
#include <newbase/NFmiDataMatrix.h>
#include <newbase/NFmiFastQueryInfo.h>
#include <newbase/NFmiGlobals.h>
#include <newbase/NFmiGrid.h>
#include <newbase/NFmiHPlaceDescriptor.h>
#include <newbase/NFmiLevelBag.h>
#include <newbase/NFmiLevelType.h>
#include <newbase/NFmiMetTime.h>
#include <newbase/NFmiParam.h>
#include <newbase/NFmiParamBag.h>
#include <newbase/NFmiParamDescriptor.h>
#include <newbase/NFmiParameterName.h>
#include <newbase/NFmiPoint.h>
#include <newbase/NFmiProducer.h>
#include <newbase/NFmiProducerName.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiString.h>
#include <newbase/NFmiTimeDescriptor.h>
#include <newbase/NFmiTimeList.h>
#include <newbase/NFmiVPlaceDescriptor.h>
#include <spine/Convenience.h>
#include <cmath>
#include <cpl_error.h>
#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <iostream>
#include <set>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

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
    // The limits of the values, if any, are the range of the parameter
    const auto& options = p.options;
    NFmiParam param(p.id,
                    p.name,
                    options.lowerLimit ? *options.lowerLimit : kFloatMissing,
                    options.upperLimit ? *options.upperLimit : kFloatMissing,
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
      if (p.source == SourceType::Observations && itsObsEngine == nullptr)
        throw Fmi::Exception(BCP, "Gridded producer needs the observation engine")
            .addParameter("Producer", p.name);
      itsProducers[p.name] = std::make_shared<const ProducerConfig>(p);
    }

    for (const auto& item : itsConfig.areas())
    {
      const auto& a = item.second;
      itsAreas[a.name] = std::make_unique<Area>(a.dem, a.tpiSigma, a.seaFraction, a.seaSigma);
      if (!a.elevationStationType.empty() && itsObsEngine == nullptr)
        throw Fmi::Exception(BCP, "Station elevations by station type need the observation engine")
            .addParameter("Area", a.name);
      itsStationElevations[a.name] = read_station_elevations(a.stationElevations);
    }

#ifndef WITHOUT_OBSERVATION
    // Point querydata station elevations from the observation engine, the file overrides
    for (const auto& item : itsConfig.areas())
    {
      const auto& a = item.second;
      if (a.elevationStationType.empty())
        continue;
      auto elevations =
          read_wmo_elevations(*itsObsEngine, a.elevationStationType, itsAreas.at(a.name)->lonlatBBox());
      auto& known = itsStationElevations[a.name];
      for (const auto& wmo : elevations)
        known.insert(wmo);  // does not replace the values from the file
      std::cout << Spine::log_time_str() << " WMS gridding: " << elevations.size()
                << " station elevations from station type " << a.elevationStationType
                << " for area " << a.name << '\n';
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
/*!
 * \brief The definition file of a producer
 *
 * The file is checked on every call and parsed again when its time or size has changed, so
 * edits take effect on the next request without a restart.
 */
// ----------------------------------------------------------------------

std::shared_ptr<const Definition> Gridder::definition(const std::string& theName) const
{
  if (itsConfig.directory().empty())
    return {};
  // Names of letters, digits, '_' and '-' only: no paths outside the directory
  for (char c : theName)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
      return {};
  if (theName.empty())
    return {};

  const auto path = std::filesystem::path(itsConfig.directory()) / (theName + ".json");
  std::error_code ec;
  const auto time = std::filesystem::last_write_time(path, ec);
  if (ec)
    return {};
  const auto size = std::filesystem::file_size(path, ec);
  if (ec)
    return {};

  {
    std::lock_guard<std::mutex> lock(itsDefinitionsMutex);
    auto pos = itsDefinitions.find(theName);
    if (pos != itsDefinitions.end() && pos->second.time == time && pos->second.size == size)
      return pos->second.definition;
  }

  std::ifstream in(path);
  if (!in)
    throw Fmi::Exception(BCP, "Failed to read the gridded producer definition")
        .addParameter("File", path.string());
  Json::Value json;
  Json::CharReaderBuilder builder;
  std::string errors;
  if (!Json::parseFromStream(builder, in, &json, &errors))
    throw Fmi::Exception(BCP, "Failed to parse the gridded producer definition")
        .addParameter("File", path.string())
        .addParameter("Error", errors);

  std::shared_ptr<const Definition> parsed;
  try
  {
    parsed = std::make_shared<const Definition>(itsConfig.parseDefinition(json, theName));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Invalid gridded producer definition file")
        .addParameter("File", path.string());
  }

  std::lock_guard<std::mutex> lock(itsDefinitionsMutex);
  itsDefinitions[theName] = CachedDefinition{time, size, parsed};
  return parsed;
}

// ----------------------------------------------------------------------

std::shared_ptr<const ProducerConfig> Gridder::producer(const std::string& theProducer) const
{
  auto pos = itsProducers.find(theProducer);
  if (pos != itsProducers.end())
    return pos->second;

  // name or name@variant of a definition file
  const auto at = theProducer.find('@');
  const auto name = theProducer.substr(0, at);
  auto def = definition(name);
  if (!def)
    throw Fmi::Exception(BCP, "Unknown gridded observations producer")
        .addParameter("Producer", theProducer);

  std::shared_ptr<const ProducerConfig> p;
  if (at == std::string::npos)
    p = def->defaultVariant;
  else
  {
    const auto variant = theProducer.substr(at + 1);
    auto v = def->variants.find(variant);
    if (v == def->variants.end())
    {
      std::string known;
      for (const auto& item : def->variants)
        known += (known.empty() ? "" : ", ") + item.first;
      throw Fmi::Exception(BCP, "Unknown variant of a gridded observations producer")
          .addParameter("Producer", name)
          .addParameter("Variant", variant)
          .addParameter("Variants", known.empty() ? "none" : known);
    }
    p = v->second;
  }
  if (p->source == SourceType::Observations && itsObsEngine == nullptr)
    throw Fmi::Exception(BCP, "Gridded producer needs the observation engine")
        .addParameter("Producer", theProducer);
  return p;
}

std::vector<std::string> Gridder::producers() const
{
  std::set<std::string> names;
  for (const auto& item : itsProducers)
    names.insert(item.first);

  // The definition files of the directory
  if (!itsConfig.directory().empty())
  {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(itsConfig.directory(), ec))
    {
      const auto& path = entry.path();
      if (path.extension() != ".json")
        continue;
      const auto stem = path.stem().string();
      if (!stem.empty() && std::all_of(stem.begin(),
                                       stem.end(),
                                       [](char c)
                                       {
                                         return std::isalnum(static_cast<unsigned char>(c)) ||
                                                c == '_' || c == '-';
                                       }))
        names.insert(stem);
    }
  }
  return {names.begin(), names.end()};
}

bool Gridder::hasProducer(const std::string& theProducer) const
{
  if (itsProducers.count(theProducer) > 0)
    return true;
  // A definition file exists: errors in it are reported when the data is requested
  if (itsConfig.directory().empty())
    return false;
  const auto name = theProducer.substr(0, theProducer.find('@'));
  if (name.empty() || !std::all_of(name.begin(),
                                   name.end(),
                                   [](char c)
                                   {
                                     return std::isalnum(static_cast<unsigned char>(c)) ||
                                            c == '_' || c == '-';
                                   }))
    return false;
  std::error_code ec;
  return std::filesystem::is_regular_file(
      std::filesystem::path(itsConfig.directory()) / (name + ".json"), ec);
}

std::vector<std::string> Gridder::parameters(const std::string& theProducer) const
{
  std::vector<std::string> names;
  for (const auto& p : producer(theProducer)->parameters)
    names.push_back(p.name);
  return names;
}

std::map<std::string, double> Gridder::boundingBox(const std::string& theProducer) const
{
  return itsAreas.at(producer(theProducer)->area)->lonlatBBox();
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
    const auto& p = *producer(theProducer);
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
          *itsObsEngine, theProducer, itsAreas.at(theProducer.area)->lonlatBBox(), theTime));
    };

    const auto maxage = std::chrono::seconds(itsConfig.observationCacheSeconds());
    if (maxage.count() == 0)
      return read_now();

    // Reuse a recent or ongoing read of the same observations, also by another producer or
    // variant reading the same area, station type, parameters and time, or start one
    std::string key = theProducer.area + ' ' + theProducer.sourceProducer + ' ' +
                      Fmi::to_iso_string(theTime) + ' ' + std::to_string(theProducer.timeTolerance);
    for (const auto& param : theProducer.parameters)
      key += ' ' + param.source;
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
        // Others may wait for the result, the read must not fail with this request, neither
        // by its cancellation nor by the interruption of its thread
        Fmi::CancellationScope none(nullptr);
        boost::this_thread::disable_interruption no_interruption;
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
      *itsQEngine, theProducer, itsStationElevations.at(theProducer.area), theTime));
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
  // The settings, not the name: a default variant and the variant it names share the data
  std::size_t hash = theProducer.hash;
  Fmi::hash_combine(hash, Fmi::hash_value(theTime));
  Fmi::hash_combine(hash, itsAreas.at(theProducer.area)->hashValue());
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
    const auto& p = *producer(theProducer);
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
    const auto& p = *producer(theProducer);
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
    // Others may wait for the result, the gridding must not fail with this request, neither
    // by its cancellation nor by the interruption of its thread
    Fmi::CancellationScope none(nullptr);
    boost::this_thread::disable_interruption no_interruption;

    // Distinct times are gridded separately, so limit the CPU they can take
    RunningSlot slot(itsRunningMutex, itsRunningCondition, itsRunning, itsConfig.maxConcurrent());

    const Area& area = *itsAreas.at(p.area);
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
