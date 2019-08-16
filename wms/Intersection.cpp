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
#include <grid-content/queryServer/definition/QueryConfigurator.h>

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
      throw Spine::Exception(BCP, "Intersection JSON is not a JSON map");

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize the intersecting polygon
 */
// ----------------------------------------------------------------------

void Intersection::init(const boost::optional<std::string>& theProducer,
                        Engine::Grid::Engine *gridEngine,
                        const Projection& theProjection,
                        const boost::posix_time::ptime& theTime,
                        const State& theState)
{
  try
  {
    QueryServer::Query query;
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string wkt = *theProjection.crs;

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      auto crs = theProjection.getCRS();
      char *out = nullptr;
      crs->exportToWkt(&out);
      wkt = out;
      OGRFree(out);

      //std::cout << wkt << "\n";

      auto bl = theProjection.bottomLeftLatLon();
      auto tr = theProjection.topRightLatLon();

      char bbox[100];
      sprintf(bbox,"%f,%f,%f,%f",bl.X(),bl.Y(),tr.X(),tr.Y());

      // Adding the bounding box information into the query.
      query.mAttributeList.addAttribute("grid.llbox",bbox);
    }

    // Adding parameter information into the query.

    std::string param = gridEngine->getParameterString(*theProducer,*parameter);
    attributeList.addAttribute("param",param);

    if (param == *parameter  &&  query.mProducerNameList.size() == 0)
    {
      gridEngine->getProducerNameList(*theProducer,query.mProducerNameList);
      if (query.mProducerNameList.size() == 0)
        query.mProducerNameList.push_back(*theProducer);
    }

    std::string forecastTime = Fmi::to_iso_string(theTime);
    attributeList.addAttribute("startTime",forecastTime);
    attributeList.addAttribute("endTime",forecastTime);
    attributeList.addAttribute("timelist",forecastTime);
    attributeList.addAttribute("timezone","UTC");

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(query,attributeList);

    // Fullfilling information into the query object.

    if (!hilimit)
      hilimit = 1000000000;

    if (!lolimit)
      lolimit = -1000000000;

    float hlimit = C_FLOAT(*hilimit);
    float llimit = C_FLOAT(*lolimit);

    for (auto it = query.mQueryParameterList.begin(); it != query.mQueryParameterList.end(); ++it)
    {
      it->mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      it->mType = QueryServer::QueryParameter::Type::Isoband;
      it->mContourLowValues.push_back(llimit);
      it->mContourHighValues.push_back(hlimit);
    }

    query.mSearchType = QueryServer::Query::SearchType::TimeSteps;
    query.mAttributeList.addAttribute("grid.crs",wkt);

    if (theProjection.xsize)
      query.mAttributeList.addAttribute("grid.width",std::to_string(*theProjection.xsize));

    if (theProjection.ysize)
      query.mAttributeList.addAttribute("grid.height",std::to_string(*theProjection.ysize));

    if (wkt == "data"  &&  theProjection.x1 && theProjection.y1 && theProjection.x2 && theProjection.y2)
    {
      char bbox[100];
      sprintf(bbox,"%f,%f,%f,%f",*theProjection.x1,*theProjection.y1,*theProjection.x2,*theProjection.y2);
      query.mAttributeList.addAttribute("grid.bbox",bbox);
    }

    if (smoother.size)
      query.mAttributeList.addAttribute("contour.smooth.size",std::to_string(*smoother.size));

    if (smoother.degree)
      query.mAttributeList.addAttribute("contour.smooth.degree",std::to_string(*smoother.degree));

    if (offset)
      query.mAttributeList.addAttribute("contour.offset",std::to_string(*offset));

    query.mAttributeList.setAttribute("contour.coordinateType",std::to_string(T::CoordinateTypeValue::ORIGINAL_COORDINATES));
    //query.mAttributeList.setAttribute("contour.coordinateType",std::to_string(T::CoordinateTypeValue::LATLON_COORDINATES));
    //query.mAttributeList.setAttribute("contour.coordinateType",std::to_string(T::CoordinateTypeValue::GRID_COORDINATES));

    // The Query object before the query execution.
    //query.print(std::cout,0,0);

    // Executing the query.
    gridEngine->executeQuery(query);

    // The Query object after the query execution.
    // query.print(std::cout,0,0);


    // Converting the returned WKB-isolines into OGRGeometry objects.

    std::vector<OGRGeometryPtr> isobands;
    for (auto param = query.mQueryParameterList.begin(); param != query.mQueryParameterList.end(); ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        if (val->mValueData.size() > 0)
        {
          uint c = 0;
          for (auto wkb = val->mValueData.begin(); wkb != val->mValueData.end(); ++wkb)
          {
            unsigned char *cwkb = reinterpret_cast<unsigned char *>(wkb->data());
            OGRGeometry *geom = nullptr;
            OGRGeometryFactory::createFromWkb(cwkb,nullptr,&geom,wkb->size());
            auto geomPtr = OGRGeometryPtr(geom);
            isobands.push_back(geomPtr);
            c++;
          }
        }
      }
    }
    isoband = *(isobands.begin());

    if (!isoband || isoband->IsEmpty() != 0)
      return;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}





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
      throw Spine::Exception(BCP, "Producer not set for intersection");

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
        throw Spine::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available");
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
      throw Spine::Exception(BCP, "Unknown isoband interpolation method '" + interpolation + "'");

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    Dali::hash_combine(valueshash, options.data_hash_value());
    std::string wkt = q->area().WKT();

    // Select the data

    if (!q->param(options.parameter.number()))
      throw Spine::Exception(BCP, "Parameter '" + options.parameter.name() + "' unavailable.");

    if (!q->firstLevel())
      throw Spine::Exception(BCP, "Unable to set first level in querydata.");

    // Select the level.
    if (options.level)
    {
      if (!q->selectLevel(*options.level))
      {
        throw Spine::Exception(BCP,
                               "Level value " + boost::lexical_cast<std::string>(*options.level) +
                                   " is not available.");
      }
    }

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, valueshash, options.time);
    CoordinatesPtr coords = qEngine.getWorldCoordinates(q, crs.get());
    std::vector<OGRGeometryPtr> isobands =
        contourer.contour(qhash, wkt, *matrix, coords, options, q->needsWraparound(), crs.get());
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
