#include "Symbols.h"
#include "Config.h"
#include "Hash.h"
#include <boost/foreach.hpp>
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <string>

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

void Symbols::init(const Json::Value& theJson, const State& theState)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto symbols_members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, symbols_members)
    {
      const Json::Value& symbol_json = theJson[name];

      if (!symbol_json.isString())
        throw Spine::Exception(BCP,
                               "Invalid object type in defs.symbols initialization, expecting "
                               "name-value pair for symbol '" +
                                   name + "'");

      std::string value = Spine::HTTP::urldecode(symbol_json.asString());
      if (value.substr(0, 6) != "data:,")
        throw Spine::Exception(BCP,
                               "Only RFC2397 data-URLs supported: URL incorrect '" + value +
                                   "' for symbol '" + name + "'");
      value = value.substr(6);  // Cut away data:,
      if (theState.setSymbol(name, value) == false)
        throw Spine::Exception(BCP, "defs.symbols symbol '" + name + "' defined multiple times");
      symbols[name] = value;
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
