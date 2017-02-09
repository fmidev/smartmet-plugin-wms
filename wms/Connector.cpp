#include "Connector.h"
#include "Config.h"
#include "Hash.h"
#include <spine/Exception.h>
#include <boost/foreach.hpp>
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
      throw SmartMet::Spine::Exception(BCP, "Connector JSON is not a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (name == "startoffset")
        startoffset = json.asInt();
      else if (name == "endoffset")
        endoffset = json.asInt();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else
        throw SmartMet::Spine::Exception(BCP,
                                         "Connector does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
