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
#include <engines/grid/Engine.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImagePaint.h>
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
      throw Fmi::Exception(BCP, "Isoline-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theConfig.defaultPrecision("isoline");

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("geometryId", nulljson);
    if (!json.isNull())
      geometryId = json.asInt();

    json = theJson.get("levelId", nulljson);
    if (!json.isNull())
      levelId = json.asInt();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("forecastType", nulljson);
    if (!json.isNull())
      forecastType = json.asInt();

    json = theJson.get("forecastNumber", nulljson);
    if (!json.isNull())
      forecastNumber = json.asInt();

    auto request = theState.getRequest();

    boost::optional<std::string> v = request.getParameter("geometryId");
    if (v)
      geometryId = toInt32(*v);

    v = request.getParameter("levelId");
    if (v)
      levelId = toInt32(*v);

    v = request.getParameter("level");
    if (v)
      level = toInt32(*v);

    v = request.getParameter("forecastType");
    if (v)
      forecastType = toInt32(*v);

    v = request.getParameter("forecastNumber");
    if (v)
      forecastNumber = toInt32(*v);

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
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the isoline vectors based on the settings
 *
 * Note: This is needed by the derived IsolabelLayer class
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> IsolineLayer::getIsolines(const std::vector<double> isovalues,
                                                      State& theState)
{
  std::vector<OGRGeometryPtr> geoms;
  if (source && *source == "grid")
    geoms = getIsolinesGrid(isovalues, theState);
  else
    geoms = getIsolinesQuerydata(isovalues, theState);

  // Establish clipping box

  const auto& box = projection.getBox();
  const auto clipbox = getClipBox(box);

  // Logical operations with maps require shapes

  const auto& gis = theState.getGisEngine();

  auto crs = projection.getCRS();
  auto valid_time = getValidTime();

  OGRGeometryPtr inshape, outshape;
  if (inside)
  {
    inshape = gis.getShape(crs.get(), inside->options);
    if (!inshape)
      throw Fmi::Exception(BCP, "IsolineLayer received empty inside-shape from database");
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

  // Perform polygon operations
  for (unsigned int i = 0; i < geoms.size(); i++)
  {
    OGRGeometryPtr geom = geoms[i];
    if (geom && geom->IsEmpty() == 0)
    {
      OGRGeometryPtr geom2(Fmi::OGR::lineclip(*geom, clipbox));

      // Do intersections if so requested

      if (geom2 && geom2->IsEmpty() == 0 && inshape)
        geom2.reset(geom2->Intersection(inshape.get()));

      if (geom2 && geom2->IsEmpty() == 0 && outshape)
        geom2.reset(geom2->Difference(outshape.get()));

      // Intersect with data too
      geom2 = intersections.intersect(geom2);

      geoms[i] = geom2;
    }
  }

  return geoms;
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the isoline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> IsolineLayer::getIsolinesGrid(const std::vector<double> isovalues,
                                                          State& theState)
{
  if (!parameter)
    throw Fmi::Exception(BCP, "Parameter not set for isoline-layer");

  auto gridEngine = theState.getGridEngine();
  std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
  QueryServer::QueryConfigurator queryConfigurator;
  T::AttributeList attributeList;

  std::string producerName = gridEngine->getProducerName(*producer);

  // std::cout << valid_time << "TIMEZONE " << tz << "\n";

  T::ParamValue_vec contourValues;
  for (auto value : isovalues)
    contourValues.push_back(value);

  // Alter units if requested
  if (!unit_conversion.empty())
  {
    auto conv = theState.getConfig().unitConversion(unit_conversion);
    multiplier = conv.multiplier;
    offset = conv.offset;
  }

  std::string wkt = *projection.crs;
  // std::cout << wkt << "\n";

  if (wkt != "data")
  {
    // Getting WKT and the bounding box of the requested projection.

    auto crs = projection.getCRS();
    char* out = nullptr;
    crs->exportToWkt(&out);
    wkt = out;
    CPLFree(out);

    // Adding the bounding box information into the query.

    char bbox[100];

    auto bl = projection.bottomLeftLatLon();
    auto tr = projection.topRightLatLon();
    sprintf(bbox, "%f,%f,%f,%f", bl.X(), bl.Y(), tr.X(), tr.Y());
    originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

    const auto& box = projection.getBox();
    sprintf(bbox, "%f,%f,%f,%f", box.xmin(), box.ymin(), box.xmax(), box.ymax());
    originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
  }
  else
  {
    // The requested projection is the same as the projection of the requested data. This means
    // that we we do not know the actual projection yet and we have to wait that the grid-engine
    // delivers us the requested data and the projection information of the current data.
  }

  // Adding parameter information into the query.

  std::string pName = *parameter;
  auto pos = pName.find(".raw");
  if (pos != std::string::npos)
  {
    attributeList.addAttribute("areaInterpolationMethod",
                               std::to_string(T::AreaInterpolationMethod::Linear));
    pName.erase(pos, 4);
  }

  std::string param = gridEngine->getParameterString(producerName, pName);
  attributeList.addAttribute("param", param);

  if (!projection.projectionParameter)
    projection.projectionParameter = param;

  if (param == *parameter && originalGridQuery->mProducerNameList.size() == 0)
  {
    gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
    if (originalGridQuery->mProducerNameList.size() == 0)
      originalGridQuery->mProducerNameList.push_back(producerName);
  }

  std::string forecastTime = Fmi::to_iso_string(*time);
  attributeList.addAttribute("startTime", forecastTime);
  attributeList.addAttribute("endTime", forecastTime);
  attributeList.addAttribute("timelist", forecastTime);
  attributeList.addAttribute("timezone", "UTC");

  if (origintime)
    attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

  // Tranforming information from the attribute list into the query object.
  queryConfigurator.configure(*originalGridQuery, attributeList);

  // Fullfilling information into the query object.

  for (auto it = originalGridQuery->mQueryParameterList.begin(); it != originalGridQuery->mQueryParameterList.end(); ++it)
  {
    it->mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
    it->mType = QueryServer::QueryParameter::Type::Isoline;
    it->mContourLowValues = contourValues;

    if (geometryId)
      it->mGeometryId = *geometryId;

    if (levelId)
      it->mParameterLevelId = *levelId;

    if (level)
      it->mParameterLevel = C_INT(*level);

    if (forecastType)
      it->mForecastType = C_INT(*forecastType);

    if (forecastNumber)
      it->mForecastNumber = C_INT(*forecastNumber);
  }

  originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
  originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

  if (projection.size && *projection.size > 0)
  {
    originalGridQuery->mAttributeList.addAttribute("grid.size", std::to_string(*projection.size));
  }
  else
  {
    if (projection.xsize)
      originalGridQuery->mAttributeList.addAttribute("grid.width", std::to_string(*projection.xsize));

    if (projection.ysize)
      originalGridQuery->mAttributeList.addAttribute("grid.height", std::to_string(*projection.ysize));
  }

  if (wkt == "data" && projection.x1 && projection.y1 && projection.x2 && projection.y2)
  {
    char bbox[100];
    sprintf(bbox, "%f,%f,%f,%f", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
    originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
  }

  if (smoother.size)
    originalGridQuery->mAttributeList.addAttribute("contour.smooth.size", std::to_string(*smoother.size));

  if (smoother.degree)
    originalGridQuery->mAttributeList.addAttribute("contour.smooth.degree", std::to_string(*smoother.degree));

  if (minarea)
    originalGridQuery->mAttributeList.addAttribute("contour.minArea", std::to_string(*minarea));

  originalGridQuery->mAttributeList.addAttribute("contour.extrapolation", std::to_string(extrapolation));

  if (extrapolation)
    originalGridQuery->mAttributeList.addAttribute("contour.multiplier", std::to_string(*multiplier));

  if (offset)
    originalGridQuery->mAttributeList.addAttribute("contour.offset", std::to_string(*offset));

  originalGridQuery->mAttributeList.setAttribute(
      "contour.coordinateType",
      std::to_string(static_cast<int>(T::CoordinateTypeValue::ORIGINAL_COORDINATES)));
  // query.mAttributeList.setAttribute("contour.coordinateType",std::to_string(T::CoordinateTypeValue::LATLON_COORDINATES));
  // query.mAttributeList.setAttribute("contour.coordinateType",std::to_string(T::CoordinateTypeValue::GRID_COORDINATES));

  // The Query object before the query execution.
  // query.print(std::cout,0,0);

  // Executing the query.
  std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

  // Converting the returned WKB-isolines into OGRGeometry objects.

  std::vector<OGRGeometryPtr> geoms;
  for (auto param = query->mQueryParameterList.begin(); param != query->mQueryParameterList.end();
       ++param)
  {
    for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
    {
      if ((*val)->mValueData.size() > 0)
      {
        fileId = (*val)->mFileId[0];
        messageIndex = (*val)->mMessageIndex[0];

        for (auto wkb = (*val)->mValueData.begin(); wkb != (*val)->mValueData.end(); ++wkb)
        {
          unsigned char* cwkb = reinterpret_cast<unsigned char*>(wkb->data());
          OGRGeometry* geom = nullptr;
          OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb->size());
          if (geom != nullptr)
          {
            auto geomPtr = OGRGeometryPtr(geom);
            geoms.push_back(geomPtr);
          }
        }
      }
    }
  }

  // Extracting the projection information from the query result.

  const char* crsStr = query->mAttributeList.getAttributeValue("grid.crs");

  if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
  {
    const char* widthStr = query->mAttributeList.getAttributeValue("grid.width");
    const char* heightStr = query->mAttributeList.getAttributeValue("grid.height");

    if (widthStr != nullptr)
      projection.xsize = atoi(widthStr);

    if (heightStr != nullptr)
      projection.ysize = atoi(heightStr);
  }

  if (!projection.xsize && !projection.ysize)
    throw Fmi::Exception(BCP, "The projection size is unknown!");

  if (crsStr != nullptr && *projection.crs == "data")
  {
    projection.crs = crsStr;
    std::vector<double> partList;

    if (!projection.bboxcrs)
    {
      const char* bboxStr = query->mAttributeList.getAttributeValue("grid.bbox");

      if (bboxStr != nullptr)
        splitString(bboxStr, ',', partList);

      if (partList.size() == 4)
      {
        projection.x1 = partList[0];
        projection.y1 = partList[1];
        projection.x2 = partList[2];
        projection.y2 = partList[3];
      }
    }
  }

  return geoms;
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the isoline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> IsolineLayer::getIsolinesQuerydata(const std::vector<double> isovalues,
                                                               State& theState)
{
  // Establish the data. Store to member variable for IsolabelLayer use
  q = getModel(theState);

  if (q && !(q->isGrid()))
    throw Fmi::Exception(BCP, "Isoline-layer can't use point data!");

  // Establish the desired direction parameter

  if (!parameter)
    throw Fmi::Exception(BCP, "Parameter not set for isoline-layer");

  auto param = Spine::ParameterFactory::instance().parse(*parameter);

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

  // Sample to higher resolution if necessary

  auto sampleresolution = sampling.getResolution(projection);
  if (sampleresolution)
  {
    if (!q)
      throw Fmi::Exception(BCP, "Cannot resample without gridded data");

    auto demdata = theState.getGeoEngine().dem();
    auto landdata = theState.getGeoEngine().landCover();
    if (!demdata || !landdata)
      throw Fmi::Exception(
          BCP, "Resampling data in IsolineLayer requires DEM and land cover data to be available");

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

  if (!q)
    throw Fmi::Exception(BCP, "Cannot generate isobands without gridded data");

  // Select the level.

  if (!q->firstLevel())
    throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

  // Logical operations with isobands are initialized before hand
  intersections.init(producer, projection, valid_time, theState);

  // Calculate the isolines and store them into the template engine

  // TODO(mheiskan): Clean up Options API to use optionals
  const auto& contourer = theState.getContourEngine();

  Engine::Contour::Options options(param, valid_time, isovalues);
  options.level = level;

  options.minarea = minarea;

  // Set the requested level

  if (!q->firstLevel())
    throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

  options.level = level;

  if (options.level)
  {
    if (!q->selectLevel(*options.level))
    {
      throw Fmi::Exception(
          BCP,
          "Level value " + boost::lexical_cast<std::string>(*options.level) + " is not available.");
    }
  }

  // Alter units if requested

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

  options.extrapolation = extrapolation;

  // Do the actual contouring, either full grid or just
  // a sampled section

  std::size_t qhash = Engine::Querydata::hash_value(q);
  auto valueshash = qhash;
  Dali::hash_combine(valueshash, options.data_hash_value());

  std::string wkt = q->area().WKT();

  const auto& qEngine = theState.getQEngine();
  auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

  CoordinatesPtr coords = qEngine.getWorldCoordinates(q, crs.get());
  auto geoms =
      contourer.contour(qhash, wkt, *matrix, coords, options, q->needsWraparound(), crs.get());

  return geoms;
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

    std::vector<double> isovalues;
    for (const Isoline& isoline : isolines)
      isovalues.push_back(isoline.value);

    auto geoms = getIsolines(isovalues, theState);

    // The above call guarantees these have been resolved:
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

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

    for (unsigned int i = 0; i < geoms.size(); i++)
    {
      OGRGeometryPtr geom = geoms[i];
      if (geom && geom->IsEmpty() == 0)
      {
        const Isoline& isoline = isolines[i];

        // Store the path with unique QID
        std::string iri = qid + (qid.empty() ? "" : ".") + isoline.getQid(theState);

        if (!theState.addId(iri))
          throw Fmi::Exception(BCP, "Non-unique ID assigned to isoline").addParameter("ID", iri);

        CTPP::CDT isoline_cdt(CTPP::CDT::HASH_VAL);
        isoline_cdt["iri"] = iri;
        isoline_cdt["time"] = Fmi::to_iso_extended_string(getValidTime());
        isoline_cdt["parameter"] = *parameter;
        isoline_cdt["type"] = Geometry::name(*geom, theState);
        isoline_cdt["layertype"] = "isoline";
        isoline_cdt["data"] = Geometry::toString(*geom, theState, box, crs, precision);
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

void IsolineLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
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

std::size_t IsolineLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    if (!(source && *source == "grid"))
      Dali::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

    Dali::hash_combine(hash, Dali::hash_value(parameter));
    Dali::hash_combine(hash, Dali::hash_value(geometryId));
    Dali::hash_combine(hash, Dali::hash_value(levelId));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(forecastType));
    Dali::hash_combine(hash, Dali::hash_value(forecastNumber));
    Dali::hash_combine(hash, Dali::hash_value(isolines, theState));
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
