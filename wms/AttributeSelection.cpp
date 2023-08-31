#include "AttributeSelection.h"
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
 * \brief Return true if the given value matches the selection
 */
// ----------------------------------------------------------------------

bool AttributeSelection::matches(double theValue) const
{
  try
  {
    if (value && (lolimit || hilimit))
      throw Fmi::Exception(BCP,
                           "Attribute depends both on a single value and upper and/or lower limit");

    if (value)
      return (theValue == *value);

    if (lolimit && (theValue < *lolimit))
      return false;
    if (hilimit && (theValue >= *hilimit))
      return false;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove and return an attribute
 */
// ----------------------------------------------------------------------

boost::optional<std::string> AttributeSelection::remove(const std::string& theName)
{
  return attributes.remove(theName);
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void AttributeSelection::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Arrows JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "value")
        value = json.asDouble();
      else if (name == "lolimit")
        lolimit = json.asDouble();
      else if (name == "hilimit")
        hilimit = json.asDouble();
      else if (name == "symbol")
        symbol = json.asString();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else if (name == "translation")
		{
		  const auto languages = json.getMemberNames();
		  for (const auto& language : languages)
			{
			  auto translation_json = json[language];
			  translations[language] = translation_json.asString();
			}
		}
      else if (name == "qid")
      {  // ignored so that f.ex. SymbolLayer may reuse Isobands
      }
      else
        throw Fmi::Exception(BCP,
                             "Attribute selection does not have a setting named '" + name + "'");
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

std::size_t AttributeSelection::hash_value(const State& theState) const
{
  try
  {
    std::size_t hash = 0;
    Fmi::hash_combine(hash, Fmi::hash_value(value));
    Fmi::hash_combine(hash, Fmi::hash_value(lolimit));
    Fmi::hash_combine(hash, Fmi::hash_value(hilimit));
    Fmi::hash_combine(hash, Dali::hash_symbol(symbol, theState));
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
