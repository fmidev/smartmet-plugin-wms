#include "Isoline.h"
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

void Isoline::init(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isoline JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      Json::Value& json = theJson[name];

      if (name == "qid")
        qid = json.asString();
      else if (name == "value")
        value = json.asDouble();
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else if (name == "label")
      {
        label = Text("Isoline");
        label->init(json, theConfig);
      }
      else
        throw Fmi::Exception(BCP, "Isoline does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    auto hash = Fmi::hash_value(qid);
    Fmi::hash_combine(hash, Fmi::hash_value(value));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(label, theState));
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
