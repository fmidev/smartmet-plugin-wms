#include "Smoother.h"
#include "Hash.h"
#include <boost/foreach.hpp>
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
      throw SmartMet::Spine::Exception(BCP, "Smoother JSON is not a JSON object");

    // Iterate through all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      if (name == "size")
        size = boost::numeric_cast<std::size_t>(json.asInt());
      else if (name == "degree")
        degree = boost::numeric_cast<std::size_t>(json.asInt());
      else
        throw SmartMet::Spine::Exception(BCP,
                                         "Smoother does not have a setting named '" + name + "'");
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
