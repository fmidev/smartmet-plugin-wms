//======================================================================

#include "IsobandLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Isoband.h"
#include "Layer.h"
#include "State.h"
#include <gis/Box.h>
#include <gis/OGR.h>
#include <ctpp2/CDT.hpp>
#include <spine/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <engines/contour/Engine.h>
#include <engines/contour/Interpolation.h>
#include <engines/gis/Engine.h>
#include <boost/foreach.hpp>

// TODO:
#include <boost/timer/timer.hpp>

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

void IsobandLayer::init(const Json::Value& theJson,
                        const State& theState,
                        const Config& theConfig,
                        const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw SmartMet::Spine::Exception(BCP, "JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("isobands", nulljson);
    if (!json.isNull())
      SmartMet::Spine::JSON::extract_array("isobands", isobands, json, theConfig);

    json = theJson.get("interpolation", nulljson);
    if (!json.isNull())
      interpolation = json.asString();

    json = theJson.get("smoother", nulljson);
    if (!json.isNull())
      smoother.init(json, theConfig);

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();

    json = theJson.get("outside", nulljson);
    if (!json.isNull())
    {
      outside.reset(Map());
      outside->init(json, theConfig);
    }

    json = theJson.get("inside", nulljson);
    if (!json.isNull())
    {
      inside.reset(Map());
      inside->init(json, theConfig);
    }

    json = theJson.get("sampling", nulljson);
    if (!json.isNull())
      sampling.init(json, theConfig);

    json = theJson.get("intersect", nulljson);
    if (!json.isNull())
      intersections.init(json, theConfig);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void IsobandLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // Establish the data
    auto q = getModel(theState);

    // Establish the parameter

    if (!parameter)
      throw SmartMet::Spine::Exception(BCP, "Parameter not set for isoband-layer!");
    auto param = SmartMet::Spine::ParameterFactory::instance().parse(*parameter);

    // Establish the valid time

    if (!time)
      throw SmartMet::Spine::Exception(BCP, "Time has not been set for isoband-layer!");

    auto valid_time = *time;
    if (time_offset)
      valid_time += boost::posix_time::minutes(*time_offset);

    // Establish the level

    if (level)
    {
      bool match = false;
      for (q->resetLevel(); !match && q->nextLevel();)
        match = (q->levelValue() == *level);
      if (!match)
        throw SmartMet::Spine::Exception(
            BCP, "Level value " + Fmi::to_string(*level) + " is not available!");
    }

    // Get projection details

    bool has_data_proj = (projection.crs && *projection.crs == "data");
    projection.update(q);
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

#if 0
        char * txt;
        crs->exportToWkt(&txt);
        std::cout << txt << std::endl;
        delete txt;
#endif

    // Sample to higher resolution if necessary

    auto sampleresolution = sampling.getResolution(projection);
    if (sampleresolution)
    {
      std::string report2 = "IsobandLayer::resample finished in %t sec CPU, %w sec real\n";
      std::unique_ptr<boost::timer::auto_cpu_timer> timer2;
      if (theState.useTimer())
        timer2.reset(new boost::timer::auto_cpu_timer(2, report2));

      auto demdata = theState.getGeoEngine().dem();
      auto landdata = theState.getGeoEngine().landCover();
      if (!demdata || !landdata)
        throw SmartMet::Spine::Exception(
            BCP, "Resampling data requires DEM and land cover data to be available!");

      q = q->sample(param,
                    valid_time,
                    *crs,
                    box.xmin(),
                    box.ymin(),
                    box.xmax(),
                    box.ymax(),
                    *sampleresolution,
                    *demdata,
                    *landdata);
    }

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Generate isobands as use tags statements inside <g>..</g>

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    if (!theState.inDefs())
    {
      group_cdt["start"] = "<g";
      group_cdt["end"] = "</g>";
      // Add attributes to the group, not the isobands
      theState.addAttributes(theGlobals, group_cdt, attributes);
    }

    // Logical operations with maps require shapes

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape, outshape;
    if (inside)
    {
      inshape = gis.getShape(crs.get(), inside->options);
      if (!inshape)
        throw SmartMet::Spine::Exception(BCP, "Received empty inside-shape from database!");

      inshape.reset(Fmi::OGR::polyclip(*inshape, box));
    }
    if (outside)
    {
      outshape = gis.getShape(crs.get(), outside->options);
      if (outshape)
        outshape.reset(Fmi::OGR::polyclip(*outshape, box));
    }

    // Logical operations with isobands are initialized before hand
    intersections.init(q, projection, valid_time, theState);

    // Calculate the isobands and store them into the template engine

    std::vector<SmartMet::Engine::Contour::Range> limits;
    const auto& contourer = theState.getContourEngine();
    BOOST_FOREACH (const Isoband& isoband, isobands)
    {
      if (isoband.qid.empty())
        throw SmartMet::Spine::Exception(BCP, "All isobands must have a non-empty QID!");
      limits.push_back(SmartMet::Engine::Contour::Range(isoband.lolimit, isoband.hilimit));
    }

    SmartMet::Engine::Contour::Options options(param, valid_time, limits);

    if (multiplier || offset)
      options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

    options.filter_size = smoother.size;
    options.filter_degree = smoother.degree;

    if (interpolation == "linear")
      options.interpolation = SmartMet::Engine::Contour::Linear;
    else if (interpolation == "nearest")
      options.interpolation = SmartMet::Engine::Contour::Nearest;
    else if (interpolation == "discrete")
      options.interpolation = SmartMet::Engine::Contour::Discrete;
    else if (interpolation == "loglinear")
      options.interpolation = SmartMet::Engine::Contour::LogLinear;
    else
      throw SmartMet::Spine::Exception(
          BCP, "Unknown isoband interpolation method '" + interpolation + "'!");

    // Do the actual contouring, either full grid or just
    // a sampled section

    std::size_t qhash = SmartMet::Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    boost::hash_combine(valueshash, options.data_hash_value());
    std::string wkt = q->area().WKT();

    // Select the data

    if (!q->param(options.parameter.number()))
    {
      throw SmartMet::Spine::Exception(BCP,
                                       "Parameter '" + options.parameter.name() + "' unavailable!");
    }

    if (!q->firstLevel())
    {
      throw SmartMet::Spine::Exception(BCP, "Unable to set first level in querydata.");
    }

    // Select the level.
    if (options.level)
    {
      if (!q->selectLevel(*options.level))
      {
        throw SmartMet::Spine::Exception(BCP,
                                         "Level value " +
                                             boost::lexical_cast<std::string>(*options.level) +
                                             " is not available!");
      }
    }

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, valueshash, options.time);

    OGRSpatialReference* sr = nullptr;
    if (!has_data_proj)
      sr = crs.get();

    CoordinatesPtr coords = qEngine.getWorldCoordinates(q, sr);
    std::vector<OGRGeometryPtr> geoms = contourer.contour(qhash, wkt, *matrix, coords, options, sr);

    for (unsigned int i = 0; i < geoms.size(); i++)
    {
      OGRGeometryPtr geom = geoms[i];
      if (geom && !geom->IsEmpty())
      {
        OGRGeometryPtr geom2(Fmi::OGR::polyclip(*geom, box));
        const Isoband& isoband = isobands[i];

        // Do intersections if so requested

        if (geom2 && !geom2->IsEmpty() && inshape)
          geom2.reset(geom2->Intersection(inshape.get()));

        if (geom2 && !geom2->IsEmpty() && outshape)
          geom2.reset(geom2->Difference(outshape.get()));

        // Intersect with data too
        geom2 = intersections.intersect(geom2);

        // Finally produce output if we still have something left
        if (geom2 && !geom2->IsEmpty())
        {
          // Store the path with unique ID
          std::string iri = qid + (qid.empty() ? "" : ".") + isoband.qid;
          theGlobals["paths"][iri] = Fmi::OGR::exportToSvg(*geom2, box, 1);

          if (!theState.inDefs())
          {
            // Add the SVG use element
            CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
            tag_cdt["start"] = "<use";
            tag_cdt["end"] = "/>";
            theState.addAttributes(theGlobals, tag_cdt, isoband.attributes);
            tag_cdt["attributes"]["xlink:href"] = "#" + iri;
            group_cdt["tags"].PushBack(tag_cdt);
          }
        }
      }
    }
    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
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

std::size_t IsobandLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    boost::hash_combine(hash, SmartMet::Engine::Querydata::hash_value(getModel(theState)));
    boost::hash_combine(hash, Dali::hash_value(parameter));
    boost::hash_combine(hash, Dali::hash_value(level));
    boost::hash_combine(hash, Dali::hash_value(isobands, theState));
    boost::hash_combine(hash, Dali::hash_value(interpolation));
    boost::hash_combine(hash, Dali::hash_value(smoother, theState));
    boost::hash_combine(hash, Dali::hash_value(multiplier));
    boost::hash_combine(hash, Dali::hash_value(offset));
    boost::hash_combine(hash, Dali::hash_value(outside, theState));
    boost::hash_combine(hash, Dali::hash_value(inside, theState));
    boost::hash_combine(hash, Dali::hash_value(sampling, theState));
    boost::hash_combine(hash, Dali::hash_value(intersections, theState));
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
