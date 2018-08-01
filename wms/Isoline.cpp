#include "Isoline.h"
#include "Config.h"
#include "Hash.h"

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

void Isoline::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Isoline JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "qid")
        qid = json.asString();
      else if (name == "value")
        value = json.asDouble();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else
        throw Spine::Exception(BCP, "Isoline does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get unique QID for the isoline
 */
// ----------------------------------------------------------------------

std::string Isoline::getQid(const State& theState) const
{
  if (!qid.empty())
    return qid;

  generated_qid = theState.makeQid("isoline");
  return generated_qid;
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Isoline::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(qid);
    boost::hash_combine(hash, Dali::hash_value(value));
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
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
