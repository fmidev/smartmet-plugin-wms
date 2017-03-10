#include "PostGISLayerFilter.h"
#include "Config.h"
#include "Hash.h"
#include <spine/Exception.h>
#include <spine/Json.h>
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

void PostGISLayerFilter::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "JSON is not a JSON object!");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (name == "where")
        where = json.asString();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else if (name == "text_attributes")
        text_attributes.init(json, theConfig);
      else
        throw Spine::Exception(BCP, "Unknown setting '" + name + "'!");
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Layer init failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t PostGISLayerFilter::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(where);
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
