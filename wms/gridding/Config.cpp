// ======================================================================
/*!
 * \brief Gridded observations configuration implementation
 *
 * The producers come from the gridding group of the plugin configuration file (libconfig) and
 * from the definition files of the gridding directory (JSON). Both are parsed by the same code
 * through the Settings adapter below; the definition files are strict, an unknown setting is an
 * error, since a misspelled setting would otherwise silently do nothing.
 */
// ======================================================================

#include "Config.h"
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
#include <newbase/NFmiEnumConverter.h>
#include <algorithm>
#include <regex>
#include <set>
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

// ----------------------------------------------------------------------
/*!
 * \brief Read access to a group of settings, in libconfig or JSON
 */
// ----------------------------------------------------------------------

class Settings
{
 public:
  virtual ~Settings() = default;
  virtual bool has(const char* theName) const = 0;
  // Numbers may be written as integers or decimals
  virtual std::optional<double> number(const char* theName) const = 0;
  virtual std::optional<std::string> string(const char* theName) const = 0;
  virtual std::optional<bool> boolean(const char* theName) const = 0;
  virtual std::optional<std::vector<double>> numbers(const char* theName) const = 0;
  virtual std::unique_ptr<Settings> group(const char* theName) const = 0;
  virtual std::vector<std::unique_ptr<Settings>> list(const char* theName) const = 0;
  // Unknown settings are errors in strict settings (the definition files)
  virtual void checkKnown(const std::set<std::string>& theNames) const = 0;
};

class LibconfigSettings : public Settings
{
 public:
  explicit LibconfigSettings(const libconfig::Setting& theSetting) : itsSetting(theSetting) {}

  bool has(const char* theName) const override { return itsSetting.exists(theName); }

  std::optional<double> number(const char* theName) const override
  {
    if (!itsSetting.exists(theName))
      return {};
    return to_number(itsSetting[theName]);
  }

  std::optional<std::string> string(const char* theName) const override
  {
    std::string value;
    if (!itsSetting.lookupValue(theName, value))
      return {};
    return value;
  }

  std::optional<bool> boolean(const char* theName) const override
  {
    bool value = false;
    if (!itsSetting.lookupValue(theName, value))
      return {};
    return value;
  }

  std::optional<std::vector<double>> numbers(const char* theName) const override
  {
    if (!itsSetting.exists(theName))
      return {};
    const auto& values = itsSetting.lookup(theName);
    if (!values.isArray() && !values.isList())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be an array", theName));
    std::vector<double> result;
    for (int i = 0; i < values.getLength(); i++)
      result.push_back(to_number(values[i]));
    return result;
  }

  std::unique_ptr<Settings> group(const char* theName) const override
  {
    const auto& value = itsSetting.lookup(theName);
    if (!value.isGroup())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be a group", theName));
    return std::make_unique<LibconfigSettings>(value);
  }

  std::vector<std::unique_ptr<Settings>> list(const char* theName) const override
  {
    const auto& values = itsSetting.lookup(theName);
    if (!values.isList())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be a list", theName));
    std::vector<std::unique_ptr<Settings>> result;
    for (int i = 0; i < values.getLength(); i++)
      result.push_back(std::make_unique<LibconfigSettings>(values[i]));
    return result;
  }

  void checkKnown(const std::set<std::string>& /* theNames */) const override {}

 private:
  // The libconfig lookups of doubles would silently ignore integer settings such as "lolimit = 0;"
  static double to_number(const libconfig::Setting& theSetting)
  {
    switch (theSetting.getType())
    {
      case libconfig::Setting::TypeInt:
        return static_cast<int>(theSetting);
      case libconfig::Setting::TypeInt64:
        return static_cast<double>(static_cast<long long>(theSetting));
      case libconfig::Setting::TypeFloat:
        return static_cast<double>(theSetting);
      default:
        throw Fmi::Exception(BCP,
                             fmt::format("Setting '{}' must be a number", theSetting.getPath()));
    }
  }

  const libconfig::Setting& itsSetting;
};

class JsonSettings : public Settings
{
 public:
  explicit JsonSettings(const Json::Value& theValue) : itsValue(theValue)
  {
    if (!itsValue.isObject())
      throw Fmi::Exception(BCP, "Expected a JSON object");
  }

