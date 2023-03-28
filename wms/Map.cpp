#include "Map.h"
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
 */
// ----------------------------------------------------------------------

void Map::init(Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Map JSON is not map");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "lines")
        lines = json.asBool();
      else if (name == "pgname")
        options.pgname = json.asString();
      else if (name == "schema")
        options.schema = json.asString();
      else if (name == "table")
        options.table = json.asString();
      else if (name == "where")
        options.where = json.asString();
      else if (name == "mindistance")
        options.mindistance = json.asDouble();
      else if (name == "minarea")
        options.minarea = json.asDouble();
      else
        throw Fmi::Exception(BCP, "Map does not have a setting named '" + name + "'");
    }
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

std::size_t Map::hash_value(const State& /* theState */) const
{
  try
  {
    auto hash = Fmi::hash_value(lines);

    // TODO(mheiskan): Should we add hash_value to Gis::MapOptions??
    Fmi::hash_combine(hash, Fmi::hash_value(options.pgname));
    Fmi::hash_combine(hash, Fmi::hash_value(options.schema));
    Fmi::hash_combine(hash, Fmi::hash_value(options.table));
    Fmi::hash_combine(hash, Fmi::hash_value(options.where));
    Fmi::hash_combine(hash, Fmi::hash_value(options.minarea));
    Fmi::hash_combine(hash, Fmi::hash_value(options.mindistance));
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
