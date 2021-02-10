//======================================================================

#include "Intersection.h"
#include "Config.h"
#include "Hash.h"
#include "Projection.h"
#include "State.h"

#include <engines/contour/Engine.h>
#include <engines/contour/Interpolation.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>

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

void Intersection::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Intersection JSON is not a JSON map");

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("lolimit", nulljson);
    if (!json.isNull())
      lolimit = json.asDouble();

    json = theJson.get("hilimit", nulljson);
    if (!json.isNull())
      hilimit = json.asDouble();

    json = theJson.get("value", nulljson);
    if (!json.isNull())
      value = json.asDouble();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("producer", nulljson);
    if (!json.isNull())
      producer = json.asString();

    json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("interpolation", nulljson);
    if (!json.isNull())
      interpolation = json.asString();

    json = theJson.get("smoother", nulljson);
    if (!json.isNull())
      smoother.init(json, theConfig);

    json = theJson.get("unit_conversion", nulljson);
    if (!json.isNull())
      unit_conversion = json.asString();

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Intersect the given polygon
 */
// ----------------------------------------------------------------------

OGRGeometryPtr Intersection::intersect(OGRGeometryPtr theGeometry) const
{
  try
  {
    // If no parameter is set, there is nothing to intersect
    if (!parameter)
      return theGeometry;

    // Otherwise if the input is null, return null
    if (!theGeometry || theGeometry->IsEmpty() != 0)
      return {};

    // Do the intersection

    if (!isoband || isoband->IsEmpty() != 0)
      return {};

    return OGRGeometryPtr(theGeometry->Intersection(isoband.get()));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test whether the given point is inside the polygon
 *
 * The coordinate CRS is assumed to be the same as in the isoband
 */
// ----------------------------------------------------------------------

bool Intersection::inside(double theX, double theY) const
{
  try
  {
    // If no parameter is set, everything is in
    if (!parameter)
      return true;

    if (!isoband || isoband->IsEmpty() != 0)
      return false;

    return Fmi::OGR::inside(*isoband, theX, theY);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the intersecting polygon
 */
// ----------------------------------------------------------------------

void Intersection::init(const boost::optional<std::string>& theProducer,
                        const Projection& theProjection,
                        const boost::posix_time::ptime& theTime,
                        const State& theState)
{
  try
  {
    // Quick exit if done already
    if (isoband)
      return;

    // If no parameter is set, no intersections will be calculated
    if (!parameter)
      return;

    // No polygons for observations

    if (theState.isObservation(theProducer))
      return;

    // Establish the data

    if (!producer && !theProducer)
      throw Fmi::Exception(BCP, "Producer not set for intersection");

    auto q = theState.get(producer ? *producer : *theProducer);

    // This can happen when we use observations
    if (!q)
      return;

    // Now that we know the parameter is not for observations, parse it

    auto param = Spine::ParameterFactory::instance().parse(*parameter);

    // Establish the level

    if (level)
    {
      bool match = false;
      for (q->resetLevel(); !match && q->nextLevel();)
        match = (q->levelValue() == *level);
      if (!match)
        throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available");
    }

    // Establish the projection from the geometry to be clipped

    auto crs = theProjection.getCRS();

    // Calculate the isoband

    const auto& contourer = theState.getContourEngine();

    std::vector<Engine::Contour::Range> limits;
    Engine::Contour::Range range(lolimit, hilimit);
    limits.push_back(range);
    Engine::Contour::Options options(param, theTime, limits);

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    if (multiplier || offset)
      options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

    options.filter_size = smoother.size;
    options.filter_degree = smoother.degree;

    if (interpolation == "linear")
      options.interpolation = Engine::Contour::Linear;
    else if (interpolation == "nearest")
      options.interpolation = Engine::Contour::Nearest;
    else if (interpolation == "discrete")
      options.interpolation = Engine::Contour::Discrete;
    else if (interpolation == "loglinear")
      options.interpolation = Engine::Contour::LogLinear;
    else
      throw Fmi::Exception(BCP, "Unknown isoband interpolation method '" + interpolation + "'");

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    Dali::hash_combine(valueshash, options.data_hash_value());

    // Select the data

    if (!q->param(options.parameter.number()))
      throw Fmi::Exception(BCP, "Parameter '" + options.parameter.name() + "' unavailable.");

    if (!q->firstLevel())
      throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

    // Select the level.
    if (options.level)
    {
      if (!q->selectLevel(*options.level))
      {
        throw Fmi::Exception(BCP,
                             "Level value " + boost::lexical_cast<std::string>(*options.level) +
                                 " is not available.");
      }
    }

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, valueshash, options.time);
    CoordinatesPtr coords = qEngine.getWorldCoordinates(q, crs);

    std::vector<OGRGeometryPtr> isobands =
        contourer.contour(qhash, q->SpatialReference(), crs, *matrix, *coords, options);

    if (isobands.empty())
      return;

    isoband = *(isobands.begin());

    if (!isoband || isoband->IsEmpty() != 0)
      return;

    // This does not obey layer margin settings, hence this is disabled:

    // Clip the contour with the bounding box for extra speed later on.
    // const auto& box = theProjection.getBox();
    // isoband.reset(Fmi::OGR::polyclip(*isoband, box));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the object
 */
// ----------------------------------------------------------------------

std::size_t Intersection::hash_value(const State& theState) const
{
  try
  {
    std::size_t hash = 0;

    if (producer)
      Dali::hash_combine(hash, Engine::Querydata::hash_value(theState.get(*producer)));

    Dali::hash_combine(hash, Dali::hash_value(lolimit));
    Dali::hash_combine(hash, Dali::hash_value(hilimit));
    Dali::hash_combine(hash, Dali::hash_value(value));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(producer));
    Dali::hash_combine(hash, Dali::hash_value(parameter));
    Dali::hash_combine(hash, Dali::hash_value(interpolation));
    Dali::hash_combine(hash, Dali::hash_value(unit_conversion));
    Dali::hash_combine(hash, Dali::hash_value(multiplier));
    Dali::hash_combine(hash, Dali::hash_value(offset));
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
