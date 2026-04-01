#include "LegendLabels.h"
#include "Config.h"
#include "Hash.h"

#include <fmt/printf.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <stdexcept>

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

void LegendLabels::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Legend-layer labels JSON must be a map");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

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
        const auto convmembers = json.getMemberNames();
        for (const auto& convname : convmembers)
        {
          const Json::Value& label_json = json[convname];
          if (label_json.isString())
            conversions[convname] = label_json.asString();
          else if (label_json.isObject())
          {
            const auto languages = label_json.getMemberNames();
            for (const auto& lang : languages)
            {
              const Json::Value& lang_json = label_json[lang];
              if (!lang_json.isString())
                throw Fmi::Exception(BCP,
                                     "Legend layer conversion '" + convname +
                                         "' value for a language must be a string");
              conversions[lang + ":" + convname] = lang_json.asString();
            }
          }
          else
            throw Fmi::Exception(
                BCP, "Legend layer conversion '" + convname + "' value must be a string or a map");
        }
      }
      else
        throw Fmi::Exception(BCP, "Legend-layer labels do not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a value and look it up in conversions
 */
// ----------------------------------------------------------------------

std::string LegendLabels::findConversion(double theValue,
                                         const std::optional<std::string>& theLanguage) const
{
  try
  {
    if (conversions.empty())
      return {};

    // Format the value the same way as LegendLayer does for label text
    std::string text;
    if (!format)
    {
      text = Fmi::to_string(theValue);
    }
    else
    {
      // Format and strip trailing zeros (mirrors LegendLayer::pretty())
      std::string ret = fmt::sprintf(format->c_str(), theValue);
      std::size_t pos = ret.size();
      while (pos > 0 && ret[--pos] == '0')
      {
      }
      if (ret[pos] != ',' && ret[pos] != '.')
        text = ret;
      else
      {
        text = ret.substr(0, pos);
        if (text == "-0")
          text = "0";
      }
    }

    // Try language-specific conversion first
    if (theLanguage)
    {
      const auto it = conversions.find(*theLanguage + ":" + text);
      if (it != conversions.end())
        return it->second;
    }

    // Fall back to language-neutral conversion
    const auto it = conversions.find(text);
    if (it != conversions.end())
      return it->second;

    return {};
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
    auto hash = Fmi::hash_value(type);
    Fmi::hash_combine(hash, Fmi::hash_value(format));
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Fmi::hash_value(separator));
    Fmi::hash_combine(hash, Fmi::hash_value(conversions));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
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
