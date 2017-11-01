#include "LegendSymbols.h"
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
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void LegendSymbols::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Legend-layer symbols JSON must be a map");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (name == "css")
        css = json.asString();
      else if (name == "symbol")
        symbol = json.asString();
      else if (name == "start")
        start = json.asString();
      else if (name == "end")
        end = json.asString();
      else if (name == "missing")
        missing = json.asString();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else
        throw Spine::Exception(BCP,
                               "Legend-layer symbols do not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t LegendSymbols::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_css(css, theState);
    boost::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    boost::hash_combine(hash, Dali::hash_symbol(start, theState));
    boost::hash_combine(hash, Dali::hash_symbol(end, theState));
    boost::hash_combine(hash, Dali::hash_symbol(missing, theState));
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
