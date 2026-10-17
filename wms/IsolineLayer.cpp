//======================================================================

#include "IsolineLayer.h"
#include "Config.h"
#include "DataTile.h"
#include "Geometry.h"
#include "GridDataGeoTiff.h"
#include "GridQueryTools.h"
#include "Hash.h"
#include "Isoline.h"
#include "JsonTools.h"
#include "Layer.h"
#include "MapboxVectorTile.h"
#include "State.h"
#include "StyleSheet.h"
#include "SubdivideGate.h"
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
  // endvalue >= startvalue and interval>0 has already been checked
  for (auto i = 0UL;; ++i)
  {
    double isovalue = startvalue + i * interval;
    if (isovalue > endvalue)
      break;
    if (!skip_value(isovalue, except_vector))
    {
      Isoline isoline;
      isoline.qid = fmt::format(fmt::runtime(pattern), isovalue);
      isoline.value = isovalue;
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

// The isovalues to skip, given as a single number or an array
std::vector<double> except_values(const Json::Value& json_except)
{
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
  return except_vector;
}

// Generate isolines from startvalue, endvalue, interval and except settings
void generate_isoline_range(Json::Value& json, std::string& autoqid, std::vector<Isoline>& isolines)
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
  std::vector<double> except_vector = except_values(json_except);

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

    JsonTools::remove_string(paraminfo.parameter, theJson, "parameter");

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
        generate_isoline_range(json, autoqid, isolines);
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

    JsonTools::remove_bool(strict, theJson, "strict");
    JsonTools::remove_bool(validate, theJson, "validate");
    JsonTools::remove_bool(desliver, theJson, "desliver");
    JsonTools::remove_int(subdivide, theJson, "subdivide");
    JsonTools::remove_double(subdivide_min_cell_pixels, theJson, "subdivide_min_cell_pixels");

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

    json = JsonTools::remove(theJson, "tfp");
    if (!json.isNull())
      tfp = ComputedFields::parseTfpOptions(json);
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
                                                      const State& theState)
{
  std::vector<OGRGeometryPtr> geoms;

  if (paraminfo.source == std::string("grid"))
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
  intersections.init(paraminfo.producer, projection, valid_time, theState);

  // Perform polygon operations
  for (auto& geom : geoms)
  {
    if (geom && geom->IsEmpty() == 0)
    {
      OGRGeometryPtr geom1(geom->clone());
      Fmi::OGR::normalizeWindingOrder(geom1.get());

      OGRGeometryPtr geom2(Fmi::OGR::lineclip(*geom1, clipbox));

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
                                                          const State& theState)
{
  const auto* gridEngine = theState.getGridEngine();
  if (!gridEngine || !gridEngine->isEnabled())
    throw Fmi::Exception(BCP, "The grid-engine is disabled!");

  if (paraminfo.parameter.empty())
    throw Fmi::Exception(BCP, "Parameter not set for isoline-layer");

  std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
  QueryServer::QueryConfigurator queryConfigurator;
  T::AttributeList attributeList;

  std::string producerName = gridEngine->getProducerName(*paraminfo.producer);

  // std::cout << valid_time << "TIMEZONE " << tz << "\n";

  T::ParamValue_vec contourValues;
  std::copy(isovalues.begin(), isovalues.end(), std::back_inserter(contourValues));

  auto crs = projection.getCRS();
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

    add_grid_bbox_attributes(*originalGridQuery, projection, clipbox, theState.animation_enabled);
  }
  else
  {
    // The requested projection is the same as the projection of the requested data. This means
    // that we we do not know the actual projection yet and we have to wait that the grid-engine
    // delivers us the requested data and the projection information of the current data.
  }

  // Adding parameter information into the query.

  add_grid_parameter(*gridEngine,
                     producerName,
                     paraminfo.parameter,
                     multiplier,
                     offset,
                     projection,
                     attributeList,
                     *originalGridQuery);

  add_time_attributes(attributeList, getValidTime(), origintime);

  // Tranforming information from the attribute list into the query object.
  queryConfigurator.configure(*originalGridQuery, attributeList);

  // Fullfilling information into the query object.

  for (auto& query_param : originalGridQuery->mQueryParameterList)
  {
    query_param.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
    query_param.mType = QueryServer::QueryParameter::Type::Isoline;
    query_param.mContourLowValues = contourValues;

    set_query_parameter_selections(query_param, paraminfo);
  }

  originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
  originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

  add_grid_size_attributes(*originalGridQuery, projection);

  if (wkt == "data")
    add_data_bbox_attribute(*originalGridQuery, projection);

  if (interpolation == "linear")
    originalGridQuery->mAttributeList.addAttribute(
        "contour.interpolation.type", Fmi::to_string((int)Trax::InterpolationType::Linear));
  else if (interpolation == "logarithmic")
    originalGridQuery->mAttributeList.addAttribute(
        "contour.interpolation.type", Fmi::to_string((int)Trax::InterpolationType::Logarithmic));
  else
    throw Fmi::Exception(BCP, "Unknown isoline interpolation method '" + interpolation + "'!");

  add_contour_attributes(*originalGridQuery, smoother, subdivide, contour_threads);
  /*
    if (minarea)
    {
      const auto& box = projection.getBox();

      auto area = *minarea;
      if (areaunit == "px^2")
        area = box.areaFactor() * area;

      originalGridQuery->mAttributeList.addAttribute("contour.minArea", Fmi::to_string(area));
    }
  */

  add_contour_value_attributes(*originalGridQuery, extrapolation, multiplier, offset);

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

          geoms.push_back(despeckle_contour(geomPtr, minarea, areaunit, box, crs));
        }
      }
    }
  }

  // Extracting the projection information from the query result.

  update_projection_size(projection, *query);

  if (!projection.xsize && !projection.ysize)
    throw Fmi::Exception(BCP, "The projection size is unknown!");

  update_data_projection(projection, *query);

  return geoms;
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the isoline vectors based on the settings
 */
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
/*!
 * \brief Select the first level or the requested level of the data
 */
