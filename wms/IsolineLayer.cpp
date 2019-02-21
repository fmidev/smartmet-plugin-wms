//======================================================================

#include "IsolineLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoline.h"
#include "Layer.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
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

void IsolineLayer::init(const Json::Value& theJson,
                        const State& theState,
                        const Config& theConfig,
                        const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Isoline-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theConfig.defaultPrecision("isoline");

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("isolines", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("isolines", isolines, json, theConfig);

    json = theJson.get("smoother", nulljson);
    if (!json.isNull())
      smoother.init(json, theConfig);

    json = theJson.get("extrapolation", nulljson);
    if (!json.isNull())
      extrapolation = json.asInt();

    json = theJson.get("precision", nulljson);
    if (!json.isNull())
      precision = json.asDouble();

    json = theJson.get("minarea", nulljson);
    if (!json.isNull())
      minarea = json.asDouble();

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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void IsolineLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::string report = "IsolineLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // Establish the data
    auto q = getModel(theState);

    if (q && !(q->isGrid()))
      throw Spine::Exception(BCP, "Isoline-layer can't use point data!");

    // Establish the desired direction parameter

    if (!parameter)
      throw Spine::Exception(BCP, "Parameter not set for isoline-layer");

    auto param = Spine::ParameterFactory::instance().parse(*parameter);

    // Establish the valid time

    auto valid_time = getValidTime();

    // Establish the level

    if (level)
    {
      bool match = false;
      for (q->resetLevel(); !match && q->nextLevel();)
        match = (q->levelValue() == *level);

      if (!match)
        throw Spine::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available");
    }

    // Get projection details

    projection.update(q);
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Sample to higher resolution if necessary

    auto sampleresolution = sampling.getResolution(projection);
    if (sampleresolution)
    {
      auto demdata = theState.getGeoEngine().dem();
      auto landdata = theState.getGeoEngine().landCover();
      if (!demdata || !landdata)
        throw Spine::Exception(
            BCP,
            "Resampling data in IsolineLayer requires DEM and land cover data to be available");

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

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate isolines as use tags statements inside <g>..</g>

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    // Add attributes to the group, not the isobands
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Logical operations with maps require shapes

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape, outshape;
    if (inside)
    {
      inshape = gis.getShape(crs.get(), inside->options);
      if (!inshape)
        throw Spine::Exception(BCP, "IsolineLayer received empty inside-shape from database");
      inshape.reset(Fmi::OGR::polyclip(*inshape, clipbox));
    }
    if (outside)
    {
      outshape = gis.getShape(crs.get(), outside->options);
      if (outshape)
        outshape.reset(Fmi::OGR::polyclip(*outshape, clipbox));
    }

    // Logical operations with isobands are initialized before hand
    intersections.init(producer, projection, valid_time, theState);

    // Calculate the isolines and store them into the template engine

    // TODO(mheiskan): Clean up Options API to use optionals
    const auto& contourer = theState.getContourEngine();
    std::vector<double> isoline_values;
    for (const Isoline& isoline : isolines)
      isoline_values.push_back(isoline.value);

    Engine::Contour::Options options(param, valid_time, isoline_values);

    options.minarea = minarea;

    if (multiplier || offset)
      options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

    options.filter_size = smoother.size;
    options.filter_degree = smoother.degree;

    options.extrapolation = extrapolation;

    // Do the actual contouring, either full grid or just
    // a sampled section

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    Dali::hash_combine(valueshash, options.data_hash_value());

    std::string wkt = q->area().WKT();

    // Select the data

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
    auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

    CoordinatesPtr coords = qEngine.getWorldCoordinates(q, crs.get());
    std::vector<OGRGeometryPtr> geoms =
        contourer.contour(qhash, wkt, *matrix, coords, options, q->needsWraparound(), crs.get());

    for (unsigned int i = 0; i < geoms.size(); i++)
    {
      OGRGeometryPtr geom = geoms[i];
      if (geom && geom->IsEmpty() == 0)
      {
        OGRGeometryPtr geom2(Fmi::OGR::lineclip(*geom, clipbox));
        const Isoline& isoline = isolines[i];

        // Do intersections if so requested

        if (geom2 && geom2->IsEmpty() == 0 && inshape)
          geom2.reset(geom2->Intersection(inshape.get()));

        if (geom2 && geom2->IsEmpty() == 0 && outshape)
          geom2.reset(geom2->Difference(outshape.get()));

        // Intersect with data too
        geom2 = intersections.intersect(geom2);

        // Finally produce output if we still have something left
        if (geom2 && geom2->IsEmpty() == 0)
        {
          // Store the path with unique QID
          std::string iri = qid + (qid.empty() ? "" : ".") + isoline.getQid(theState);

          if (!theState.addId(iri))
            throw Spine::Exception(BCP, "Non-unique ID assigned to isoline")
                .addParameter("ID", iri);

          CTPP::CDT isoline_cdt(CTPP::CDT::HASH_VAL);
          isoline_cdt["iri"] = iri;
          isoline_cdt["time"] = Fmi::to_iso_extended_string(valid_time);
          isoline_cdt["parameter"] = *parameter;
          isoline_cdt["type"] = Geometry::name(*geom2, theState.getType());
          isoline_cdt["layertype"] = "isoline";
          isoline_cdt["data"] = Geometry::toString(*geom2, theState.getType(), box, crs, precision);
          isoline_cdt["value"] = isoline.value;

          theState.addPresentationAttributes(isoline_cdt, css, attributes, isoline.attributes);

          theGlobals["paths"][iri] = isoline_cdt;

          // Add the SVG use element
          CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
          tag_cdt["start"] = "<use";
          tag_cdt["end"] = "/>";
          theState.addAttributes(theGlobals, tag_cdt, isoline.attributes);
          tag_cdt["attributes"]["xlink:href"] = "#" + iri;
          group_cdt["tags"].PushBack(tag_cdt);
        }
      }
    }
    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
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

std::size_t IsolineLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Dali::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));
    Dali::hash_combine(hash, Dali::hash_value(parameter));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(isolines, theState));
    Dali::hash_combine(hash, Dali::hash_value(smoother, theState));
    Dali::hash_combine(hash, Dali::hash_value(extrapolation));
    Dali::hash_combine(hash, Dali::hash_value(precision));
    Dali::hash_combine(hash, Dali::hash_value(minarea));
    Dali::hash_combine(hash, Dali::hash_value(multiplier));
    Dali::hash_combine(hash, Dali::hash_value(offset));
    Dali::hash_combine(hash, Dali::hash_value(outside, theState));
    Dali::hash_combine(hash, Dali::hash_value(inside, theState));
    Dali::hash_combine(hash, Dali::hash_value(sampling, theState));
    Dali::hash_combine(hash, Dali::hash_value(intersections, theState));
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
