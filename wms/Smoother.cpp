#include "Smoother.h"
#include "Hash.h"

#include <boost/numeric/conversion/cast.hpp>
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

void Smoother::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Smoother JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "size")
        size = boost::numeric_cast<std::size_t>(json.asInt());
      else if (name == "degree")
        degree = boost::numeric_cast<std::size_t>(json.asInt());
      else
        throw Spine::Exception(BCP, "Smoother does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t Smoother::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(size);
    boost::hash_combine(hash, Dali::hash_value(degree));
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
