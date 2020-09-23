#include "Filters.h"
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

void Filters::init(const Json::Value& theJson, const State& theState)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto filters_members = theJson.getMemberNames();
    for (const auto& name : filters_members)
    {
      const Json::Value& filter_json = theJson[name];

      if (!filter_json.isString())
        throw Fmi::Exception(BCP,
                               "Invalid object type in defs.filters initialization, expecting "
                               "name-value pair for filter '" +
                                   name + "'");

      std::string value = Spine::HTTP::urldecode(filter_json.asString());
      if (value.substr(0, 6) != "data:,")
        throw Fmi::Exception(BCP,
                               "Only RFC2397 data-URLs supported: URL incorrect '" + value +
                                   "' for filter '" + name + "'");
      value = value.substr(6);  // Cut away data:,
      if (!theState.setFilter(name, value))
        throw Fmi::Exception(BCP, "defs.filters filter '" + name + "' defined multiple times");
      filters[name] = value;
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
