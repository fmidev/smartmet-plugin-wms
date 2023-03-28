#include "Patterns.h"
#include "Config.h"
#include "Hash.h"

#include <macgyver/Exception.h>
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

void Patterns::init(Json::Value& theJson, const State& theState)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto patterns_members = theJson.getMemberNames();
    for (const auto& name : patterns_members)
    {
      const Json::Value& pattern_json = theJson[name];

      if (!pattern_json.isString())
        throw Fmi::Exception(BCP,
                             "Invalid object type in defs.patterns initialization, expecting "
                             "name-value pair for pattern '" +
                                 name + "'");

      std::string value = Spine::HTTP::urldecode(pattern_json.asString());
      if (value.substr(0, 6) != "data:,")
        throw Fmi::Exception(BCP,
                             "Only RFC2397 data-URLs supported: URL incorrect '" + value +
                                 "' for pattern '" + name + "'");
      value = value.substr(6);  // Cut away data:,
      if (!theState.setPattern(name, value))
        throw Fmi::Exception(BCP, "defs.patterns pattern '" + name + "' defined multiple times");
      patterns[name] = value;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
