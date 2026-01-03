//======================================================================

#include "IsolineLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Isoline.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include "StyleSheet.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <trax/InterpolationType.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
bool skip_value(double value, const std::vector<double>& except_vector)
{
  if (except_vector.empty())
    return false;

  for (const auto& except : except_vector)
  {
    if (except != 0.0 && std::fmod(value, except) == 0.0)
      return true;
  }
  return false;
}

// Generate isolines with a step
void generate_isolines(std::vector<Isoline>& isolines,
                       double startvalue,
                       double endvalue,
                       double interval,
                       const std::string& pattern,
                       const std::vector<double>& except_vector)
{
  for (double i = startvalue; i <= endvalue; i += interval)  // NOLINT(cert-flp30-c)
  {
    if (!skip_value(i, except_vector))
    {
      Isoline isoline;
      isoline.qid = fmt::format(fmt::runtime(pattern), i);
      isoline.value = i;
      isolines.push_back(isoline);

      if (isolines.size() > 10000)
        throw Fmi::Exception(BCP, "Too many (> 10000) isolines");
    }
  }
}

// Make sure all isolines have a qid if an autoqid has been defines
void apply_autoqid(std::vector<Isoline>& isolines, const std::string& pattern)
{
  for (auto& isoline : isolines)
  {
    if (!pattern.empty() && isoline.qid.empty())
      isoline.qid = fmt::format(fmt::runtime(pattern), isoline.value);
    boost::replace_all(isoline.qid, ".", ",");  // replace decimal dots with ,
  }
}

