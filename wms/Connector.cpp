#include "Connector.h"
#include "Config.h"
#include "Hash.h"

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

void Connector::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Connector JSON is not a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "startoffset")
        startoffset = json.asInt();
      else if (name == "endoffset")
        endoffset = json.asInt();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else
        throw Spine::Exception(BCP, "Connector does not have a setting named '" + name + "'");
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

std::size_t Connector::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(startoffset);
    boost::hash_combine(hash, Dali::hash_value(endoffset));
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
