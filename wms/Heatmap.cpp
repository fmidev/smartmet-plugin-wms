#include "Heatmap.h"
#include "Config.h"
#include "Hash.h"
#include <boost/foreach.hpp>
#include <spine/Exception.h>
#include <stdexcept>

namespace
{
  static float linearKernel(float d)
  {
    return d;
  }

  static float sqrtKernel(float d)
  {
    return sqrtf(d);
  }

  static float expKernel(float d, double deviation)
  {
    return (float) exp(-0.5*sqrtf(d/deviation));
  }
}

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
      throw Spine::Exception(BCP, "Heatmap JSON is not map");

    // Iterate through all the members.

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
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
        throw Spine::Exception(BCP, "Heatmap does not have a setting named '" + name + "'");
    }

    if (!resolution)
      throw Spine::Exception(BCP, "Heatmap resolution is not set");
    else if (!radius)
      throw Spine::Exception(BCP, "Heatmap radius is not set");

    if (!kernel)
      kernel = "exp";
    if (!deviation)
      deviation = 10.0;

    max_points = std::min(theConfig.maxHeatmapPoints(), max_max_points);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get stamp using the selected kernel and radius
 */
// ----------------------------------------------------------------------

std::unique_ptr<heatmap_stamp_t, void(*)(heatmap_stamp_t*)> Heatmap::getStamp(unsigned r)
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
      unsigned d = 2*r+1;

      float* stamp = (float*) calloc(d*d, sizeof(float));

      if (!stamp)
        throw Spine::Exception(BCP, "Could not allocate memory for heatmap stamp");
      std::unique_ptr<float> up_stamp(stamp);

      for(y = 0 ; y < d ; ++y) {
        float* line = stamp + y*d;
        unsigned x;

        for(x = 0 ; x < d ; ++x, ++line) {
            const float dist = sqrtf((float)((x-r)*(x-r) + (y-r)*(y-r)))/(float)(r+1);
            const float ds = expKernel(dist, *deviation);
            /* This doesn't generate optimal assembly, but meh, it's readable. */
            const float clamped_ds = ds > 1.0f ? 1.0f
                                   : ds < 0.0f ? 0.0f
                                   :             ds;
            *line = 1.0f - clamped_ds;
        }
      }

      hms = heatmap_stamp_load(d, d, stamp);
    }
    else
      throw Spine::Exception(BCP, "Heatmap does not support kernel '" + *kernel + "'");

    if (!hms)
      throw Spine::Exception(BCP, "Heatmap stamp generation failed");

    return std::unique_ptr<heatmap_stamp_t, void(*)(heatmap_stamp_t*)>(hms, heatmap_stamp_free);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Heatmap::hash_value(const State& theState) const
{
  try
  {
    std::size_t hash = 0;

    boost::hash_combine(hash, Dali::hash_value(resolution));
    boost::hash_combine(hash, Dali::hash_value(radius));
    boost::hash_combine(hash, Dali::hash_value(kernel));
    if (kernel && (*kernel == "exp"))
      boost::hash_combine(hash, Dali::hash_value(deviation));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet