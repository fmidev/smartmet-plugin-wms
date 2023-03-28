#include "MapStyles.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
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
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void MapStyles::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "MapStyles JSON is not a JSON object");

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("field", nulljson);
    if (!json.isNull())
      field = json.asString();
    else
      throw Fmi::Exception(BCP, "Database field must be set when maps are styled");

    json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();
    else
      throw Fmi::Exception(BCP, "Forecast parameter must be set when maps are styled");

    json = theJson.get("attributes", nulljson);
    if (json.isNull())
      throw Fmi::Exception(BCP, "Attributes must be set when styling map components");
    JsonTools::extract_array("attributes", data_attributes, json, theConfig);

    // features-setting can be missing. Then we assume the feature value matches the station number
    json = theJson.get("features", nulljson);
    if (!json.isNull())
    {
      if (!json.isArray())
        throw Fmi::Exception(BCP,
                             "Feature mapping from database fields to stations must be an array");

      for (const auto& j : json)
      {
        auto jvalue = j.get("value", nulljson);
        if (jvalue.isNull())
          throw Fmi::Exception(BCP, "'value' setting missing for a map style setting");

        auto jnumber = j.get("number", nulljson);
        if (jnumber.isNull())
          throw Fmi::Exception(BCP, "'number' setting missing for a map style setting");

        features[jvalue.asString()] = jnumber.asInt();

        auto jattributes = j.get("attributes", nulljson);
        if (!jattributes.isNull())
          feature_attributes[jvalue.asString()].init(jattributes, theConfig);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t MapStyles::hash_value(const State& theState) const
{
  try
  {
    auto hash = Fmi::hash_value(field);
    Fmi::hash_combine(hash, Fmi::hash_value(parameter));
    Fmi::hash_combine(hash, Fmi::hash_value(features));
    Fmi::hash_combine(hash, Dali::hash_value(feature_attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(data_attributes, theState));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
