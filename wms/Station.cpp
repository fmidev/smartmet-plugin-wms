#include "Station.h"
#include "Config.h"
#include "Hash.h"

#include <macgyver/Exception.h>
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

void Station::init(Json::Value& theJson, const Config& theConfig)
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
      throw Fmi::Exception(BCP, "Station JSON must be an integer (fmisid) or a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "fmisid")
        fmisid = json.asInt();
      else if (name == "lpnn")
        lpnn = json.asInt();
      else if (name == "wmo")
        wmo = json.asInt();
      else if (name == "geoid")
        geoid = json.asInt();
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
      else if (name == "dx")
        dx = json.asInt();
      else if (name == "dy")
        dy = json.asInt();
      else
        throw Fmi::Exception(BCP, "Station does not have a setting named '" + name + "'");
    }

    // Make sure the selection is unique. For historical reasons
    // we allow the coordinates to be specified even if an unique
    // ID is given, otherwise the WindRose test would break.

    int count = 0;
    if (fmisid)
      ++count;
    if (lpnn)
      ++count;
    if (wmo)
      ++count;
    if (geoid)
      ++count;

    if (count == 0)
      throw Fmi::Exception(
          BCP, "Station does not specify any of fmisid, lpnn, wmo, geoid or latlon coordinates");
    if (count > 1)
      throw Fmi::Exception(
          BCP,
          "Station must be defined by only one of fmisid, lpnn, wmo, geoid or latlon coordinates");

    if (longitude && !latitude)
      throw Fmi::Exception(BCP, "Station latitude missing");
    if (!longitude && latitude)
      throw Fmi::Exception(BCP, "Station longitude missing");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    auto hash = Fmi::hash_value(fmisid);
    Fmi::hash_combine(hash, Fmi::hash_value(lpnn));
    Fmi::hash_combine(hash, Fmi::hash_value(wmo));
    Fmi::hash_combine(hash, Fmi::hash_value(geoid));
    Fmi::hash_combine(hash, Fmi::hash_value(longitude));
    Fmi::hash_combine(hash, Fmi::hash_value(latitude));
    Fmi::hash_combine(hash, Fmi::hash_value(symbol));
    Fmi::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(title, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
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
