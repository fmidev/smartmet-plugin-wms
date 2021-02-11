#include "Heatmap.h"
#include "Config.h"
#include "Hash.h"

#include <macgyver/Exception.h>
#include <stdexcept>

namespace
{
float linearKernel(float d)
{
  return d;
}

float sqrtKernel(float d)
{
  return sqrtf(d);
}

float expKernel(float d, double deviation)
{
  return static_cast<float>(exp(-0.5 * sqrtf(d / deviation)));
}
}  // namespace

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

void Heatmap::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Heatmap JSON is not map");

    // Iterate through all the members.

    const auto members = theJson.getMemberNames();
    for (const auto& name : members)
    {
      const Json::Value& json = theJson[name];

      if (name == "resolution")
        resolution = json.asDouble();
      else if (name == "radius")
        radius = json.asDouble();
      else if (name == "kernel")
        kernel = json.asString();
      else if (name == "deviation")
        deviation = json.asDouble();
      else
        throw Fmi::Exception(BCP, "Heatmap does not have a setting named '" + name + "'");
    }

    if (!resolution)
      throw Fmi::Exception(BCP, "Heatmap resolution is not set");
    if (!radius)
      throw Fmi::Exception(BCP, "Heatmap radius is not set");

    if (!kernel)
      kernel = "exp";
    if (!deviation)
      deviation = 10.0;

    max_points = std::min(theConfig.maxHeatmapPoints(), max_max_points);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get stamp using the selected kernel and radius
 */
// ----------------------------------------------------------------------

std::unique_ptr<heatmap_stamp_t, void (*)(heatmap_stamp_t*)> Heatmap::getStamp(unsigned r)
{
  try
  {
    heatmap_stamp_t* hms;

    if (*kernel == "linear")
      hms = heatmap_stamp_gen_nonlinear(r, &linearKernel);
    else if (*kernel == "sqrt")
      hms = heatmap_stamp_gen_nonlinear(r, &sqrtKernel);
    else if (*kernel == "exp")
    {
      // Code below to is copied from heatmap_stamp_gen_nonlinear() (heatmap.c).
      //
      // The kernel call is different, passing second argument (deviation) too.
      // heatmap_stamp_load makes a copy of the stamp buffer; it must be deleted at return.

      unsigned y;
      unsigned d = 2 * r + 1;

      auto* stamp = static_cast<float*>(std::calloc(d * d, sizeof(float)));

      if (stamp == nullptr)
        throw Fmi::Exception(BCP, "Could not allocate memory for heatmap stamp");

      // Safely deallocate the data in case of exceptions
      std::unique_ptr<float, decltype(free)*> delete_stamp{stamp, free};

      for (y = 0; y < d; ++y)
      {
        float* line = stamp + y * d;
        unsigned x;

        for (x = 0; x < d; ++x, ++line)
        {
          const auto dist =
              static_cast<float>(sqrt(((x - r) * (x - r) + (y - r) * (y - r))) / (r + 1));
          const float ds = expKernel(dist, *deviation);
          /* This doesn't generate optimal assembly, but meh, it's readable. */
          const float clamped_ds = ds > 1.0f ? 1.0f : ds < 0.0f ? 0.0f : ds;
          *line = 1.0f - clamped_ds;
        }
      }

      hms = heatmap_stamp_load(d, d, stamp);
    }
    else
      throw Fmi::Exception(BCP, "Heatmap does not support kernel '" + *kernel + "'");

    if (hms == nullptr)
      throw Fmi::Exception(BCP, "Heatmap stamp generation failed");

    return std::unique_ptr<heatmap_stamp_t, void (*)(heatmap_stamp_t*)>(hms, heatmap_stamp_free);
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

std::size_t Heatmap::hash_value(const State& /* theState */) const
{
  try
  {
    std::size_t hash = 0;

    Dali::hash_combine(hash, Dali::hash_value(resolution));
    Dali::hash_combine(hash, Dali::hash_value(radius));
    Dali::hash_combine(hash, Dali::hash_value(kernel));
    if (kernel && (*kernel == "exp"))
      Dali::hash_combine(hash, Dali::hash_value(deviation));
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
