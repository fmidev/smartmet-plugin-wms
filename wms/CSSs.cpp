#include "CSSs.h"
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

void CSSs::init(const Json::Value& theJson, const State& theState)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto csss_members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, csss_members)
    {
      const Json::Value& css_json = theJson[name];

      if (!css_json.isString())
        throw Spine::Exception(BCP,
                               "Invalid object type in defs.csss initialization, expecting "
                               "name-value pair for css '" +
                                   name + "'");

      std::string value = Spine::HTTP::urldecode(css_json.asString());
      if (value.substr(0, 6) != "data:,")
        throw Spine::Exception(BCP,
                               "Only RFC2397 data-URLs supported: URL incorrect '" + value +
                                   "' for css '" + name + "'");
      value = value.substr(6);  // Cut away data:,
      if (csss.count(name) > 0)
        throw Spine::Exception(BCP, "defs.csss css '" + name + "' defined multiple times");
      csss[name] = value;
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
