#include "Text.h"
#include "Config.h"
#include "Hash.h"
#include <spine/Exception.h>
#include <boost/foreach.hpp>
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
 *
 * Allowed forms:
 *
 *   "text": "foobar";
 *	 "text": { "default": "foobar"; }
 *   "text": { "fi": "jotain"; "en": "foobar"; ... }
 *
 */
// ----------------------------------------------------------------------

void Text::init(const Json::Value& theJson, const Config& theConfig)
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
      throw SmartMet::Spine::Exception(BCP, "Text JSON must be a string or a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (!json.isString())
        throw SmartMet::Spine::Exception(
            BCP,
            "Text hash must consist of name string-value pairs, value of " + name +
                " is not a string");

      translations[name] = json.asString();
    }
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

std::size_t Text::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(attributes, theState);
    boost::hash_combine(hash, Dali::hash_value(translations));
    return hash;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Retrieve the text value
 */
// ----------------------------------------------------------------------

const std::string& Text::translate(const std::string& theLanguage) const
{
  try
  {
    // Use exact translation if there is one
    const auto match1 = translations.find(theLanguage);
    if (match1 != translations.end())
      return match1->second;

    // Return default translation if possible

    const auto match2 = translations.find("default");
    if (match2 != translations.end())
      return match2->second;

    // Error

    throw SmartMet::Spine::Exception(BCP, "No translation set for language '" + theLanguage + "'");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Retrieve the text value
 */
// ----------------------------------------------------------------------

const std::string& Text::translate(const boost::optional<std::string>& theLanguage) const
{
  try
  {
    if (theLanguage)
      return translate(*theLanguage);

    // Return default translation if possible

    const auto match = translations.find("default");
    if (match != translations.end())
      return match->second;

    // Error

    throw SmartMet::Spine::Exception(BCP, "Default text value missing when language is not set");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
