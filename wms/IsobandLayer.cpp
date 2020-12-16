//======================================================================

#include "IsobandLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoband.h"
#include "Layer.h"
#include "State.h"
#include "ValueTools.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/contour/Interpolation.h>
#include <engines/gis/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/observation/Settings.h>
#include <engines/querydata/Model.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiGdalArea.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>
#include <macgyver/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <spine/ParameterTools.h>
#include <limits>

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
      throw Fmi::Exception(BCP, "JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theConfig.defaultPrecision("isoband");

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("isobands", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("isobands", isobands, json, theConfig);

    json = theJson.get("interpolation", nulljson);
    if (!json.isNull())
      interpolation = json.asString();

    json = theJson.get("extrapolation", nulljson);
    if (!json.isNull())
      extrapolation = json.asInt();

    json = theJson.get("smoother", nulljson);
    if (!json.isNull())
      smoother.init(json, theConfig);

    json = theJson.get("precision", nulljson);
    if (!json.isNull())
      precision = json.asDouble();

    json = theJson.get("minarea", nulljson);
    if (!json.isNull())
      minarea = json.asDouble();

    json = theJson.get("unit_conversion", nulljson);
    if (!json.isNull())
      unit_conversion = json.asString();

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

    json = theJson.get("heatmap", nulljson);
    if (!json.isNull())
      heatmap.init(json, theConfig);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

boost::shared_ptr<Engine::Querydata::QImpl> IsobandLayer::buildHeatmap(
    const Spine::Parameter& theParameter, const boost::posix_time::ptime& theTime, State& theState)
{
  try
  {
    if (!isFlashOrMobileProducer(*producer))
      throw Fmi::Exception(BCP, "Heatmap requires flash or mobile data!");

    auto valid_time_period = getValidTimePeriod();
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    Engine::Observation::Settings settings;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();
    settings.starttimeGiven = true;
    settings.stationtype = *producer;
    settings.timezone = "UTC";

    // Get actual data (flash coordinates plus parameter column values)
    auto& obsengine = theState.getObsEngine();
    settings.parameters.push_back(Spine::makeParameter("longitude"));
    settings.parameters.push_back(Spine::makeParameter("latitude"));
    settings.parameters.push_back(Spine::makeParameter(*parameter));

    settings.boundingBox = getClipBoundingBox(box, theState, crs);

    auto result = obsengine.values(settings);

    // Establish new projection and the required grid size of the desired resolution

    auto newarea = boost::make_shared<NFmiGdalArea>(
        "FMI", *crs, box.xmin(), box.ymin(), box.xmax(), box.ymax());
    double datawidth = newarea->WorldXYWidth() / 1000.0;  // view extent in kilometers
    double dataheight = newarea->WorldXYHeight() / 1000.0;
    unsigned int width = lround(datawidth / *heatmap.resolution);
    unsigned int height = lround(dataheight / *heatmap.resolution);

    if (width * height > heatmap.max_points)
      throw Fmi::Exception(
          BCP,
          (std::string("Heatmap too big (") + Fmi::to_string(width * height) + " points, max " +
           Fmi::to_string(heatmap.max_points) + "), increase resolution"));

    // Must use at least two grid points, value 1 would cause a segmentation fault in here

    width = std::max(width, 2u);
    height = std::max(height, 2u);

    NFmiGrid grid(newarea.get(), width, height);
    std::unique_ptr<heatmap_t, void (*)(heatmap_t*)> hm(nullptr, heatmap_free);
    unsigned radius;

    if (result)
    {
      const auto& values = *result;

      if (!values.empty())
      {
        // Station coordinates are WGS84

        auto transformation = theState.getGisEngine().getCoordinateTransformation(
            "WGS84", projection.getProjString());

        const auto nrows = values[0].size();

        hm.reset(heatmap_new(width, height));
        if (!hm)
          throw Fmi::Exception(BCP, "Heatmap allocation failed");

        radius = lround(*heatmap.radius / *heatmap.resolution);
        if (radius == 0)
          radius = 1;

        auto hms = heatmap.getStamp(radius);
        if (!hms)
          throw Fmi::Exception(BCP, "Heatmap stamp generation failed");

        for (std::size_t row = 0; row < nrows; ++row)
        {
          double lon = get_double(values.at(0).at(row));
          double lat = get_double(values.at(1).at(row));

          // The parameter value could be used to add weighted points
          //
          // double value = get_double(values.at(2).at(row));

          // Convert latlon to world coordinate if needed

          double x = lon;
          double y = lat;

          if (crs->IsGeographic() == 0)
            if (transformation->Transform(1, &x, &y) == 0)
              continue;

          // To pixel coordinate
          box.transform(x, y);

          // Skip if not inside desired area
          if (Properties::inside(box, x, y))
          {
            NFmiPoint gridPoint = grid.LatLonToGrid(lon, lat);
            heatmap_add_point_with_stamp(
                hm.get(), lround(gridPoint.X()), lround(gridPoint.Y()), hms.get());
          }
        }
      }
    }

    // Establish the new descriptors

    NFmiLevelBag lbag;
    lbag.AddLevel(NFmiLevel(kFmiAnyLevelType, 0));

    NFmiVPlaceDescriptor vdesc(lbag);

    NFmiParamBag pbag;
    pbag.Add(NFmiParam(1));
    NFmiParamDescriptor pdesc(pbag);

    NFmiTimeList tlist;
    tlist.Add(new NFmiMetTime(theTime));  // NOLINT(cppcoreguidelines-owning-memory)
    NFmiTimeDescriptor tdesc(theTime, tlist);

    NFmiHPlaceDescriptor hdesc(grid);

    // Then create the new querydata

    NFmiFastQueryInfo info(pdesc, tdesc, hdesc, vdesc);
    boost::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
    if (data == nullptr)
      throw Fmi::Exception(BCP, "Failed to create heatmap");

    NFmiFastQueryInfo dstinfo(data.get());
    dstinfo.First();

    if (result && hm && hm->buf != nullptr)
    {
      float* v = hm->buf;

      for (dstinfo.ResetLocation(); dstinfo.NextLocation();)
        dstinfo.FloatValue(*v++);
    }

    // Return the new Q but with a new hash value

    std::size_t hash = 0;
    boost::hash_combine(hash, theParameter);
    boost::hash_combine(hash, to_simple_string(valid_time_period.begin()));
    boost::hash_combine(hash, to_simple_string(valid_time_period.end()));
    boost::hash_combine(hash, box.xmin());
    boost::hash_combine(hash, box.ymin());
    boost::hash_combine(hash, box.xmax());
    boost::hash_combine(hash, box.ymax());
    boost::hash_combine(hash, Dali::hash_value(heatmap, theState));
    boost::hash_combine(hash, radius);

    char* tmp;
    crs->exportToWkt(&tmp);
    boost::hash_combine(hash, tmp);
    OGRFree(tmp);

    auto model = boost::make_shared<Engine::Querydata::Model>(data, hash);
    return boost::make_shared<Engine::Querydata::QImpl>(model);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void IsobandLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // Establish the data
    auto q = getModel(theState);

    if (q && !(q->isGrid()))
      throw Fmi::Exception(BCP, "Isoband-layer can't use point data!");

    // Establish the parameter
    //
    // Heatmap does not use the parameter currently (only flash or mobile coordinates)
    bool allowUnknownParam = (theState.isObservation(producer) &&
                              isFlashOrMobileProducer(*producer) && heatmap.resolution);

    if (!parameter)
      throw Fmi::Exception(BCP, "Parameter not set for isoband-layer!");
    auto param = Spine::ParameterFactory::instance().parse(*parameter, allowUnknownParam);

    // Establish the valid time

    auto valid_time = getValidTime();

    // Establish the level

    if (q && !q->firstLevel())
      throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

    if (level)
    {
      if (!q)
        throw Fmi::Exception(BCP, "Cannot generate isobands without gridded level data");

      if (!q->selectLevel(*level))
        throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available!");
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
      if (!q)
        throw Fmi::Exception(BCP, "Cannot resample without gridded data");

      if (heatmap.resolution)
        throw Fmi::Exception(BCP, "Isoband-layer can't use both sampling and heatmap!");

      std::string report2 = "IsobandLayer::resample finished in %t sec CPU, %w sec real\n";
      boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer2;
      if (theState.useTimer())
        timer2 = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report2);

      auto demdata = theState.getGeoEngine().dem();
      auto landdata = theState.getGeoEngine().landCover();
      if (!demdata || !landdata)
        throw Fmi::Exception(BCP,
                               "Resampling data requires DEM and land cover data to be available!");

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
    else if (heatmap.resolution)
    {
      q = buildHeatmap(param, valid_time, theState);
    }
    else if (theState.isObservation(producer))
      throw Fmi::Exception(
          BCP, "Can't produce isobandlayer from observation data without heatmap configuration!");

    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate isobands without gridded data");

    // Logical operations with maps require shapes

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape, outshape;
    if (inside)
    {
      inshape = gis.getShape(crs.get(), inside->options);
      if (!inshape)
        throw Fmi::Exception(BCP, "Received empty inside-shape from database!");

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

    // Calculate the isobands and store them into the template engine

    std::vector<Engine::Contour::Range> limits;
    const auto& contourer = theState.getContourEngine();
    for (const Isoband& isoband : isobands)
      limits.emplace_back(Engine::Contour::Range(isoband.lolimit, isoband.hilimit));

    Engine::Contour::Options options(param, valid_time, limits);
    options.level = level;

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    if (multiplier || offset)
      options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

    options.minarea = minarea;

    options.filter_size = smoother.size;
    options.filter_degree = smoother.degree;

    options.extrapolation = extrapolation;

    if (interpolation == "linear")
      options.interpolation = Engine::Contour::Linear;
    else if (interpolation == "nearest")
      options.interpolation = Engine::Contour::Nearest;
    else if (interpolation == "discrete")
      options.interpolation = Engine::Contour::Discrete;
    else if (interpolation == "loglinear")
      options.interpolation = Engine::Contour::LogLinear;
    else
      throw Fmi::Exception(BCP, "Unknown isoband interpolation method '" + interpolation + "'!");

    // Do the actual contouring, either full grid or just
    // a sampled section

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    boost::hash_combine(valueshash, options.data_hash_value());
    std::string wkt = q->area().WKT();

    // Select the data

    if (heatmap.resolution)
    {
      // Heatmap querydata has just 1 fixed parameter (1/pressure)
      options.parameter = Spine::ParameterFactory::instance().parse("1");
    }

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

    CoordinatesPtr coords = qEngine.getWorldCoordinates(q, crs.get());
    std::vector<OGRGeometryPtr> geoms =
        contourer.contour(qhash, wkt, *matrix, coords, options, q->needsWraparound(), crs.get());

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate isobands as use tags statements inside <g>..</g>

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    // Add attributes to the group, not the isobands
    theState.addAttributes(theGlobals, group_cdt, attributes);

    for (unsigned int i = 0; i < geoms.size(); i++)
    {
      OGRGeometryPtr geom = geoms[i];
      if (geom && geom->IsEmpty() == 0)
      {
        OGRGeometryPtr geom2(Fmi::OGR::polyclip(*geom, clipbox));
        const Isoband& isoband = isobands[i];

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
          // Store the path with unique ID
          std::string iri = qid + (qid.empty() ? "" : ".") + isoband.getQid(theState);

          if (!theState.addId(iri))
            throw Fmi::Exception(BCP, "Non-unique ID assigned to isoband")
                .addParameter("ID", iri);

          CTPP::CDT isoband_cdt(CTPP::CDT::HASH_VAL);
          isoband_cdt["iri"] = iri;
          isoband_cdt["time"] = Fmi::to_iso_extended_string(valid_time);
          isoband_cdt["parameter"] = *parameter;
          isoband_cdt["data"] = Geometry::toString(*geom2, theState, box, crs, precision);
          isoband_cdt["type"] = Geometry::name(*geom2, theState);
          isoband_cdt["layertype"] = "isoband";

          // Use null to indicate unset values in GeoJSON
          if (isoband.lolimit)
            isoband_cdt["lolimit"] = *isoband.lolimit;
          else
            isoband_cdt["lolimit"] = "null";

          if (isoband.hilimit)
            isoband_cdt["hilimit"] = *isoband.hilimit;
          else
            isoband_cdt["hilimit"] = "null";

          theState.addPresentationAttributes(isoband_cdt, css, attributes, isoband.attributes);

          theGlobals["paths"][iri] = isoband_cdt;

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
    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void IsobandLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  if (theState.isObservation(producer))
    return;
  if (parameter)
  {
    ParameterInfo info(*parameter);
    info.producer = producer;
    info.level = level;
    add(infos, info);
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

    if (!theState.isObservation(producer))
      Dali::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));
    Dali::hash_combine(hash, Dali::hash_value(parameter));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(isobands, theState));
    Dali::hash_combine(hash, Dali::hash_value(interpolation));
    Dali::hash_combine(hash, Dali::hash_value(smoother, theState));
    Dali::hash_combine(hash, Dali::hash_value(extrapolation));
    Dali::hash_combine(hash, Dali::hash_value(precision));
    Dali::hash_combine(hash, Dali::hash_value(minarea));
    Dali::hash_combine(hash, Dali::hash_value(unit_conversion));
    Dali::hash_combine(hash, Dali::hash_value(multiplier));
    Dali::hash_combine(hash, Dali::hash_value(offset));
    Dali::hash_combine(hash, Dali::hash_value(outside, theState));
    Dali::hash_combine(hash, Dali::hash_value(inside, theState));
    Dali::hash_combine(hash, Dali::hash_value(sampling, theState));
    Dali::hash_combine(hash, Dali::hash_value(intersections, theState));
    Dali::hash_combine(hash, Dali::hash_value(heatmap, theState));
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