  bool has(const char* theName) const override { return itsValue.isMember(theName); }

  std::optional<double> number(const char* theName) const override
  {
    if (!has(theName))
      return {};
    const auto& v = itsValue[theName];
    if (!v.isNumeric())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be a number", theName));
    return v.asDouble();
  }

  std::optional<std::string> string(const char* theName) const override
  {
    if (!has(theName))
      return {};
    const auto& v = itsValue[theName];
    if (!v.isString())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be a string", theName));
    return v.asString();
  }

  std::optional<bool> boolean(const char* theName) const override
  {
    if (!has(theName))
      return {};
    const auto& v = itsValue[theName];
    if (!v.isBool())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be true or false", theName));
    return v.asBool();
  }

  std::optional<std::vector<double>> numbers(const char* theName) const override
  {
    if (!has(theName))
      return {};
    const auto& v = itsValue[theName];
    if (!v.isArray())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be an array", theName));
    std::vector<double> result;
    for (const auto& x : v)
    {
      if (!x.isNumeric())
        throw Fmi::Exception(BCP, fmt::format("Setting '{}' must contain numbers", theName));
      result.push_back(x.asDouble());
    }
    return result;
  }

  std::unique_ptr<Settings> group(const char* theName) const override
  {
    const auto& v = itsValue[theName];
    if (!v.isObject())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be an object", theName));
    return std::make_unique<JsonSettings>(v);
  }

  std::vector<std::unique_ptr<Settings>> list(const char* theName) const override
  {
    const auto& v = itsValue[theName];
    if (!v.isArray())
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be an array", theName));
    std::vector<std::unique_ptr<Settings>> result;
    for (const auto& x : v)
      result.push_back(std::make_unique<JsonSettings>(x));
    return result;
  }

  void checkKnown(const std::set<std::string>& theNames) const override
  {
    for (const auto& name : itsValue.getMemberNames())
      if (theNames.count(name) == 0)
        throw Fmi::Exception(BCP, fmt::format("Unknown setting '{}'", name));
  }

 private:
  const Json::Value& itsValue;
};

// The settings of one parameter, its background, and of a definition file
const std::set<std::string> kParameterSettings = {"name",
                                                  "source",
                                                  "method",
                                                  "residuals",
                                                  "qc",
                                                  "qc_threshold",
                                                  "damping",
                                                  "prior_lapse_rate",
                                                  "local_sigma",
                                                  "regional_sigma",
                                                  "lolimit",
                                                  "hilimit",
                                                  "bilateral_sigma_h",
                                                  "oi_sigma_h",
                                                  "residual_sigmas",
                                                  "oi_scales",
                                                  "oi_error_ratio",
                                                  "use_sea_fraction",
                                                  "background"};
const std::set<std::string> kBackgroundSettings = {
    "producer", "parameter", "orography", "lapse_rate", "weight", "taper"};
const std::set<std::string> kDefinitionSettings = {"title",
                                                   "area",
                                                   "source",
                                                   "producer",
                                                   "timestep",
                                                   "delay",
                                                   "history",
                                                   "time_tolerance",
                                                   "max_times",
                                                   "parameters",
                                                   "default",
                                                   "variants"};
const std::set<std::string> kVariantSettings = {"name", "title", "area", "parameters"};

// Optional number in kilometres to metres
void lookup_km(const Settings& theSettings, const char* theName, double& theValue)
{
  if (auto km = theSettings.number(theName))
  {
    if (*km <= 0)
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be positive", theName));
    theValue = *km * 1000;
  }
}

// Optional nonempty array of kilometres to metres
void lookup_km_array(const Settings& theSettings, const char* theName, std::vector<double>& theValues)
{
  auto values = theSettings.numbers(theName);
  if (!values)
    return;
  if (values->empty())
    throw Fmi::Exception(BCP, fmt::format("Setting '{}' must be a nonempty array of km", theName));
  theValues.clear();
  for (double km : *values)
  {
    if (km <= 0)
      throw Fmi::Exception(BCP, fmt::format("Setting '{}' must contain positive values", theName));
    theValues.push_back(1000 * km);
  }
}

