#include "WindRose.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
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

void WindRose::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "WindRose JSON is not a JSON hash");

    JsonTools::remove_int(radius, theJson, "radius");
    JsonTools::remove_int(minpercentage, theJson, "minpercentage");
    JsonTools::remove_int(sectors, theJson, "sectors");
    JsonTools::remove_string(symbol, theJson, "symbol");
    JsonTools::remove_string(parameter, theJson, "parameter");

    auto json = JsonTools::remove(theJson, "attributes");
    attributes.init(json, theConfig);

    json = JsonTools::remove(theJson, "connector");
    if (!json.isNull())
      (connector = Connector())->init(json, theConfig);

    json = JsonTools::remove(theJson, "limits");
    if (!json.isNull())
      JsonTools::extract_array("limits", limits, json, theConfig);

    if (!theJson.empty())
    {
      auto names = theJson.getMemberNames();
      auto namelist = boost::algorithm::join(names, ",");
      throw Fmi::Exception(BCP, "Unknown windrose settings: " + namelist);
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
