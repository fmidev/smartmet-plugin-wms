#include "Properties.h"
#include "Config.h"
#include "Hash.h"
#include "Time.h"
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/Exception.h>
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize a JSON attribute
 */
// ----------------------------------------------------------------------

void Properties::init(const Json::Value& theJson, const State& theState, const Config& theConfig)
{
  try
  {
    // if (!theJson.isObject())
    //  return;

    Json::Value nulljson;

    auto json = theJson.get("language", theConfig.defaultLanguage());
    language = json.asString();

    json = theJson.get("producer", theConfig.defaultModel());
    producer = json.asString();

    json = theJson.get("timestep", nulljson);
    if (!json.isNull())
      timestep = json.asInt();

    json = theJson.get("source", nulljson);
    if (!json.isNull())
    {
      source = json.asString();
    }
    else
    {
      if (theConfig.primaryForecastSource() == "grid")
        source = "grid";
      else if (theConfig.primaryForecastSource().empty() && producer)
      {
        const auto* gridEngine = theState.getGridEngine();
        if (gridEngine && gridEngine->isEnabled() && gridEngine->isGridProducer(*producer))
          source = "grid";
      }
      else
      {
        source = theConfig.primaryForecastSource();
      }
    }

    json = theJson.get("forecastType", nulljson);
    if (!json.isNull())
      forecastType = json.asInt();

    json = theJson.get("forecastNumber", nulljson);
    if (!json.isNull())
      forecastNumber = json.asInt();

    json = theJson.get("geometryId", nulljson);
    if (!json.isNull())
      geometryId = json.asInt();

    json = theJson.get("tz", nulljson);
    if (json.isString())
      tz = parse_timezone(json.asString(), theState.getGeoEngine().getTimeZones());
    else if (!json.isNull())
      throw Fmi::Exception(BCP, "Failed to parse tz setting: '" + json.asString());

    json = theJson.get("origintime", nulljson);
    if (json.isString())
      origintime = parse_time(json.asString());
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      origintime = parse_time(Fmi::to_string(tmp));
    }
    else if (!json.isNull())
      throw Fmi::Exception(BCP, "Failed to parse origintime setting: '" + json.asString());

    json = theJson.get("time", nulljson);
    if (json.isString())
      time = parse_time(json.asString(), tz);
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      time = parse_time(Fmi::to_string(tmp), tz);
    }
    else if (!json.isNull())
      throw Fmi::Exception(BCP, "Failed to parse time setting: '" + json.asString());

    json = theJson.get("time_offset", nulljson);
    if (!json.isNull())
      time_offset = json.asInt();

    json = theJson.get("interval_start", nulljson);
    if (!json.isNull())
      interval_start = json.asInt();

    json = theJson.get("interval_end", nulljson);
    if (!json.isNull())
      interval_end = json.asInt();

    json = theJson.get("projection", nulljson);
    if (!json.isNull())
      projection.init(json, theState, theConfig);

    json = theJson.get("margin", nulljson);
    if (!json.isNull())
    {
      xmargin = json.asInt();
      ymargin = xmargin;
    }
    else
    {
      json = theJson.get("xmargin", nulljson);
      if (!json.isNull())
        xmargin = json.asInt();

      json = theJson.get("ymargin", nulljson);
      if (!json.isNull())
        ymargin = json.asInt();
    }

    json = theJson.get("clip", nulljson);
    if (!json.isNull())
      clip = json.asBool();

    // Use external elevation if given
    if (theState.getRequest().getParameter("elevation"))
      level = std::stod(*theState.getRequest().getParameter("elevation"));
    else
    {
      json = theJson.get("level", nulljson);
      if (!json.isNull())
        level = json.asDouble();
    }

    json = theJson.get("levelId", nulljson);
    if (!json.isNull())
      levelId = json.asInt();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize a JSON attribute
 */
// ----------------------------------------------------------------------

