#include "Patterns.h"
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

void Patterns::init(const Json::Value& theJson, const State& theState)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto patterns_members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, patterns_members)
    {
      const Json::Value& pattern_json = theJson[name];

      if (!pattern_json.isString())
        throw Spine::Exception(BCP,
                               "Invalid object type in defs.patterns initialization, expecting "
                               "name-value pair for pattern '" +
                                   name + "'");

      std::string value = Spine::HTTP::urldecode(pattern_json.asString());
      if (value.substr(0, 6) != "data:,")
        throw Spine::Exception(BCP,
                               "Only RFC2397 data-URLs supported: URL incorrect '" + value +
                                   "' for pattern '" + name + "'");
      value = value.substr(6);  // Cut away data:,
      if (theState.setPattern(name, value) == false)
        throw Spine::Exception(BCP, "defs.patterns pattern '" + name + "' defined multiple times");
      patterns[name] = value;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