int lookup_minutes(const Settings& theSettings, const char* theName, int theDefault)
{
  const auto value = theSettings.number(theName);
  if (!value)
    return theDefault;
  if (*value < 0)
    throw Fmi::Exception(BCP, fmt::format("Setting '{}' must not be negative", theName));
  return static_cast<int>(*value);
}

// ----------------------------------------------------------------------
/*!
 * \brief Gridding options of a parameter
 *
 * method "terrain" (the default) uses the height and topographic position regression with
 * optimal interpolation of the residuals, suitable for temperatures. Method "barnes" is a
 * damped multi-scale Barnes interpolation without any terrain dependency, suitable for reduced
 * pressure and other smooth fields. Setting "residuals" overrides the residual analysis of
 * either method.
 */
// ----------------------------------------------------------------------

Fmi::PointGridding::Options parse_options(const Settings& theSettings)
{
  Fmi::PointGridding::Options options;

  const std::string method = theSettings.string("method").value_or("terrain");
  if (method == "barnes")
  {
    options.predictors.clear();
    options.bilateralSigmaH.reset();
    options.oiSigmaH.reset();
    options.residuals = Fmi::PointGridding::Residuals::Barnes;
  }
  else if (method != "terrain")
    throw Fmi::Exception(BCP, "Unknown gridding method '" + method + "'")
        .addDetail("Known methods are 'terrain' and 'barnes'");

  if (auto qc = theSettings.boolean("qc"))
    options.qc = *qc;
  options.qcMinThreshold = theSettings.number("qc_threshold").value_or(options.qcMinThreshold);
  options.damping = theSettings.number("damping").value_or(options.damping);
  options.priorLapseRate = theSettings.number("prior_lapse_rate").value_or(options.priorLapseRate);
  lookup_km(theSettings, "local_sigma", options.localSigma);
  lookup_km(theSettings, "regional_sigma", options.regionalSigma);

  options.lowerLimit = theSettings.number("lolimit");
  options.upperLimit = theSettings.number("hilimit");
  if (options.lowerLimit && options.upperLimit && *options.lowerLimit >= *options.upperLimit)
    throw Fmi::Exception(BCP, "Setting 'lolimit' must be below 'hilimit'");

  if (auto sigma_h = theSettings.number("bilateral_sigma_h"))
  {
    if (*sigma_h > 0)
      options.bilateralSigmaH = *sigma_h;
    else
      options.bilateralSigmaH.reset();
  }
  if (auto sigma_h = theSettings.number("oi_sigma_h"))
  {
    if (*sigma_h > 0)
      options.oiSigmaH = *sigma_h;
    else
      options.oiSigmaH.reset();
  }

  if (auto residuals = theSettings.string("residuals"))
  {
    if (*residuals == "oi")
      options.residuals = Fmi::PointGridding::Residuals::OI;
    else if (*residuals == "barnes")
      options.residuals = Fmi::PointGridding::Residuals::Barnes;
    else
      throw Fmi::Exception(BCP, "Unknown residual analysis '" + *residuals + "'")
          .addDetail("Known residual analyses are 'oi' and 'barnes'");
  }
  lookup_km_array(theSettings, "residual_sigmas", options.residualSigmas);
  lookup_km_array(theSettings, "oi_scales", options.oiScales);
  if (auto ratio = theSettings.number("oi_error_ratio"))
  {
    if (*ratio <= 0)
      throw Fmi::Exception(BCP, "Setting 'oi_error_ratio' must be positive");
    options.oiErrorRatio = *ratio;
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

BackgroundConfig parse_background(const Settings& theSettings,
                                  const std::string& theParameter,
                                  Fmi::PointGridding::Options& theOptions)
{
  theSettings.checkKnown(kBackgroundSettings);

  BackgroundConfig background;
  auto producer = theSettings.string("producer");
  if (!producer)
    throw Fmi::Exception(BCP, "Setting 'background.producer' is required");
  background.producer = *producer;

  background.parameter = parameter_id(theSettings.string("parameter").value_or(theParameter));
  if (auto orography = theSettings.string("orography"))
    background.orography = parameter_id(*orography);

  theOptions.backgroundLapseRate = (theOptions.predictors.empty() ? 0.0 : -6.5);
  theOptions.backgroundLapseRate =
      theSettings.number("lapse_rate").value_or(theOptions.backgroundLapseRate);
  theOptions.backgroundWeight = theSettings.number("weight").value_or(theOptions.backgroundWeight);
  lookup_km(theSettings, "taper", theOptions.backgroundTaper);
  if (theOptions.backgroundWeight <= 0 || theOptions.backgroundWeight > 1)
    throw Fmi::Exception(BCP, "Setting 'background.weight' must be in the range (0,1]");
  return background;
}

// ----------------------------------------------------------------------
/*!
 * \brief The source, times and parameters of a producer
 */
// ----------------------------------------------------------------------

void parse_data_settings(const Settings& theSettings,
                         ProducerConfig& theProducer,
                         bool theSeaFraction)
{
  const auto source = theSettings.string("source");
  if (!source)
    throw Fmi::Exception(BCP, "Setting 'source' is required");
  if (*source == "observations")
    theProducer.source = SourceType::Observations;
  else if (*source == "querydata")
    theProducer.source = SourceType::Querydata;
  else
    throw Fmi::Exception(BCP, "Setting 'source' must be 'observations' or 'querydata'");

  const auto producer = theSettings.string("producer");
  if (!producer)
    throw Fmi::Exception(BCP, "Setting 'producer' is required");
  theProducer.sourceProducer = *producer;

  theProducer.timestep = lookup_minutes(theSettings, "timestep", theProducer.timestep);
  theProducer.delay = lookup_minutes(theSettings, "delay", theProducer.delay);
  theProducer.history = lookup_minutes(theSettings, "history", theProducer.history);
  theProducer.timeTolerance = lookup_minutes(theSettings, "time_tolerance", 0) * 60;
  theProducer.maxTimes = static_cast<int>(
      theSettings.number("max_times").value_or(static_cast<double>(theProducer.maxTimes)));
  if (theProducer.maxTimes < 1)
    throw Fmi::Exception(BCP, "Setting 'max_times' must be at least 1");
  if (theProducer.timestep == 0)
    throw Fmi::Exception(BCP, "Setting 'timestep' must be positive");

  if (!theSettings.has("parameters"))
    throw Fmi::Exception(BCP, "Setting 'parameters' is required");
  const auto parameters = theSettings.list("parameters");
  if (parameters.empty())
    throw Fmi::Exception(BCP, "Setting 'parameters' must be a nonempty list");

  for (const auto& setting : parameters)
  {
    ParameterConfig param;
    auto name = setting->string("name");
    if (!name)
      throw Fmi::Exception(BCP, "Parameter setting 'name' is required");
    try
    {
      setting->checkKnown(kParameterSettings);
      param.name = *name;
      param.id = parameter_id(param.name);
      param.source = setting->string("source").value_or(param.name);
      param.options = parse_options(*setting);

      // The sea fraction is used by default with the terrain method only
      bool use_sea_fraction =
          setting->boolean("use_sea_fraction").value_or(!param.options.predictors.empty());
      if (use_sea_fraction && theSeaFraction)
        param.options.covariates = {kSeaFraction};
      if (setting->has("background"))
        param.background =
            parse_background(*setting->group("background"), param.name, param.options);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Invalid gridded parameter").addParameter("Parameter", *name);
    }
    if (std::any_of(theProducer.parameters.begin(),
                    theProducer.parameters.end(),
                    [&](const ParameterConfig& p) { return p.name == param.name; }))
      throw Fmi::Exception(BCP, "Duplicate parameter '" + param.name + "'");
    theProducer.parameters.push_back(param);
  }
}

// The area settings of a group: an area of its own or of a producer of the configuration file
void parse_area_settings(const Settings& theSettings, AreaConfig& theArea)
{
  auto dem = theSettings.string("dem");
  if (!dem)
    throw Fmi::Exception(BCP, "Setting 'dem' is required");
  theArea.dem = *dem;
  lookup_km(theSettings, "tpi_sigma", theArea.tpiSigma);
  theArea.seaFraction = theSettings.string("sea_fraction").value_or("");
  lookup_km(theSettings, "sea_sigma", theArea.seaSigma);
  theArea.stationElevations = theSettings.string("station_elevations").value_or("");
  theArea.elevationStationType = theSettings.string("elevation_stationtype").value_or("");
}

// Names of definition files and variants: no paths
bool valid_name(const std::string& theName)
{
  static const std::regex re("[A-Za-z0-9_-]+");
  return std::regex_match(theName, re);
}

// The JSON of a variant: the definition with the variant's overrides merged in
Json::Value merge_variant(const Json::Value& theBase, const Json::Value& theVariant)
{
  Json::Value result = theBase;
  result.removeMember("variants");
  result.removeMember("default");
  for (const char* name : {"title", "area"})
    if (theVariant.isMember(name))
      result[name] = theVariant[name];

  if (theVariant.isMember("parameters"))
  {
    const auto& overrides = theVariant["parameters"];
    if (!overrides.isObject())
      throw Fmi::Exception(BCP, "The parameters of a variant must be an object keyed by parameter name");
    for (const auto& param : overrides.getMemberNames())
    {
      const auto& settings = overrides[param];
      if (!settings.isObject())
        throw Fmi::Exception(BCP, "The override of parameter '" + param + "' must be an object");
      if (settings.isMember("name") || settings.isMember("source"))
        throw Fmi::Exception(BCP, "A variant cannot change the name or source of parameter '" + param + "'");
      bool found = false;
      for (auto& p : result["parameters"])
      {
        if (p.isObject() && p["name"].isString() && p["name"].asString() == param)
        {
          for (const auto& key : settings.getMemberNames())
            p[key] = settings[key];
          found = true;
        }
      }
      if (!found)
        throw Fmi::Exception(BCP, "A variant cannot add parameter '" + param + "'");
    }
  }
  return result;
}

// The hash of the settings which determine the gridded values
std::size_t definition_hash(Json::Value theJson)
{
  theJson.removeMember("title");
  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  return Fmi::hash_value(Json::writeString(builder, theJson));
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

    theSetting.lookupValue("directory", itsDirectory);

    int maxparameters = static_cast<int>(itsMaxParameters);
    if (theSetting.lookupValue("max_parameters", maxparameters) && maxparameters < 1)
      throw Fmi::Exception(BCP, "Setting 'gridding.max_parameters' must be at least 1");
    itsMaxParameters = static_cast<std::size_t>(maxparameters);

    if (theSetting.exists("allowed_sources"))
    {
      const auto& sources = theSetting.lookup("allowed_sources");
      if (!sources.isArray())
        throw Fmi::Exception(BCP, "Setting 'gridding.allowed_sources' must be an array of strings");
      for (int i = 0; i < sources.getLength(); i++)
        itsAllowedSources.emplace_back(static_cast<const char*>(sources[i]));
    }

    if (theSetting.exists("areas"))
    {
      const auto& areas = theSetting.lookup("areas");
      if (!areas.isGroup())
        throw Fmi::Exception(BCP, "Setting 'gridding.areas' must be a group");
      for (int i = 0; i < areas.getLength(); i++)
        parseArea(areas[i]);
    }

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
 * \brief Parse a named area
 */
// ----------------------------------------------------------------------

void Config::parseArea(const libconfig::Setting& theSetting)
{
  AreaConfig area;
  area.name = theSetting.getName();
  try
  {
    if (!theSetting.isGroup())
      throw Fmi::Exception(BCP, "Area setting must be a group");
    parse_area_settings(LibconfigSettings(theSetting), area);
    if (!itsAreas.insert(std::make_pair(area.name, area)).second)
      throw Fmi::Exception(BCP, "Duplicate area");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Invalid gridding area").addParameter("Area", area.name);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse the settings of one producer of the configuration file
 *
 * The producer has the area settings itself, and an area of its own with its name.
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

    LibconfigSettings settings(theSetting);
    AreaConfig area;
    area.name = producer.name;
    parse_area_settings(settings, area);
    if (!itsAreas.insert(std::make_pair(area.name, area)).second)
      throw Fmi::Exception(BCP, "The producer has the name of an area");

    producer.area = area.name;
    producer.hash = Fmi::hash_value(producer.name);
    parse_data_settings(settings, producer, !area.seaFraction.empty());

    if (!itsProducers.insert(std::make_pair(producer.name, producer)).second)
      throw Fmi::Exception(BCP, "Duplicate producer");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Invalid gridded producer configuration")
        .addParameter("Producer", producer.name);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parse a definition file of the gridding directory
 *
 * Without variants the file defines one producer, named after the file. With variants every
 * variant is the definition with the variant's overrides (title, area and parameter settings)
 * merged in, and "default" names the variant the plain name means. A variant cannot change the
 * source, the times or the parameters themselves, so that all variants grid the same
 * observations.
 */
// ----------------------------------------------------------------------

Definition Config::parseDefinition(const Json::Value& theJson, const std::string& theName) const
{
  try
  {
    if (!valid_name(theName))
      throw Fmi::Exception(BCP, "Invalid gridded producer name '" + theName + "'");
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "The definition must be a JSON object");
    JsonSettings(theJson).checkKnown(kDefinitionSettings);

    // The JSON of each variant, by name ("" for the definition without variants)
    std::map<std::string, Json::Value> variants;
    std::string defaultName;

    if (theJson.isMember("variants"))
    {
      const auto& list = theJson["variants"];
      if (!list.isArray() || list.empty())
        throw Fmi::Exception(BCP, "Setting 'variants' must be a nonempty array");
      for (const auto& v : list)
      {
        if (!v.isObject())
          throw Fmi::Exception(BCP, "A variant must be an object");
        JsonSettings(v).checkKnown(kVariantSettings);
        if (!v.isMember("name") || !v["name"].isString() || !valid_name(v["name"].asString()))
          throw Fmi::Exception(BCP, "Every variant needs a name of letters, digits, '_' and '-'");
        const auto name = v["name"].asString();
        if (variants.count(name) > 0)
          throw Fmi::Exception(BCP, "Duplicate variant '" + name + "'");
        variants[name] = merge_variant(theJson, v);
      }
      if (theJson.isMember("default"))
      {
        if (!theJson["default"].isString())
          throw Fmi::Exception(BCP, "Setting 'default' must be the name of a variant");
        defaultName = theJson["default"].asString();
      }
      else if (variants.size() == 1)
        defaultName = variants.begin()->first;
      else
        throw Fmi::Exception(BCP, "Setting 'default' is required with two or more variants");
      if (variants.count(defaultName) == 0)
        throw Fmi::Exception(BCP, "The default variant '" + defaultName + "' does not exist");
    }
    else
    {
      if (theJson.isMember("default"))
        throw Fmi::Exception(BCP, "Setting 'default' needs variants");
      variants[""] = theJson;
    }

    Definition definition;
    for (const auto& item : variants)
    {
      const auto& variant = item.first;
      const auto& json = item.second;
      try
      {
        JsonSettings settings(json);
        auto producer = std::make_shared<ProducerConfig>();
        producer->name = variant.empty() ? theName : theName + "@" + variant;
        auto area = settings.string("area");
        if (!area)
          throw Fmi::Exception(BCP, "Setting 'area' is required");
        auto pos = itsAreas.find(*area);
        if (pos == itsAreas.end())
          throw Fmi::Exception(BCP, "Unknown gridding area '" + *area + "'");
        producer->area = *area;
        parse_data_settings(settings, *producer, !pos->second.seaFraction.empty());

        if (producer->parameters.size() > itsMaxParameters)
          throw Fmi::Exception(BCP,
                               fmt::format("At most {} parameters are allowed", itsMaxParameters));
        if (!itsAllowedSources.empty() &&
            std::find(itsAllowedSources.begin(), itsAllowedSources.end(), producer->sourceProducer) ==
                itsAllowedSources.end())
          throw Fmi::Exception(BCP, "Source producer '" + producer->sourceProducer + "' is not allowed");

        producer->hash = definition_hash(json);

        if (variant.empty())
          definition.defaultVariant = producer;
        else
        {
          definition.variants[variant] = producer;
          if (variant == defaultName)
          {
            // The plain name, with the same settings and hence the same cached data
            auto plain = std::make_shared<ProducerConfig>(*producer);
            plain->name = theName;
            definition.defaultVariant = plain;
          }
        }
      }
      catch (...)
      {
        auto e = Fmi::Exception::Trace(BCP, "Invalid variant");
        if (!variant.empty())
          e.addParameter("Variant", variant);
        throw e;
      }
    }
    return definition;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Invalid gridded producer definition")
        .addParameter("Producer", theName);
  }
}

}  // namespace Gridding
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
