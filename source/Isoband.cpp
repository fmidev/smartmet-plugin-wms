#include "Isoband.h"
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

void Isoband::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw SmartMet::Spine::Exception(BCP, "Isoband JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (name == "qid")
        qid = json.asString();
      else if (name == "lolimit")
        lolimit = json.asDouble();
      else if (name == "hilimit")
        hilimit = json.asDouble();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else
        throw SmartMet::Spine::Exception(BCP,
                                         "Isoband does not have a setting named '" + name + "'");
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

std::size_t Isoband::hash_value(const State& theState) const
{
  try
  {
    auto hash = boost::hash_value(qid);
    boost::hash_combine(hash, Dali::hash_value(lolimit));
    boost::hash_combine(hash, Dali::hash_value(hilimit));
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
