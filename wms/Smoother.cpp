#include "Smoother.h"
#include "Hash.h"

#include <boost/numeric/conversion/cast.hpp>
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

void Smoother::init(Json::Value& theJson, const Config& /* theConfig */)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Smoother JSON is not a JSON object");

    // Iterate through all the members. "size"/"degree" drive the legacy
    // Savitzky-Golay filter; "method" (with radius/passes/boundary/morphology)
    // selects the Trax grid smoother. The two paths are independent.

    Trax::SmoothOptions trax;
    bool has_method = false;

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "size")
        size = boost::numeric_cast<std::size_t>(json.asInt());
      else if (name == "degree")
        degree = boost::numeric_cast<std::size_t>(json.asInt());
      else if (name == "method")
      {
        trax.method = Trax::to_smooth_method(json.asString());
        has_method = true;
      }
      else if (name == "radius")
        trax.radius = boost::numeric_cast<int>(json.asInt());
      else if (name == "passes")
        trax.passes = boost::numeric_cast<int>(json.asInt());
      else if (name == "boundary")
        trax.boundary = Trax::to_smooth_boundary(json.asString());
      else if (name == "morphology")
        trax.morphology = Trax::to_morphology_op(json.asString());
      else if (name == "preserve_missing")
        trax.preserve_missing = json.asBool();
      else
        throw Fmi::Exception(BCP, "Smoother does not have a setting named '" + name + "'");
    }

    if (has_method)
      trax_options = trax;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t Smoother::hash_value(const State& /* theState */) const
{
  try
  {
    auto hash = Fmi::hash_value(size);
    Fmi::hash_combine(hash, Fmi::hash_value(degree));
    if (trax_options)
      Fmi::hash_combine(hash, trax_options->hash());
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
