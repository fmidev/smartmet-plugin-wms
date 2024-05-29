#include "Properties.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
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

void Properties::init(Json::Value& theJson, const State& theState, const Config& theConfig)
{
  try
  {
    JsonTools::remove_string(language, theJson, "language", theConfig.defaultLanguage());
    JsonTools::remove_string(producer, theJson, "producer", theConfig.defaultModel());
    JsonTools::remove_int(timestep, theJson, "timestep");
    JsonTools::remove_int(forecastType, theJson, "forecastType");
    JsonTools::remove_int(forecastNumber, theJson, "forecastNumber");
    JsonTools::remove_uint(geometryId, theJson, "geometryId");

    JsonTools::remove_time(origintime, theJson, "origintime");
    const auto& zones = theState.getGeoEngine().getTimeZones();
    JsonTools::remove_tz(tz, theJson, "tz", zones);
    JsonTools::remove_time(time, theJson, "time", tz);  // after tz handling

    JsonTools::remove_int(time_offset, theJson, "time_offset");
    JsonTools::remove_int(interval_start, theJson, "interval_start");
    JsonTools::remove_int(interval_end, theJson, "interval_end");

    // Margin overrides xmargin and ymargin
    JsonTools::remove_int(xmargin, theJson, "xmargin");
    JsonTools::remove_int(ymargin, theJson, "ymargin");

    auto json = JsonTools::remove(theJson, "margin");
    if (!json.isNull())
    {
      xmargin = json.asInt();
      ymargin = xmargin;
    }

    JsonTools::remove_bool(clip, theJson, "clip");

    // Use external elevation if given
    JsonTools::remove_double(level, theJson, "level");
    if (theState.getRequest().getParameter("elevation"))
    {
      level = std::stod(*theState.getRequest().getParameter("elevation"));
      //elevation_unit = "m";
    }

    if (theState.getRequest().getParameter("elevation_unit"))
      JsonTools::remove_string(elevation_unit, theJson, "elevation_unit");

    JsonTools::remove_double(pressure, theJson, "pressure");
    JsonTools::remove_int(levelId, theJson, "levelid");

    JsonTools::remove_string(source, theJson, "source");
    if (!source)
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
        source = theConfig.primaryForecastSource();
    }

    json = JsonTools::remove(theJson, "projection");
    projection.init(json, theState, theConfig);
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

void Properties::init(Json::Value& theJson,
                      const State& theState,
                      const Config& theConfig,
                      const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      return;

    JsonTools::remove_string(language, theJson, "language", theProperties.language);
    JsonTools::remove_string(source, theJson, "source", theProperties.source);
    JsonTools::remove_string(producer, theJson, "producer", theProperties.producer);
    JsonTools::remove_int(timestep, theJson, "timestep", theProperties.timestep);
    JsonTools::remove_int(forecastType, theJson, "forecastType", theProperties.forecastType);
    JsonTools::remove_int(forecastNumber, theJson, "forecastNumber", theProperties.forecastNumber);
    JsonTools::remove_uint(geometryId, theJson, "geometryId", theProperties.geometryId);

    JsonTools::remove_time(origintime, theJson, "origintime", theProperties.origintime);
    const auto& zones = theState.getGeoEngine().getTimeZones();
    JsonTools::remove_tz(tz, theJson, "tz", zones, theProperties.tz);
    JsonTools::remove_time(time, theJson, "time", tz, theProperties.time);  // after tz

    JsonTools::remove_int(time_offset, theJson, "time_offset", theProperties.time_offset);
    JsonTools::remove_int(interval_start, theJson, "interval_start", theProperties.interval_start);
    JsonTools::remove_int(interval_end, theJson, "interval_end", theProperties.interval_end);

    projection = theProperties.projection;
    auto json = JsonTools::remove(theJson, "projection");
    if (!json.isNull())
      projection.init(json, theState, theConfig);

    // Here a single margin setting overrides xmargin and ymargin.

    JsonTools::remove_int(xmargin, theJson, "xmargin", theProperties.xmargin);
    JsonTools::remove_int(ymargin, theJson, "ymargin", theProperties.ymargin);

    json = JsonTools::remove(theJson, "margin");
    if (!json.isNull())
    {
      xmargin = json.asInt();
      ymargin = xmargin;
    }

    JsonTools::remove_bool(clip, theJson, "clip", theProperties.clip);

    // Use external elevation if given
    if (theState.getRequest().getParameter("elevation"))
    {
      level = std::stod(*theState.getRequest().getParameter("elevation"));
      //elevation_unit = "m";
    }
    else
      JsonTools::remove_double(level, theJson, "level", theProperties.level);

    if (theState.getRequest().getParameter("elevation_unit"))
      JsonTools::remove_string(elevation_unit, theJson, "elevation_unit");

    JsonTools::remove_double(pressure, theJson, "pressure", theProperties.pressure);
    JsonTools::remove_int(levelId, theJson, "levelId", theProperties.levelId);
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

void Properties::setValidTime(const Fmi::DateTime& theTime)
{
  time = theTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish valid time if not set already
 */
// ----------------------------------------------------------------------

Fmi::DateTime Properties::getValidTime(const Fmi::DateTime& theDefault) const
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

Fmi::DateTime Properties::getValidTime() const
{
  if (!time)
    throw Fmi::Exception(BCP, "Time has not been set for all layers");

  auto valid_time = *time;
  if (time_offset)
    valid_time += Fmi::Minutes(*time_offset);

  return valid_time;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the time period for the layer
 */
// ----------------------------------------------------------------------

Fmi::TimePeriod Properties::getValidTimePeriod() const
{
  auto tstart = getValidTime();
  auto tend = tstart;

  if (interval_start)
    tstart -= Fmi::Minutes(*interval_start);

  if (interval_end)
    tend += Fmi::Minutes(*interval_end);

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
    Fmi::hash_combine(hash, Fmi::hash_value(elevation_unit));
    Fmi::hash_combine(hash, Fmi::hash_value(pressure));
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
