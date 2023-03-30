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
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "MapStyles JSON is not a JSON object");

    // Extract member values

    JsonTools::remove_string(field, theJson, "field");
    JsonTools::remove_string(parameter, theJson, "parameter");

    if (field.empty())
      throw Fmi::Exception(BCP, "Database field must be set when maps are styled");
    if (parameter.empty())
      throw Fmi::Exception(BCP, "Forecast parameter must be set when maps are styled");

    auto json = JsonTools::remove(theJson, "attributes");
    if (json.isNull())
      throw Fmi::Exception(BCP, "Attributes must be set when styling map components");
    JsonTools::extract_array("attributes", data_attributes, json, theConfig);

    // features-setting can be missing. Then we assume the feature value matches the station number
    json = JsonTools::remove(theJson, "features");
    if (!json.isNull())
    {
      if (!json.isArray())
        throw Fmi::Exception(BCP,
                             "Feature mapping from database fields to stations must be an array");

      for (auto& j : json)
      {
        std::string name;
        boost::optional<int> number;

        JsonTools::remove_string(name, j, "value");
        JsonTools::remove_int(number, j, "number");

        if (name.empty())
          throw Fmi::Exception(BCP, "'value' setting not set for a map style setting");

        if (!number)
          throw Fmi::Exception(BCP, "'number' setting missing for a map style setting");

        features[name] = *number;

        auto jattributes = JsonTools::remove(j, "attributes");
        if (!jattributes.isNull())
          feature_attributes[name].init(jattributes, theConfig);

        if (j.size() > 0)
        {
          auto names = j.getMemberNames();
          auto namelist = boost::algorithm::join(names, ",");
          throw Fmi::Exception(BCP, "Unknown mapstyles feature settings: " + namelist);
        }
      }
    }

    if (theJson.size() > 0)
    {
      auto names = theJson.getMemberNames();
      auto namelist = boost::algorithm::join(names, ",");
      throw Fmi::Exception(BCP, "Unknown mapstyles settings: " + namelist);
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
