#include "Intersections.h"
#include "Hash.h"
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include <ctpp2/CDT.hpp>
#include <spine/Exception.h>

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
      throw Spine::Exception(BCP, "Intersections must be a JSON array or a JSON map");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize isobands
 */
// ----------------------------------------------------------------------

void Intersections::init(Engine::Querydata::Q q,
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the given values satisfy the set limits
 *
 * Note: We ignore the producer setting on purpose. Checking values
 * against another producer is not supported.
 */
// ----------------------------------------------------------------------

bool Intersections::inside(const IntersectValues& theValues) const
{
  for (const auto& isection : intersections)
  {
    if (isection.parameter)
    {
      const auto& iter = theValues.find(*isection.parameter);
      if (iter != theValues.end())
      {
        // There is a value for the parameter, check it against the limits
        double value = iter->second;

        if (isection.multiplier)
          value *= *isection.multiplier;
        if (isection.offset)
          value += *isection.offset;

        if (isection.value && *isection.value != value)
          return false;
        if (isection.lolimit && *isection.lolimit > value)
          return false;
        if (isection.hilimit && *isection.hilimit < value)
          return false;
      }
    }
  }
  return true;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get all parameter names used in the intersections
 */
// ----------------------------------------------------------------------

std::vector<std::string> Intersections::parameters() const
{
  std::set<std::string> params;

  for (const auto& intersection : intersections)
  {
    if (intersection.parameter)
      params.insert(*intersection.parameter);
  }

  std::vector<std::string> result(params.begin(), params.end());
  return result;
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
