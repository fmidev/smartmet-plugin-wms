#include "AttributeSelection.h"
#include "Config.h"
#include "Hash.h"
#include <boost/foreach.hpp>
#include <spine/Exception.h>
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
      throw Spine::Exception(
          BCP, "Attribute depends both on a single value and upper and/or lower limit");

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

void AttributeSelection::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Arrows JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

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
      else if (name == "qid")
      {  // ignored so that f.ex. SymbolLayer may reuse Isobands
      }
      else
        throw Spine::Exception(BCP,
                               "Attribute selection does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    auto hash = Dali::hash_value(value);
    boost::hash_combine(hash, Dali::hash_value(lolimit));
    boost::hash_combine(hash, Dali::hash_value(hilimit));
    boost::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