void Properties::init(const Json::Value& theJson,
                      const State& theState,
                      const Config& theConfig,
                      const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      return;

    Json::Value nulljson;

    auto json = theJson.get("language", nulljson);
    if (json.isNull())
      language = theProperties.language;
    else
      language = json.asString();

    json = theJson.get("source", nulljson);
    if (json.isNull())
      source = theProperties.source;
    else
      source = json.asString();

    json = theJson.get("producer", nulljson);
    if (json.isNull())
      producer = theProperties.producer;
    else
      producer = json.asString();

    json = theJson.get("timestep", nulljson);
    if (json.isNull())
      timestep = theProperties.timestep;
    else
      timestep = json.asInt();

    json = theJson.get("forecastType", nulljson);
    if (json.isNull())
      forecastType = theProperties.forecastType;
    else
      forecastType = json.asInt();

    json = theJson.get("forecastNumber", nulljson);
    if (json.isNull())
      forecastNumber = theProperties.forecastNumber;
    else
      forecastNumber = json.asInt();

    json = theJson.get("tz", nulljson);
    if (json.isString())
      tz = parse_timezone(json.asString(), theState.getGeoEngine().getTimeZones());
    else if (json.isNull())
      tz = theProperties.tz;
    else
      throw Fmi::Exception(BCP, "Failed to parse tz setting: '" + json.asString());

    json = theJson.get("origintime", nulljson);
    if (json.isString())
      origintime = parse_time(json.asString());
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      origintime = parse_time(Fmi::to_string(tmp));
    }
    else if (json.isNull())
      origintime = theProperties.origintime;
    else
      throw Fmi::Exception(BCP, "Failed to parse origintime setting: '" + json.asString());

    json = theJson.get("time", nulljson);
    if (json.isString())
      time = parse_time(json.asString(), tz);
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      time = parse_time(Fmi::to_string(tmp), tz);
    }
    else if (json.isNull())
      time = theProperties.time;
    else
      throw Fmi::Exception(BCP, "Failed to parse time setting: '" + json.asString());

    json = theJson.get("time_offset", nulljson);
    if (json.isNull())
      time_offset = theProperties.time_offset;
    else
      time_offset = json.asInt();

    json = theJson.get("interval_start", nulljson);
    if (json.isNull())
      interval_start = theProperties.interval_start;
    else
      interval_start = json.asInt();

    json = theJson.get("interval_end", nulljson);
    if (json.isNull())
      interval_end = theProperties.interval_end;
    else
      interval_end = json.asInt();

    projection = theProperties.projection;
    json = theJson.get("projection", nulljson);
    if (!json.isNull())
      projection.init(json, theState, theConfig);

    // Propagating margins is a bit more complicated. Here
    // a single margin setting overrides xmargin and ymargin.

    json = theJson.get("margin", nulljson);
    if (!json.isNull())
    {
      xmargin = json.asInt();
      ymargin = json.asInt();
    }
    else
    {
      json = theJson.get("xmargin", nulljson);
      if (json.isNull())
        xmargin = theProperties.xmargin;
      else
        xmargin = json.asInt();

      json = theJson.get("ymargin", nulljson);
      if (json.isNull())
        ymargin = theProperties.ymargin;
      else
        ymargin = json.asInt();
    }

    json = theJson.get("clip", nulljson);
    if (json.isNull())
      clip = theProperties.clip;
    else
      clip = json.asBool();

    // Use external elevation if given
    if (theState.getRequest().getParameter("elevation"))
      level = std::stod(*theState.getRequest().getParameter("elevation"));
    else
    {
      json = theJson.get("level", nulljson);
      if (json.isNull())
        level = theProperties.level;
      else
        level = json.asDouble();
    }

    json = theJson.get("levelId", nulljson);
    if (json.isNull())
      levelId = theProperties.levelId;
    else
      levelId = json.asInt();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test whether a valid time has been set
 */
// ----------------------------------------------------------------------

bool Properties::hasValidTime() const
{
  return !!time;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the valid time
 */
// ----------------------------------------------------------------------

void Properties::setValidTime(const boost::posix_time::ptime& theTime)
{
  time = theTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish valid time if not set already
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime Properties::getValidTime(const boost::posix_time::ptime& theDefault) const
{
  if (!time)
    return theDefault;
  return getValidTime();
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the time for the layer
 *
 * This method is not used by layers which require no specific time.
 * The layers which do require one call this and get an exception if
 * no time has been specified.
 */
// ----------------------------------------------------------------------

boost::posix_time::ptime Properties::getValidTime() const
{
  if (!time)
    throw Fmi::Exception(BCP, "Time has not been set for all layers");

  auto valid_time = *time;
  if (time_offset)
    valid_time += boost::posix_time::minutes(*time_offset);

  return valid_time;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the time period for the layer
 */
// ----------------------------------------------------------------------

boost::posix_time::time_period Properties::getValidTimePeriod() const
{
  auto tstart = getValidTime();
  auto tend = tstart;

  if (interval_start)
    tstart -= boost::posix_time::minutes(*interval_start);

  if (interval_end)
    tend += boost::posix_time::minutes(*interval_end);

  return {tstart, tend};
}

// ----------------------------------------------------------------------
/*!
 * \brief Test whether the given pixel coordinate is within pixel box bounds
 */
// ----------------------------------------------------------------------

bool Properties::inside(const Fmi::Box& theBox, double theX, double theY) const
{
  return ((theX >= -xmargin) && (theX <= theBox.width() + xmargin) && (theY >= -ymargin) &&
          (theY <= theBox.height() + ymargin));
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Properties::hash_value(const State& theState) const
{
  try
  {
    auto hash = Fmi::hash_value(language);
    Fmi::hash_combine(hash, Fmi::hash_value(producer));
    Fmi::hash_combine(hash, Fmi::hash_value(source));
    Fmi::hash_combine(hash, Fmi::hash_value(forecastType));
    Fmi::hash_combine(hash, Fmi::hash_value(forecastNumber));
    Fmi::hash_combine(hash, Fmi::hash_value(geometryId));
    Fmi::hash_combine(hash, Fmi::hash_value(level));
    Fmi::hash_combine(hash, Fmi::hash_value(levelId));
    Fmi::hash_combine(hash, Fmi::hash_value(origintime));
    // timezone is irrelevant, time is always in UTC timen
    Fmi::hash_combine(hash, Fmi::hash_value(time));
    Fmi::hash_combine(hash, Fmi::hash_value(time_offset));
    Fmi::hash_combine(hash, Fmi::hash_value(interval_start));
    Fmi::hash_combine(hash, Fmi::hash_value(interval_end));
    Fmi::hash_combine(hash, Fmi::hash_value(xmargin));
    Fmi::hash_combine(hash, Fmi::hash_value(ymargin));
    Fmi::hash_combine(hash, Fmi::hash_value(clip));
    Fmi::hash_combine(hash, Dali::hash_value(projection, theState));

    if (source && *source == "grid")
    {
      const auto* gridEngine = theState.getGridEngine();
      if (!gridEngine || !gridEngine->isEnabled())
        throw Fmi::Exception(BCP, "The grid-engine is disabled!");

      std::string producerName = gridEngine->getProducerName(*producer);
      auto pHash = gridEngine->getProducerHash(producerName);
      Fmi::hash_combine(hash, Fmi::hash_value(pHash));
    }

    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to calculate hash value for layer properties!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
