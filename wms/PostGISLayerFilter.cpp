#include "PostGISLayerFilter.h"
#include "Config.h"
#include "Hash.h"

#include <spine/Exception.h>
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

void PostGISLayerFilter::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "JSON is not a JSON object!");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
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
    throw Spine::Exception::Trace(BCP, "Layer init failed!");
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
    Dali::hash_combine(hash, Dali::hash_value(attributes, theState));
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