// Generate a class attribute for those missing one
void apply_autoclass(std::vector<Isoline>& isolines, const std::string& pattern)
{
  if (pattern.empty())
    return;

  for (auto& isoline : isolines)
  {
    if (isoline.attributes.value("class").empty())
    {
      auto name = fmt::format(fmt::runtime(pattern), isoline.value);
      boost::replace_all(name, ".", ",");  // replace decimal dots with ,
      isoline.attributes.add("class", name);
    }
  }
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void IsolineLayer::init(Json::Value& theJson,
                        const State& theState,
                        const Config& theConfig,
                        const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isoline-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("isoline");

    // Extract member values

    JsonTools::remove_string(parameter, theJson, "parameter");

    // Support old qidprefix and new autoqid commands
    std::string autoqid;
    JsonTools::remove_string(autoqid, theJson, "autoqid");

    auto json = JsonTools::remove(theJson, "isolines");

    if (!json.isNull())
    {
      if (json.isArray())
        JsonTools::extract_array("isolines", isolines, json, theConfig);
      else if (!json.isObject())
        throw Fmi::Exception(BCP, "Isoline layer isolines setting must be an array or a group");
      else
      {
        std::optional<double> startvalue;
        std::optional<double> endvalue;
        std::optional<double> interval;

        JsonTools::remove_double(startvalue, json, "startvalue");
        JsonTools::remove_double(endvalue, json, "endvalue");
        JsonTools::remove_double(interval, json, "interval");

        if (!startvalue)
          throw Fmi::Exception(BCP, "isolines startvalue missing");
        if (!endvalue)
          throw Fmi::Exception(BCP, "isolines endvalue missing");
        if (!interval)
          throw Fmi::Exception(BCP, "isolines interval missing");

        if (*startvalue > *endvalue)
          throw Fmi::Exception(BCP, "isolines startvalue > endvalue");
        if (*interval <= 0)
          throw Fmi::Exception(BCP, "isolines interval must be positive");

        auto json_except = JsonTools::remove(json, "except");
        std::vector<double> except_vector;
        if (!json_except.isNull())
        {
          if (json_except.isArray())
          {
            for (const auto& e_json : json_except)
              except_vector.push_back(e_json.asDouble());
          }
          else
            except_vector.push_back(json_except.asDouble());
        }

        std::string qidprefix;
        JsonTools::remove_string(qidprefix, json, "qidprefix");

        if (autoqid.empty())  // use autoqid as is if it was given
        {
          if (!qidprefix.empty())
            autoqid = qidprefix + "_{}";  // else use "qidprefix_{}" if qidprefix was given
          else
            autoqid = "isoline_{}";  // else use "isoline_{}"
        }

        if (!json.empty())
        {
          auto names = json.getMemberNames();
          auto namelist = boost::algorithm::join(names, ",");
          throw Fmi::Exception(
              BCP, "Unknown member variables in Isoline layer isovalues setting: " + namelist);
        }

        generate_isolines(isolines, *startvalue, *endvalue, *interval, autoqid, except_vector);
      }
    }

    // Make sure everything has a unique qid
    apply_autoqid(isolines, autoqid);

    std::string autoclass;
    JsonTools::remove_string(autoclass, theJson, "autoclass");
    apply_autoclass(isolines, autoclass);

    json = JsonTools::remove(theJson, "smoother");
    smoother.init(json, theConfig);

    JsonTools::remove_string(interpolation, theJson, "interpolation");
    JsonTools::remove_int(extrapolation, theJson, "extrapolation");
    JsonTools::remove_double(precision, theJson, "precision");
    JsonTools::remove_double(minarea, theJson, "minarea");
    JsonTools::remove_string(areaunit, theJson, "areaunit");
    JsonTools::remove_string(unit_conversion, theJson, "unit_conversion");
    JsonTools::remove_double(multiplier, theJson, "multiplier");
    JsonTools::remove_double(offset, theJson, "offset");

    JsonTools::remove_bool(strict, theJson, "strict");
    JsonTools::remove_bool(validate, theJson, "validate");
    JsonTools::remove_bool(desliver, theJson, "desliver");

    json = JsonTools::remove(theJson, "outside");
    if (!json.isNull())
    {
      outside = Map();
      outside->init(json, theConfig);
    }

    json = JsonTools::remove(theJson, "inside");
    if (!json.isNull())
    {
      inside = Map();
      inside->init(json, theConfig);
    }

    json = JsonTools::remove(theJson, "sampling");
    sampling.init(json, theConfig);

    json = JsonTools::remove(theJson, "intersect");
    intersections.init(json, theConfig);

    json = JsonTools::remove(theJson, "filter");
    filter.init(json);
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

std::vector<OGRGeometryPtr> IsolineLayer::getIsolines(const std::vector<double>& isovalues,
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

  OGRGeometryPtr inshape;
  OGRGeometryPtr outshape;
  if (inside)
  {
    inshape = gis.getShape(&crs, inside->options);
    if (!inshape)
      throw Fmi::Exception(BCP, "IsolineLayer received empty inside-shape from database");
    inshape.reset(Fmi::OGR::polyclip(*inshape, clipbox));
  }
  if (outside)
  {
    outshape = gis.getShape(&crs, outside->options);
    if (outshape)
      outshape.reset(Fmi::OGR::polyclip(*outshape, clipbox));
  }

  // Logical operations with isobands are initialized before hand
  intersections.init(producer, projection, valid_time, theState);

  // Perform polygon operations
  for (auto& geom : geoms)
  {
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

      geom = geom2;
    }
  }

  return geoms;
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the isoline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> IsolineLayer::getIsolinesGrid(const std::vector<double>& isovalues,
                                                          State& theState)
{
  const auto* gridEngine = theState.getGridEngine();
  if (!gridEngine || !gridEngine->isEnabled())
    throw Fmi::Exception(BCP, "The grid-engine is disabled!");

  if (!parameter)
    throw Fmi::Exception(BCP, "Parameter not set for isoline-layer");

  std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
  QueryServer::QueryConfigurator queryConfigurator;
  T::AttributeList attributeList;

  std::string producerName = gridEngine->getProducerName(*producer);

  // std::cout << valid_time << "TIMEZONE " << tz << "\n";

  T::ParamValue_vec contourValues;
  std::copy(isovalues.begin(), isovalues.end(), std::back_inserter(contourValues));

  // Alter units if requested
  if (!unit_conversion.empty())
  {
    auto conv = theState.getConfig().unitConversion(unit_conversion);
    multiplier = conv.multiplier;
    offset = conv.offset;
  }

  const auto& box = projection.getBox();
  const auto clipbox = getClipBox(box);

  std::string wkt = *projection.crs;
  // std::cout << wkt << "\n";

  if (wkt != "data")
  {
    // Getting WKT and the bounding box of the requested projection.

    if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
      wkt = projection.getCRS().WKT();

    // Adding the bounding box information into the query.

    auto bbox =
        fmt::format("{},{},{},{}", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(), clipbox.ymax());
    auto bl = projection.bottomLeftLatLon();
    auto tr = projection.topRightLatLon();

    // # Testing if the target grid is defined as latlon:
    if (projection.x1 == bl.X() && projection.y1 == bl.Y() && projection.x2 == tr.X() &&
        projection.y2 == tr.Y())
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

    originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    if (!theState.animation_enabled)
      originalGridQuery->mAttributeList.addAttribute("grid.countSize", "1");
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
    attributeList.addAttribute("grid.areaInterpolationMethod",
                               Fmi::to_string(T::AreaInterpolationMethod::Nearest));
    pName.erase(pos, 4);
  }

  std::string param = gridEngine->getParameterString(producerName, pName);

  if (multiplier && *multiplier != 1.0)
    param = "MUL{" + param + ";" + std::to_string(*multiplier) + "}";

  if (offset && *offset)
    param = "SUM{" + param + ";" + std::to_string(*offset) + "}";

  attributeList.addAttribute("param", param);

  if (!projection.projectionParameter)
    projection.projectionParameter = param;

  if (param == *parameter && originalGridQuery->mProducerNameList.empty())
  {
    gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
    if (originalGridQuery->mProducerNameList.empty())
      originalGridQuery->mProducerNameList.push_back(producerName);
  }

  std::string forecastTime = Fmi::to_iso_string(getValidTime());
  attributeList.addAttribute("startTime", forecastTime);
  attributeList.addAttribute("endTime", forecastTime);
  attributeList.addAttribute("timelist", forecastTime);
  attributeList.addAttribute("timezone", "UTC");

  if (origintime)
    attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

  // Tranforming information from the attribute list into the query object.
  queryConfigurator.configure(*originalGridQuery, attributeList);

  // Fullfilling information into the query object.

  for (auto& query_param : originalGridQuery->mQueryParameterList)
  {
    query_param.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
    query_param.mType = QueryServer::QueryParameter::Type::Isoline;
    query_param.mContourLowValues = contourValues;

    if (geometryId)
      query_param.mGeometryId = *geometryId;

    if (levelId)
    {
      query_param.mParameterLevelId = *levelId;
    }

    if (level)
    {
      query_param.mParameterLevel = C_INT(*level);
    }
    else if (pressure)
    {
      query_param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
      query_param.mParameterLevel = C_INT(*pressure);
    }

    if (elevation_unit)
    {
      if (*elevation_unit == "m")
        query_param.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;

      if (*elevation_unit == "p")
        query_param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
    }

    if (forecastType)
      query_param.mForecastType = C_INT(*forecastType);

    if (forecastNumber)
      query_param.mForecastNumber = C_INT(*forecastNumber);
  }

  originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
  originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

  if (projection.size && *projection.size > 0)
  {
    originalGridQuery->mAttributeList.addAttribute("grid.size", Fmi::to_string(*projection.size));
  }
  else
  {
    if (projection.xsize)
      originalGridQuery->mAttributeList.addAttribute("grid.width",
                                                     Fmi::to_string(*projection.xsize));

    if (projection.ysize)
      originalGridQuery->mAttributeList.addAttribute("grid.height",
                                                     Fmi::to_string(*projection.ysize));
  }

  if (wkt == "data" && projection.x1 && projection.y1 && projection.x2 && projection.y2)
  {
    auto bbox =
        fmt::format("{},{},{},{}", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
    originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
  }

  if (interpolation == "linear")
    originalGridQuery->mAttributeList.addAttribute(
        "contour.interpolation.type", Fmi::to_string((int)Trax::InterpolationType::Linear));
  else if (interpolation == "logarithmic")
    originalGridQuery->mAttributeList.addAttribute(
        "contour.interpolation.type", Fmi::to_string((int)Trax::InterpolationType::Logarithmic));
  else
    throw Fmi::Exception(BCP, "Unknown isoline interpolation method '" + interpolation + "'!");

  if (smoother.size)
    originalGridQuery->mAttributeList.addAttribute("contour.smooth.size",
                                                   Fmi::to_string(*smoother.size));

  if (smoother.degree)
    originalGridQuery->mAttributeList.addAttribute("contour.smooth.degree",
                                                   Fmi::to_string(*smoother.degree));

  if (minarea)
  {
    const auto& box = projection.getBox();

    auto area = *minarea;
    if (areaunit == "px^2")
      area = box.areaFactor() * area;

    originalGridQuery->mAttributeList.addAttribute("contour.minArea", Fmi::to_string(area));
  }

  originalGridQuery->mAttributeList.addAttribute("contour.extrapolation",
                                                 Fmi::to_string(extrapolation));

  if (extrapolation)
    originalGridQuery->mAttributeList.addAttribute("contour.multiplier",
                                                   Fmi::to_string(*multiplier));

  if (offset)
    originalGridQuery->mAttributeList.addAttribute("contour.offset", Fmi::to_string(*offset));

  originalGridQuery->mAttributeList.setAttribute(
      "contour.coordinateType",
      Fmi::to_string(static_cast<int>(T::CoordinateTypeValue::ORIGINAL_COORDINATES)));
  // query.mAttributeList.setAttribute("contour.coordinateType",Fmi::to_string(T::CoordinateTypeValue::LATLON_COORDINATES));
  // query.mAttributeList.setAttribute("contour.coordinateType",Fmi::to_string(T::CoordinateTypeValue::GRID_COORDINATES));

  // The Query object before the query execution.
  // query.print(std::cout,0,0);

  // Executing the query.
  std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

  // Converting the returned WKB-isolines into OGRGeometry objects.

  std::vector<OGRGeometryPtr> geoms;
  for (const auto& param : query->mQueryParameterList)
  {
    for (const auto& val : param.mValueList)
    {
      if (!val->mValueData.empty())
      {
        fileId = val->mFileId[0];
        messageIndex = val->mMessageIndex[0];

        for (const auto& wkb : val->mValueData)
        {
          const auto* cwkb = reinterpret_cast<const unsigned char*>(wkb.data());
          OGRGeometry* geom = nullptr;
          OGRGeometryFactory::createFromWkb(cwkb, nullptr, &geom, wkb.size());
          auto geomPtr = OGRGeometryPtr(geom);
          geoms.push_back(geomPtr);
        }
      }
    }
  }

  // Extracting the projection information from the query result.

  if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
  {
    const char* widthStr = query->mAttributeList.getAttributeValue("grid.width");
    const char* heightStr = query->mAttributeList.getAttributeValue("grid.height");

    if (widthStr != nullptr)
      projection.xsize = Fmi::stoi(widthStr);

    if (heightStr != nullptr)
      projection.ysize = Fmi::stoi(heightStr);
  }

  if (!projection.xsize && !projection.ysize)
    throw Fmi::Exception(BCP, "The projection size is unknown!");

  if (*projection.crs == "data")
  {
    const char* crsStr = query->mAttributeList.getAttributeValue("grid.crs");
    const char* proj4Str = query->mAttributeList.getAttributeValue("grid.proj4");
    if (proj4Str != nullptr && strstr(proj4Str, "+lon_wrap") != nullptr)
      crsStr = proj4Str;

    if (crsStr != nullptr)
    {
      projection.crs = crsStr;
      if (!projection.bboxcrs)
      {
        const char* bboxStr = query->mAttributeList.getAttributeValue("grid.bbox");
        if (bboxStr != nullptr)
        {
          std::vector<double> partList;
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
    }
  }

  return geoms;
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the isoline vectors based on the settings
 */
// ----------------------------------------------------------------------

std::vector<OGRGeometryPtr> IsolineLayer::getIsolinesQuerydata(const std::vector<double>& isovalues,
                                                               const State& theState)
{
  // Establish the data. Store to member variable for IsolabelLayer use
  q = getModel(theState);

  if (q && !(q->isGrid()))
    throw Fmi::Exception(BCP, "Isoline-layer can't use point data!");

  // Establish the desired direction parameter

  if (!parameter)
    throw Fmi::Exception(BCP, "Parameter not set for isoline-layer");

  auto param = TS::ParameterFactory::instance().parse(*parameter);

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
  const auto& crs = projection.getCRS();
  const auto& box = projection.getBox();
  const auto clipbox = getClipBox(box);

  // Sample to higher resolution if necessary

  auto sampleresolution = sampling.getResolution(projection);
  if (sampleresolution)
  {
    if (!q)
      throw Fmi::Exception(BCP, "Cannot resample without gridded data");

    q = q->sample(
        param, valid_time, crs, box.xmin(), box.ymin(), box.xmax(), box.ymax(), *sampleresolution);
  }

  if (!q)
    throw Fmi::Exception(BCP, "Cannot generate isolines without gridded data");

  // Logical operations with maps require shapes

  const auto& gis = theState.getGisEngine();

  OGRGeometryPtr inshape;
  OGRGeometryPtr outshape;
  if (inside)
  {
    inshape = gis.getShape(&crs, inside->options);
    if (!inshape)
      throw Fmi::Exception(BCP, "IsolineLayer received empty inside-shape from database");
    inshape.reset(Fmi::OGR::polyclip(*inshape, clipbox));
  }
  if (outside)
  {
    outshape = gis.getShape(&crs, outside->options);
    if (outshape)
      outshape.reset(Fmi::OGR::polyclip(*outshape, clipbox));
  }

  // Logical operations with isobands are initialized before hand
  intersections.init(producer, projection, valid_time, theState);

  // Calculate the isolines and store them into the template engine

  // TODO(mheiskan): Clean up Options API to use optionals
  const auto& contourer = theState.getContourEngine();

  Engine::Contour::Options options(param, valid_time, isovalues);
  options.level = level;

  options.minarea = minarea;
  if (minarea)
  {
    if (areaunit == "px^2")
      options.minarea = box.areaFactor() * *minarea;
  }

  options.bbox = Fmi::BBox(box);

  // Set the requested level

  if (!q->firstLevel())
    throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

  if (options.level)
  {
    if (!q->selectLevel(*options.level))
    {
      throw Fmi::Exception(BCP,
                           "Level value " + Fmi::to_string(*options.level) + " is not available.");
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

  if (interpolation == "linear")
    options.interpolation = Trax::InterpolationType::Linear;
  else if (interpolation == "logarithmic")
    options.interpolation = Trax::InterpolationType::Logarithmic;
  else
    throw Fmi::Exception(BCP, "Unknown isoline interpolation method '" + interpolation + "'!");

  options.strict = strict;
  options.validate = validate;
  options.desliver = desliver;

  // Do the actual contouring, either full grid or just
  // a sampled section

  std::size_t qhash = Engine::Querydata::hash_value(q);
  auto valueshash = qhash;
  Fmi::hash_combine(valueshash, options.data_hash_value());

  const auto& qEngine = theState.getQEngine();
  auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

  // Avoid reprojecting data when sampling has been used for better speed (and accuracy)
  CoordinatesPtr coords;
  if (sampleresolution)
    coords = qEngine.getWorldCoordinates(q);
  else
    coords = qEngine.getWorldCoordinates(q, crs);

  auto geoms = contourer.contour(qhash, crs, *matrix, *coords, clipbox, options);

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

    if (isolines.empty())
      return;

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IsolineLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    std::vector<double> isovalues;
    isovalues.reserve(isolines.size());
    for (const Isoline& isoline : isolines)
      isovalues.push_back(isoline.value);

    auto geoms = getIsolines(isovalues, theState);

    // Output image CRS and BBOX
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Convert filter pixel distance to metric distance for smoothing
    filter.bbox(box);

    // Smoothen the isolines
    filter.apply(geoms, false);

    // Update the globals

    const bool topojson = (theState.getType() == "topojson");

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    CTPP::CDT object_cdt;
    std::string objectKey = "isoline:" + *parameter + ":" + qid;
    object_cdt["objectKey"] = objectKey;

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

        if (!theState.addId(iri) && !theState.animation_enabled)
          throw Fmi::Exception(BCP, "Non-unique ID assigned to isoline").addParameter("ID", iri);

        CTPP::CDT isoline_cdt(CTPP::CDT::HASH_VAL);
        isoline_cdt["iri"] = iri;
        isoline_cdt["time"] = Fmi::to_iso_extended_string(getValidTime());
        isoline_cdt["parameter"] = *parameter;
        isoline_cdt["type"] = Geometry::name(*geom, theState.getType());
        isoline_cdt["layertype"] = "isoline";

        std::string arcNumbers;
        std::string arcCoordinates;
        std::string pointCoordinates = Geometry::toString(*geom,
                                                          theState.getType(),
                                                          box,
                                                          crs,
                                                          precision,
                                                          theState.arcHashMap,
                                                          theState.arcCounter,
                                                          arcNumbers,
                                                          arcCoordinates);

        if (!pointCoordinates.empty())
          isoline_cdt["data"] = pointCoordinates;

        isoline_cdt["value"] = isoline.value;

        theState.addPresentationAttributes(isoline_cdt, css, attributes, isoline.attributes);

        if (topojson)
        {
          if (!arcNumbers.empty())
            isoline_cdt["arcs"] = arcNumbers;

          object_cdt["paths"][iri] = isoline_cdt;

          if (!arcCoordinates.empty())
          {
            CTPP::CDT arc_cdt(CTPP::CDT::HASH_VAL);
            arc_cdt["data"] = arcCoordinates;
            theGlobals["arcs"][theState.insertCounter] = arc_cdt;
            theState.insertCounter++;
          }
        }
        else
        {
          theGlobals["paths"][iri] = isoline_cdt;
        }

        // Add the SVG use element
        CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
        tag_cdt["start"] = "<use";
        tag_cdt["end"] = "/>";
        theState.addAttributes(theGlobals, tag_cdt, isoline.attributes);
        tag_cdt["attributes"]["xlink:href"] = "#" + iri;
        group_cdt["tags"].PushBack(tag_cdt);
      }
    }
    theGlobals["bbox"] = Fmi::to_string(box.xmin()) + "," + Fmi::to_string(box.ymin()) + "," +
                         Fmi::to_string(box.xmax()) + "," + Fmi::to_string(box.ymax());
    theGlobals["objects"][objectKey] = object_cdt;
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -(int)precision);

    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Operation failed!", nullptr);
    exception.printError();
    throw exception;
    // throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
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
      Fmi::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

    Fmi::hash_combine(hash, countParameterHash(theState, parameter));
    Fmi::hash_combine(hash, Dali::hash_value(isolines, theState));
    Fmi::hash_combine(hash, Dali::hash_value(smoother, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(interpolation));
    Fmi::hash_combine(hash, Fmi::hash_value(extrapolation));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Fmi::hash_value(minarea));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Dali::hash_value(outside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(inside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(sampling, theState));
    Fmi::hash_combine(hash, Dali::hash_value(intersections, theState));
    Fmi::hash_combine(hash, filter.hash_value());
    Fmi::hash_combine(hash, Fmi::hash_value(strict));
    Fmi::hash_combine(hash, Fmi::hash_value(validate));
    Fmi::hash_combine(hash, Fmi::hash_value(desliver));
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
