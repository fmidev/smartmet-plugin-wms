#include "Intersections.h"
#include "Hash.h"
#include <spine/Exception.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize the intersections from JSON
 */
// ----------------------------------------------------------------------

void Intersections::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (theJson.isArray())
    {
      for (unsigned int i = 0; i < theJson.size(); i++)
      {
        const Json::Value& json = theJson[i];
        Intersection intersection;
        intersection.init(json, theConfig);
        intersections.push_back(intersection);
      }
    }
    else if (theJson.isObject())
    {
      Intersection intersection;
      intersection.init(theJson, theConfig);
      intersections.push_back(intersection);
    }
    else
      throw SmartMet::Spine::Exception(BCP, "Intersections must be a JSON array or a JSON map");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize isobands
 */
// ----------------------------------------------------------------------

void Intersections::init(SmartMet::Engine::Querydata::Q q,
                         const Projection& theProjection,
                         const boost::posix_time::ptime& theTime,
                         const State& theState)
{
  try
  {
    for (auto& intersection : intersections)
    {
      intersection.init(q, theProjection, theTime, theState);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Intersect with isobands
 */
// ----------------------------------------------------------------------

OGRGeometryPtr Intersections::intersect(OGRGeometryPtr geom) const
{
  try
  {
    for (const auto& intersection : intersections)
    {
      geom = intersection.intersect(geom);
    }
    return geom;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test insidedness
 *
 * Note that testing insidedness is a linear operation in the number
 * of polygon vertices, but calculating the intersection of all the
 * polygons may be much more expensive, even though the resulting
 * number of may be much lower.
 */
// ----------------------------------------------------------------------

bool Intersections::inside(double theX, double theY) const
{
  try
  {
    for (const auto& intersection : intersections)
    {
      if (!intersection.inside(theX, theY))
        return false;
    }
    return true;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value = combined hash from all layers
 */
// ----------------------------------------------------------------------

std::size_t Intersections::hash_value(const State& theState) const
{
  try
  {
    return Dali::hash_value(intersections, theState);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
