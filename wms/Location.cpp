#include "Location.h"
#include "Config.h"
#include "Hash.h"
#include "State.h"

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
 *
 * We allow initializing from an integer, which we interpret to
 * mean the location fmisid. The coordinates will be initialized
 * respectively.
 *
 */
// ----------------------------------------------------------------------

void Location::init(const Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    // One could initialize from a string to establish a place like Station does for ints

    // Initialize with more details

    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Location JSON must be a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "longitude")
        longitude = json.asDouble();
      else if (name == "latitude")
        latitude = json.asDouble();
      else if (name == "dx")
        dx = json.asInt();
      else if (name == "dy")
        dy = json.asInt();
      else
        throw Spine::Exception(BCP, "Location does not have a setting named '" + name + "'");
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

std::size_t Location::hash_value(const State& /* theState */) const
{
  try
  {
    auto hash = Dali::hash_value(longitude);
    boost::hash_combine(hash, Dali::hash_value(latitude));
    boost::hash_combine(hash, Dali::hash_value(dx));
    boost::hash_combine(hash, Dali::hash_value(dy));
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
