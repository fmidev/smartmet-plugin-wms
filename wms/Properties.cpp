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

    json = theJson.get("time", nulljson);
    if (json.isString())
      time = parse_time(json.asString());
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      time = parse_time(Fmi::to_string(tmp));
    }
    else if (!json.isNull())
      throw SmartMet::Spine::Exception(BCP, "Failed to parse time setting: '" + json.asString());

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
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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

    json = theJson.get("time", nulljson);
    if (json.isString())
      time = parse_time(json.asString());
    else if (json.isUInt64())
    {
      // A timestamp may look like an integer in a query string
      std::size_t tmp = json.asUInt64();
      time = parse_time(Fmi::to_string(tmp));
    }
    else if (json.isNull())
      time = theProperties.time;
    else
      throw SmartMet::Spine::Exception(BCP, "Failed to parse time setting: '" + json.asString());

    json = theJson.get("time_offset", nulljson);
    if (json.isNull())
      time_offset = theProperties.time_offset;
    else
      time_offset = json.asInt();

    projection = theProperties.projection;
    json = theJson.get("projection", nulljson);
    if (!json.isNull())
      projection.init(json, theState, theConfig);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
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
    boost::hash_combine(hash, Dali::hash_value(producer));
    boost::hash_combine(hash, Dali::hash_value(time));
    boost::hash_combine(hash, Dali::hash_value(time_offset));
    boost::hash_combine(hash, Dali::hash_value(interval_start));
    boost::hash_combine(hash, Dali::hash_value(interval_end));
    boost::hash_combine(hash, Dali::hash_value(projection, theState));
    return hash;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(
        BCP, "Failed to calculate hash value for layer properties!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
