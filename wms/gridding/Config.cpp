// ======================================================================
/*!
 * \brief Gridded observations engine configuration implementation
 */
// ======================================================================

#include "Config.h"
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <newbase/NFmiEnumConverter.h>
#include <spine/Convenience.h>
#include <filesystem>
#include <iostream>

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
// Parameter names to numbers. The converter is expensive to construct.
FmiParameterName parameter_id(const std::string& theName)
{
  // The lookups do not modify the converter, a shared one is safe
  static NFmiEnumConverter converter;
  const auto id = static_cast<FmiParameterName>(converter.ToEnum(theName));
  if (id == kFmiBadParameter)
    throw Fmi::Exception(BCP, "Unknown newbase parameter name '" + theName + "'");
  return id;
}

// Optional number in kilometres to metres
void lookup_km(const libconfig::Setting& theSetting, const char* theName, double& theValue)
{
  double km = 0;
  if (theSetting.lookupValue(theName, km))
  {
    if (km <= 0)
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be positive", theName));
    theValue = km * 1000;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Gridding options of a parameter
 *
 * method "terrain" (the default) uses the height and topographic position regression,
 * suitable for temperatures. Method "barnes" is a damped multi-scale Barnes interpolation
 * without any terrain dependency, suitable for reduced pressure and other smooth fields.
 */
// ----------------------------------------------------------------------

Fmi::PointGridding::Options parse_options(const libconfig::Setting& theSetting)
{
  Fmi::PointGridding::Options options;

  std::string method = "terrain";
  theSetting.lookupValue("method", method);
  if (method == "barnes")
  {
    options.predictors.clear();
    options.bilateralSigmaH.reset();
  }
  else if (method != "terrain")
    throw Fmi::Exception(BCP, "Unknown gridding method '" + method + "'")
        .addDetail("Known methods are 'terrain' and 'barnes'");

  theSetting.lookupValue("qc", options.qc);
  theSetting.lookupValue("qc_threshold", options.qcMinThreshold);
  theSetting.lookupValue("damping", options.damping);
  lookup_km(theSetting, "local_sigma", options.localSigma);
  lookup_km(theSetting, "regional_sigma", options.regionalSigma);

  double sigma_h = 0;
  if (theSetting.lookupValue("bilateral_sigma_h", sigma_h))
  {
    if (sigma_h > 0)
      options.bilateralSigmaH = sigma_h;
    else
      options.bilateralSigmaH.reset();
  }

  if (theSetting.exists("residual_sigmas"))
  {
    const auto& sigmas = theSetting.lookup("residual_sigmas");
    if (!sigmas.isArray() || sigmas.getLength() == 0)
      throw Fmi::Exception(BCP, "Setting 'residual_sigmas' must be a nonempty array of km");
    options.residualSigmas.clear();
    for (int i = 0; i < sigmas.getLength(); i++)
      options.residualSigmas.push_back(1000 * static_cast<double>(sigmas[i]));
  }
  return options;
}

// ----------------------------------------------------------------------
/*!
 * \brief Background settings of a parameter
 *
 * The lapse rate moves the background from the model orography to the actual heights. It
 * defaults to the standard atmosphere for the terrain method and to zero for the barnes
 * method, which is meant for fields without a height dependency.
 */
// ----------------------------------------------------------------------

BackgroundConfig parse_background(const libconfig::Setting& theSetting,
                                  const std::string& theParameter,
                                  Fmi::PointGridding::Options& theOptions)
{
  if (!theSetting.isGroup())
    throw Fmi::Exception(BCP, "Setting 'background' must be a group");

  BackgroundConfig background;
  if (!theSetting.lookupValue("producer", background.producer))
    throw Fmi::Exception(BCP, "Setting 'background.producer' is required");

  std::string name = theParameter;
  theSetting.lookupValue("parameter", name);
  background.parameter = parameter_id(name);

  if (theSetting.lookupValue("orography", name))
    background.orography = parameter_id(name);

  theOptions.backgroundLapseRate = (theOptions.predictors.empty() ? 0.0 : -6.5);
  theSetting.lookupValue("lapse_rate", theOptions.backgroundLapseRate);
  theSetting.lookupValue("weight", theOptions.backgroundWeight);
  lookup_km(theSetting, "taper", theOptions.backgroundTaper);
  if (theOptions.backgroundWeight <= 0 || theOptions.backgroundWeight > 1)
    throw Fmi::Exception(BCP, "Setting 'background.weight' must be in the range (0,1]");
  return background;
}

int lookup_minutes(const libconfig::Setting& theSetting, const char* theName, int theDefault)
{
  int value = theDefault;
  theSetting.lookupValue(theName, value);
  if (value < 0)
    throw Fmi::Exception(BCP, fmt::format("Setting '{}' must not be negative", theName));
  return value;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Read and validate the configuration file
 */
// ----------------------------------------------------------------------

Config::Config(const libconfig::Setting& theSetting)
{
  try
  {
    if (!theSetting.isGroup())
      throw Fmi::Exception(BCP, "The gridding setting must be a group");

    int cachesize = static_cast<int>(itsCacheSize);
    if (theSetting.lookupValue("cache_size", cachesize) && cachesize < 1)
      throw Fmi::Exception(BCP, "Setting 'gridding.cache_size' must be at least 1");
    itsCacheSize = static_cast<std::size_t>(cachesize);

    int threads = 0;
    if (theSetting.lookupValue("threads", threads) && threads < 0)
      throw Fmi::Exception(BCP, "Setting 'gridding.threads' must not be negative");
    itsThreads = static_cast<unsigned int>(threads);

    int concurrency = static_cast<int>(itsMaxConcurrent);
    if (theSetting.lookupValue("max_concurrent", concurrency) && concurrency < 1)
      throw Fmi::Exception(BCP, "Setting 'gridding.max_concurrent' must be at least 1");
    itsMaxConcurrent = static_cast<unsigned int>(concurrency);

    int cacheseconds = static_cast<int>(itsObservationCacheSeconds);
    if (theSetting.lookupValue("observation_cache_seconds", cacheseconds) && cacheseconds < 0)
      throw Fmi::Exception(BCP, "Setting 'gridding.observation_cache_seconds' must not be negative");
    itsObservationCacheSeconds = static_cast<unsigned int>(cacheseconds);

    if (theSetting.exists("producers"))
    {
      const auto& producers = theSetting.lookup("producers");
      if (!producers.isGroup())
        throw Fmi::Exception(BCP, "Setting 'gridding.producers' must be a group");
      for (int i = 0; i < producers.getLength(); i++)
        parseProducer(producers[i]);
    }
  }
  catch (const libconfig::ConfigException& e)
  {
    throw Fmi::Exception(BCP, fmt::format("Invalid gridding configuration: {}", e.what()));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Gridding configuration failed");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the settings of one producer
 */
// ----------------------------------------------------------------------

void Config::parseProducer(const libconfig::Setting& theSetting)
{
  ProducerConfig producer;
  producer.name = theSetting.getName();

  try
  {
    if (!theSetting.isGroup())
      throw Fmi::Exception(BCP, "Producer setting must be a group");

    std::string source;
    if (!theSetting.lookupValue("source", source))
      throw Fmi::Exception(BCP, "Setting 'source' is required");
    if (source == "observations")
      producer.source = SourceType::Observations;
    else if (source == "querydata")
      producer.source = SourceType::Querydata;
    else
      throw Fmi::Exception(BCP, "Setting 'source' must be 'observations' or 'querydata'");

    if (!theSetting.lookupValue("producer", producer.sourceProducer))
      throw Fmi::Exception(BCP, "Setting 'producer' is required");
    if (!theSetting.lookupValue("dem", producer.dem))
      throw Fmi::Exception(BCP, "Setting 'dem' is required");

    lookup_km(theSetting, "tpi_sigma", producer.tpiSigma);
    theSetting.lookupValue("station_elevations", producer.stationElevations);
    theSetting.lookupValue("elevation_stationtype", producer.elevationStationType);

    producer.timestep = lookup_minutes(theSetting, "timestep", producer.timestep);
    producer.delay = lookup_minutes(theSetting, "delay", producer.delay);
    producer.history = lookup_minutes(theSetting, "history", producer.history);
    producer.timeTolerance = lookup_minutes(theSetting, "time_tolerance", 0) * 60;
    theSetting.lookupValue("max_times", producer.maxTimes);
    if (producer.maxTimes < 1)
      throw Fmi::Exception(BCP, "Setting 'max_times' must be at least 1");
    if (producer.timestep == 0)
      throw Fmi::Exception(BCP, "Setting 'timestep' must be positive");

    if (!theSetting.exists("parameters"))
      throw Fmi::Exception(BCP, "Setting 'parameters' is required");
    const auto& parameters = theSetting.lookup("parameters");
    if (!parameters.isList() || parameters.getLength() == 0)
      throw Fmi::Exception(BCP, "Setting 'parameters' must be a nonempty list");

    for (int i = 0; i < parameters.getLength(); i++)
    {
      const auto& setting = parameters[i];
      ParameterConfig param;
      if (!setting.lookupValue("name", param.name))
        throw Fmi::Exception(BCP, "Parameter setting 'name' is required");
      param.id = parameter_id(param.name);
      param.source = param.name;
      setting.lookupValue("source", param.source);
      param.options = parse_options(setting);
      if (setting.exists("background"))
        param.background = parse_background(setting.lookup("background"), param.name, param.options);
      producer.parameters.push_back(param);
    }

    if (!itsProducers.insert(std::make_pair(producer.name, producer)).second)
      throw Fmi::Exception(BCP, "Duplicate producer");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Invalid gridded producer configuration")
        .addParameter("Producer", producer.name);
  }
}

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
