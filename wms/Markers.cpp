#include "Markers.h"
#include "Config.h"
#include "Hash.h"

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

void Markers::init(const Json::Value& theJson, const State& theState)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto markers_members = theJson.getMemberNames();
    for (const auto& name : markers_members)
    {
      const Json::Value& marker_json = theJson[name];

      if (!marker_json.isString())
        throw Spine::Exception(BCP,
                               "Invalid object type in defs.markers initialization, expecting "
                               "name-value pair for marker '" +
                                   name + "'");

      std::string value = Spine::HTTP::urldecode(marker_json.asString());
      if (value.substr(0, 6) != "data:,")
        throw Spine::Exception(BCP,
                               "Only RFC2397 data-URLs supported: URL incorrect '" + value +
                                   "' for marker '" + name + "'");
      value = value.substr(6);  // Cut away data:,
      if (!theState.setMarker(name, value))
        throw Spine::Exception(BCP, "defs.markers marker '" + name + "' defined multiple times");
      markers[name] = value;
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
