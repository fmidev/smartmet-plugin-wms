#include "WindRose.h"
#include "Config.h"
#include "Hash.h"

#include <macgyver/Exception.h>
#include <spine/Json.h>
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

void WindRose::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "WindRose JSON is not a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "radius")
        radius = json.asInt();
      else if (name == "minpercentage")
        minpercentage = json.asInt();
      else if (name == "sectors")
        sectors = json.asInt();
      else if (name == "symbol")
        symbol = json.asString();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else if (name == "connector")
        (connector = Connector())->init(json, theConfig);
      else if (name == "parameter")
        parameter = json.asString();
      else if (name == "limits")
        Spine::JSON::extract_array("limits", limits, json, theConfig);
      else
        throw Fmi::Exception(BCP, "WindRose does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value<
 */
// ----------------------------------------------------------------------

std::size_t WindRose::hash_value(const State& theState) const
{
  try
  {
    auto hash = Fmi::hash_value(minpercentage);
    Fmi::hash_combine(hash, Fmi::hash_value(radius));
    Fmi::hash_combine(hash, Fmi::hash_value(sectors));
    Fmi::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(connector, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(parameter));
    Fmi::hash_combine(hash, Dali::hash_value(limits, theState));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
