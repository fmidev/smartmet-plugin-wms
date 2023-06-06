#include "Text.h"
#include "Config.h"
#include "Hash.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// Create a tagged object with no initial translations
Text::Text(std::string name) : tag(std::move(name)) {}
Text::Text(const char* name) : tag(name) {}

// Create a tagged object with just one default translation
Text::Text(std::string name, const std::string& value) : tag(std::move(name))
{
  translations["default"] = value;
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 *
 * Allowed forms:
 *
 *   "text": "foobar";
 *   "text": { "default": "foobar" }
 *   "text": { "fi": "jotain", "en": "foobar" ... }
 *   "text": { "fi": "jotain", "en": "foobar", ... , "attributes": { ... } }
 *   "
 *
 */
// ----------------------------------------------------------------------

void Text::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    // Handle the case of a fixed text

    if (theJson.isString())
    {
      translations["default"] = theJson.asString();
      return;
    }

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Text JSON must be a string or a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "attributes")
        attributes.init(json, theConfig);
      else
      {
        if (!json.isString())
          throw Fmi::Exception(BCP,
                               "Text hash for " + tag +
                                   " must consist of name string-value pairs, value of " + name +
                                   " is not a string");

        translations[name] = json.asString();
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

std::size_t Text::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(attributes, theState);
    Fmi::hash_combine(hash, Fmi::hash_value(translations));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Retrieve the text value
 */
// ----------------------------------------------------------------------

std::string Text::translate(const std::string& theLanguage) const
{
  try
  {
    // Use exact translation if there is one
    const auto match1 = translations.find(theLanguage);
    if (match1 != translations.end())
      return Fmi::xmlescape(match1->second);

    // Return default translation if possible

    const auto match2 = translations.find("default");
    if (match2 != translations.end())
      return Fmi::xmlescape(match2->second);

    // Error
    throw Fmi::Exception(BCP,
                         "No translation set for '" + tag + "' in language '" + theLanguage +
                             "' (available:" + dump() + ")");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Retrieve the text value
 */
// ----------------------------------------------------------------------

std::string Text::translate(const boost::optional<std::string>& theLanguage) const
{
  try
  {
    if (theLanguage)
      return translate(*theLanguage);

    // Return default translation if possible

    const auto match = translations.find("default");
    if (match != translations.end())
      return Fmi::xmlescape(match->second);

    // Error

    throw Fmi::Exception(BCP, "Default text value missing when language is not set");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Dump translations as a string for debugging purposes
 */
// ----------------------------------------------------------------------

std::string Text::dump() const
{
  std::string ret;
  for (const auto& lang_trans : translations)
  {
    if (!ret.empty())
      ret += ' ';
    ret += (lang_trans.first + "='" + lang_trans.second + "'");
  }
  return ret;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