// ----------------------------------------------------------------------

void IsolineLayer::selectLevel() const
{
  if (q && !q->firstLevel())
    throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

  if (paraminfo.level)
  {
    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate isobands without gridded level data");

    if (!q->selectLevel(*paraminfo.level))
      throw Fmi::Exception(
          BCP, "Level value " + Fmi::to_string(*paraminfo.level) + " is not available!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch the inside and outside map shapes clipped to the clipping box
 */
// ----------------------------------------------------------------------

void IsolineLayer::getMapShapes(const Engine::Gis::Engine& gis,
                                const Fmi::SpatialReference& crs,
                                const Fmi::Box& clipbox,
                                OGRGeometryPtr& inshape,
                                OGRGeometryPtr& outshape) const
{
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
}

// ----------------------------------------------------------------------
/*!
 * \brief Contouring options for the querydata engine. Also selects the requested level.
 */
// ----------------------------------------------------------------------

Engine::Contour::Options IsolineLayer::contourOptions(const Spine::Parameter& param,
                                                      const Fmi::DateTime& valid_time,
                                                      const std::vector<double>& isovalues,
                                                      const Fmi::Box& box) const
{
  Engine::Contour::Options options(param, valid_time, isovalues);
  options.level = paraminfo.level;

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

  if (multiplier || offset)
    options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

  options.filter_size = smoother.size;
  options.filter_degree = smoother.degree;
  options.smoother = smoother.trax_options;

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
  options.subdivide = subdivide;
  options.threads = contour_threads;

  return options;
}

std::vector<OGRGeometryPtr> IsolineLayer::getIsolinesQuerydata(const std::vector<double>& isovalues,
                                                               const State& theState)
{
  // Establish the data. Store to member variable for IsolabelLayer use
  q = getModel(theState);

  if (q && !(q->isGrid()))
    throw Fmi::Exception(BCP, "Isoline-layer can't use point data!");

  // Establish the desired direction parameter. When the layer is
  // configured as a TFP metaparameter, fetch the underlying scalar
  // field instead and we'll compute TFP on the result further down.

  if (paraminfo.parameter.empty())
    throw Fmi::Exception(BCP, "Parameter not set for isoline-layer");

  const bool tfp_mode = tfp && ComputedFields::isTfpParameter(paraminfo.parameter);
  const std::string& fetch_name = tfp_mode ? tfp->field : paraminfo.parameter;
  auto param = TS::ParameterFactory::instance().parse(fetch_name);

  // Establish the valid time

  auto valid_time = getValidTime();

  // Establish the level

  selectLevel();

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
  getMapShapes(gis, crs, clipbox, inshape, outshape);

  // Logical operations with isobands are initialized before hand
  intersections.init(paraminfo.producer, projection, valid_time, theState);

  // Calculate the isolines and store them into the template engine

  // TODO(mheiskan): Clean up Options API to use optionals
  const auto& contourer = theState.getContourEngine();

  Engine::Contour::Options options = contourOptions(param, valid_time, isovalues, box);

  // Do the actual contouring, either full grid or just
  // a sampled section

  std::size_t qhash = Engine::Querydata::hash_value(q);
  auto valueshash = qhash;
  Fmi::hash_combine(valueshash, options.data_hash_value());

  const auto& qEngine = theState.getQEngine();
  auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

  // Avoid reprojecting data when sampling has been used for better speed (and accuracy)
  CoordinatesPtr coords =
      (sampleresolution ? qEngine.getWorldCoordinates(q) : qEngine.getWorldCoordinates(q, crs));

  // When TFP mode is active, replace the fetched underlying field with
  // its Thermal Front Parameter derivative before contouring. The contour
  // cache key gets salted with the TFP options so different TFP configs
  // don't share cache entries.
  if (tfp_mode && matrix)
    ComputedFields::applyTfp(matrix, qhash, q, qEngine, *tfp);

  // Gate bilinear subdivision on output pixel density: if the projected data
  // cells are sub-pixel the interior samples cannot possibly be visible, so
  // skip them. See SubdivideGate.h for details.
  if (coords)
    options.subdivide = effective_subdivide(subdivide, subdivide_min_cell_pixels, *coords, box);

  auto geoms = contourer.contour(qhash, crs, *matrix, *coords, clipbox, options);

  return geoms;
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a single isoline to the output
 */
// ----------------------------------------------------------------------

void IsolineLayer::addIsoline(CTPP::CDT& theGlobals,
                              CTPP::CDT& object_cdt,
                              CTPP::CDT& group_cdt,
                              State& theState,
                              const OGRGeometry& geom,
                              const Isoline& isoline,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box,
                              bool topojson)
{
  // Store the path with unique QID
  std::string iri = qid + (qid.empty() ? "" : ".") + isoline.getQid(theState);

  if (!theState.addId(iri) && !theState.animation_enabled)
    throw Fmi::Exception(BCP, "Non-unique ID assigned to isoline").addParameter("ID", iri);

  CTPP::CDT isoline_cdt(CTPP::CDT::HASH_VAL);
  isoline_cdt["iri"] = iri;
  isoline_cdt["time"] = Fmi::to_iso_extended_string(getValidTime());
  isoline_cdt["parameter"] = paraminfo.parameter;
  isoline_cdt["type"] = Geometry::name(geom, theState.getType());
  isoline_cdt["layertype"] = "isoline";

  std::string arcNumbers;
  std::string arcCoordinates;
  std::string pointCoordinates;

  // Use Bezier SVG for SVG output when enabled
  if (filter.bezierEnabled() && !topojson && theState.getType() != "geojson" &&
      theState.getType() != "kml")
  {
    pointCoordinates = filter.toBezierSvg(geom, box, precision, &theState.getBezierCache());
  }

  if (pointCoordinates.empty())
  {
    pointCoordinates = Geometry::toString(geom,
                                          theState.getType(),
                                          box,
                                          crs,
                                          precision,
                                          theState.arcHashMap,
                                          theState.arcCounter,
                                          arcNumbers,
                                          arcCoordinates);
  }

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
    std::string objectKey = "isoline:" + paraminfo.parameter + ":" + qid;
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
      const OGRGeometryPtr& geom = geoms[i];

      if (geom && geom->IsEmpty() == 0)
        addIsoline(
            theGlobals, object_cdt, group_cdt, theState, *geom, isolines[i], crs, box, topojson);
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
  if (theState.isObservation(paraminfo.producer))
    return;
  if (!paraminfo.parameter.empty())
  {
    ParameterInfo info(paraminfo.parameter);
    info.producer = paraminfo.producer;
    info.level = paraminfo.level;
    add(infos, info);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo
 */
// ----------------------------------------------------------------------

void IsolineLayer::getFeatureInfo(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    // Sanity checks
    if (!validLayer(theState))
      return;
    if (paraminfo.parameter.empty())
      return;

    Layer::getFeatureValue(theInfo, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetFeatureInfo operation failed!")
        .addParameter("qid", qid)
        .addParameter("Producer", *paraminfo.producer)
        .addParameter("Parameter", paraminfo.parameter);
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

    if (!(paraminfo.source == std::string("grid")))
      Fmi::hash_combine(hash, getModelHashValueOrEmpty(theState));

    Fmi::hash_combine(hash, countParameterHash(theState, paraminfo.parameter));
    Fmi::hash_combine(hash, Dali::hash_value(isolines, theState));
    Fmi::hash_combine(hash, Dali::hash_value(smoother, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(interpolation));
    Fmi::hash_combine(hash, Fmi::hash_value(extrapolation));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Fmi::hash_value(minarea));
    Fmi::hash_combine(hash, Dali::hash_value(outside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(inside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(sampling, theState));
    Fmi::hash_combine(hash, Dali::hash_value(intersections, theState));
    Fmi::hash_combine(hash, filter.hash_value());
    if (tfp)
      ComputedFields::hashTfpOptions(hash, *tfp);
    Fmi::hash_combine(hash, Fmi::hash_value(strict));
    Fmi::hash_combine(hash, Fmi::hash_value(validate));
    Fmi::hash_combine(hash, Fmi::hash_value(desliver));
    Fmi::hash_combine(hash, Fmi::hash_value(subdivide));
    Fmi::hash_combine(hash, Fmi::hash_value(subdivide_min_cell_pixels));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string IsolineLayer::generateGeoTiff(State& theState)
{
  try
  {
    return gridDataGeoTiff(*this, paraminfo.parameter, "linear", theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "IsolineLayer::generateGeoTiff failed!");
  }
}

std::string IsolineLayer::generateDataTile(State& theState)
{
  try
  {
    return gridDataTile(*this, paraminfo.parameter, "linear", theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "IsolineLayer::generateDataTile failed!");
  }
}

void IsolineLayer::addMVTLayer(MVTTileBuilder& theBuilder, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;
    if (paraminfo.parameter.empty())
      return;
    if (isolines.empty())
      return;

    std::vector<double> isovalues;
    isovalues.reserve(isolines.size());
    for (const Isoline& isoline : isolines)
      isovalues.push_back(isoline.value);

    auto geoms = getIsolines(isovalues, theState);

    const auto& box = projection.getBox();
    filter.bbox(box);
    filter.apply(geoms, false);

    const std::string layerName = qid.empty() ? paraminfo.parameter : qid;
    auto& mvtLayer = theBuilder.layer(layerName);

    for (size_t i = 0; i < geoms.size() && i < isolines.size(); i++)
    {
      const OGRGeometryPtr& geom = geoms[i];
      if (!geom || geom->IsEmpty())
        continue;

      const Isoline& isoline = isolines[i];
      std::vector<std::pair<std::string, MVTValue>> attrs;
      attrs.emplace_back("parameter", paraminfo.parameter);
      attrs.emplace_back("value", isoline.value);
      const auto cls = isoline.attributes.value("class");
      if (!cls.empty())
        attrs.emplace_back("class", cls);

      mvtLayer.addFeature(*geom, attrs);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "IsolineLayer::addMVTLayer failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
