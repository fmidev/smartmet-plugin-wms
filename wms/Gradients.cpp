#include "Gradients.h"
#include "Config.h"
#include "Hash.h"
#include <boost/foreach.hpp>
#include <spine/Exception.h>
#include <spine/HTTP.h>
#include <string>

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

void Gradients::init(const Json::Value& theJson, const State& theState)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Styles JSON must be a JSON object (name-value pairs)");

    // Iterate through all the members

    const auto gradients_members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, gradients_members)
    {
      const Json::Value& gradient_json = theJson[name];

      if (!gradient_json.isString())
        throw Spine::Exception(BCP,
                               "Invalid object type in defs.gradients initialization, expecting "
                               "name-value pair for gradient '" +
                                   name + "'");

      std::string value = Spine::HTTP::urldecode(gradient_json.asString());
      if (value.substr(0, 6) != "data:,")
        throw Spine::Exception(BCP,
                               "Only RFC2397 data-URLs supported: URL incorrect '" + value +
                                   "' for gradient '" + name + "'");
      value = value.substr(6);  // Cut away data:,
      if (theState.setGradient(name, value) == false)
        throw Spine::Exception(BCP,
                               "defs.gradients gradient '" + name + "' defined multiple times");
      gradients[name] = value;
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
