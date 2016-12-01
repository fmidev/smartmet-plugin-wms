#include "Station.h"
#include "Config.h"
#include "Hash.h"
#include <boost/foreach.hpp>
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
 * mean the station fmisid. The coordinates will be initialized
 * respectively.
 *
 */
// ----------------------------------------------------------------------

void Station::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    // Initialize from integer

    if (theJson.isInt())
    {
      fmisid = theJson.asInt();
      return;
    }

    // Initialize with more details

    if (!theJson.isObject())
      throw SmartMet::Spine::Exception(BCP,
                                       "Station JSON must be an integer (fmisid) or a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (name == "fmisid")
        fmisid = json.asInt();
#if 0
        else if(name == "lpnn")
        lpnn = json.asInt();
        else if(name == "wmo")
        wmo = json.asInt();
#endif
      else if (name == "longitude")
        longitude = json.asDouble();
      else if (name == "latitude")
        latitude = json.asDouble();
      else if (name == "symbol")
        symbol = json.asString();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else if (name == "title")
      {
        title = Title();
        title->init(json, theConfig);
      }
      else
        throw SmartMet::Spine::Exception(BCP,
                                         "Station does not have a setting named '" + name + "'");
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

std::size_t Station::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(fmisid);
    boost::hash_combine(hash, Dali::hash_value(longitude));
    boost::hash_combine(hash, Dali::hash_value(latitude));
    boost::hash_combine(hash, Dali::hash_value(symbol));
    boost::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
    boost::hash_combine(hash, Dali::hash_value(title, theState));
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
