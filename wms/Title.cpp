#include "Title.h"
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
 * \brief Set reasonable defaults
 */
// ----------------------------------------------------------------------

Title::Title() : qid(), dx(0), dy(0), attributes(), text() {}
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
      throw Spine::Exception(BCP, "Title JSON must be a JSON hash");

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
        throw Spine::Exception(BCP, "Title object has no field named '" + name + "'");
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

std::size_t Title::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(qid);
    boost::hash_combine(hash, Dali::hash_value(dx));
    boost::hash_combine(hash, Dali::hash_value(dy));
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
    boost::hash_combine(hash, Dali::hash_value(text, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
