#include "Map.h"
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
 */
// ----------------------------------------------------------------------

void Map::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Map JSON is not map");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

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
        throw Spine::Exception(BCP, "Map does not have a setting named '" + name + "'");
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

std::size_t Map::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(lines);

    // TODO: Should we add hash_value to Gis::MapOptions??
    boost::hash_combine(hash, Dali::hash_value(options.pgname));
    boost::hash_combine(hash, Dali::hash_value(options.schema));
    boost::hash_combine(hash, Dali::hash_value(options.table));
    boost::hash_combine(hash, Dali::hash_value(options.where));
    boost::hash_combine(hash, Dali::hash_value(options.minarea));
    boost::hash_combine(hash, Dali::hash_value(options.mindistance));
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
