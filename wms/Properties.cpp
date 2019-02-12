#include "Properties.h"
#include "Config.h"
#include "Hash.h"
#include "Time.h"
#include <spine/Exception.h>

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
    if (!theJson.isObject())
      return;

    Json::Value nulljson;

    auto json = theJson.get("language", theConfig.defaultLanguage());
    language = json.asString();

    json = theJson.get("producer", theConfig.defaultModel());
    producer = json.asString();

    json = theJson.get("tz", nulljson);
    if (json.isString())
      tz = parse_timezone(json.asString(), theState.getGeoEngine().getTimeZones());
    else if (!json.isNull())
      throw Spine::Exception(BCP, "Failed to parse tz setting: '" + json.asString());

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
      throw Spine::Exception(BCP, "Failed to parse origintime setting: '" + json.asString());

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
      throw Spine::Exception(BCP, "Failed to parse time setting: '" + json.asString());

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
        xmargin = json.asInt();
    }

    json = theJson.get("clip", nulljson);
    if (!json.isNull())
      clip = json.asBool();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    json = theJson.get("producer", nulljson);
    if (json.isNull())
      producer = theProperties.producer;
    else
      producer = json.asString();

    json = theJson.get("tz", nulljson);
    if (json.isString())
      tz = parse_timezone(json.asString(), theState.getGeoEngine().getTimeZones());
    else if (json.isNull())
      tz = theProperties.tz;
    else
      throw Spine::Exception(BCP, "Failed to parse tz setting: '" + json.asString());

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
      throw Spine::Exception(BCP, "Failed to parse origintime setting: '" + json.asString());

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
      throw Spine::Exception(BCP, "Failed to parse time setting: '" + json.asString());

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
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
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
    throw Spine::Exception(BCP, "Time has not been set for all layers");

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
    auto hash = Dali::hash_value(language);
    Dali::hash_combine(hash, Dali::hash_value(producer));
    Dali::hash_combine(hash, Dali::hash_value(origintime));
    // timezone is irrelevant, time is always in UTC timen
    Dali::hash_combine(hash, Dali::hash_value(time));
    Dali::hash_combine(hash, Dali::hash_value(time_offset));
    Dali::hash_combine(hash, Dali::hash_value(interval_start));
    Dali::hash_combine(hash, Dali::hash_value(interval_end));
    Dali::hash_combine(hash, Dali::hash_value(xmargin));
    Dali::hash_combine(hash, Dali::hash_value(ymargin));
    Dali::hash_combine(hash, Dali::hash_value(clip));
    Dali::hash_combine(hash, Dali::hash_value(projection, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to calculate hash value for layer properties!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
