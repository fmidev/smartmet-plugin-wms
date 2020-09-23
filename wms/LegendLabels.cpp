#include "LegendLabels.h"
#include "Config.h"
#include "Hash.h"

#include <macgyver/Exception.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief The defaults assume a vertical legend
 */
// ----------------------------------------------------------------------

LegendLabels::LegendLabels() : type("range"), dx(30), dy(0), separator(u8"\u2013")  // en dash
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void LegendLabels::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Legend-layer labels JSON must be a map");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "type")
        type = json.asString();
      else if (name == "format")
        format = json.asString();
      else if (name == "dx")
        dx = json.asInt();
      else if (name == "dy")
        dy = json.asInt();
      else if (name == "separator")
        separator = json.asString();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else if (name == "conversions")
      {
        if (!json.isObject())
          throw Fmi::Exception(BCP, "legend-layer conversions setting in a must be a map");
        const auto members = json.getMemberNames();
        for (const auto& name : members)
        {
          const Json::Value& label_json = json[name];
          if (label_json.isString())
            conversions[name] = label_json.asString();
          else if (label_json.isObject())
          {
            const auto languages = label_json.getMemberNames();
            for (const auto& lang : languages)
            {
              const Json::Value& lang_json = label_json[lang];
              if (!lang_json.isString())
                throw Fmi::Exception(
                    BCP,
                    "Legend layer conversion '" + name + "' value for a language must be a string");
              conversions[lang + ":" + name] = lang_json.asString();
            }
          }
          else
            throw Fmi::Exception(
                BCP, "Legend layer conversion '" + name + "' value must be a string or a map");
        }
      }
      else
        throw Fmi::Exception(BCP,
                               "Legend-layer labels do not have a setting named '" + name + "'");
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

std::size_t LegendLabels::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(type);
    Dali::hash_combine(hash, Dali::hash_value(format));
    Dali::hash_combine(hash, Dali::hash_value(dx));
    Dali::hash_combine(hash, Dali::hash_value(dy));
    Dali::hash_combine(hash, Dali::hash_value(separator));
    Dali::hash_combine(hash, Dali::hash_value(conversions));
    Dali::hash_combine(hash, Dali::hash_value(attributes, theState));
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
