#include "Isoband.h"
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

void Isoband::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isoband JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
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
      else if (name == "label")
      {
        label = Text();
        label->init(json, theConfig);
      }
      else
        throw Fmi::Exception(BCP, "Isoband does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get unique QID for the isoband
 */
// ----------------------------------------------------------------------

std::string Isoband::getQid(const State& theState) const
{
  if (!qid.empty())
    return qid;

  generated_qid = theState.makeQid("isoband");
  return generated_qid;
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
    auto hash = Fmi::hash_value(qid);
    Fmi::hash_combine(hash, Fmi::hash_value(lolimit));
    Fmi::hash_combine(hash, Fmi::hash_value(hilimit));
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
