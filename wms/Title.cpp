#include "Title.h"
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

void Title::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Title JSON must be a JSON hash");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "qid")
        qid = json.asString();
      else if (name == "dx")
        dx = json.asInt();
      else if (name == "dy")
        dy = json.asInt();
      else if (name == "text")
        text.init(json, theConfig);
      else if (name == "attributes")
        attributes.init(json, theConfig);
      else
        throw Fmi::Exception(BCP, "Title object has no field named '" + name + "'");
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

std::size_t Title::hash_value(const State& theState) const
{
  try
  {
    auto hash = Fmi::hash_value(qid);
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(text, theState));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Translate the title to the given language
 */
// ----------------------------------------------------------------------

const std::string& Title::translate(const std::string& theLanguage) const
{
  try
  {
    return text.translate(theLanguage);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Translate the title to the given language
 */
// ----------------------------------------------------------------------

const std::string& Title::translate(const boost::optional<std::string>& theLanguage) const
{
  try
  {
    return text.translate(theLanguage);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
