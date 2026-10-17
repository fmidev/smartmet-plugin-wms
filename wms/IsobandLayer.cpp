//======================================================================

#include "IsobandLayer.h"
#include "AggregationUtility.h"
#include "Config.h"
#include "DataTile.h"
#include "Geometry.h"
#include "GridDataGeoTiff.h"
#include "GridQueryTools.h"
#include "Hash.h"
#include "Isoband.h"
#include "JsonTools.h"
#include "Layer.h"
#include "MapboxVectorTile.h"
#include "State.h"
#include "StyleSheet.h"
#include "SubdivideGate.h"
#include "ValueTools.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/contour/Engine.h>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/observation/Settings.h>
#include <engines/querydata/Model.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiQueryData.h>
#include <newbase/NFmiQueryDataUtil.h>
#include <newbase/NFmiTimeList.h>
#include <spine/Convenience.h>
#include <spine/Json.h>
#include <spine/Parameter.h>
#include <timeseries/ParameterFactory.h>
#include <timeseries/ParameterTools.h>
#include <trax/InterpolationType.h>
#include <limits>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{

std::string format_auto(const Isoband& isoband, const std::string& pattern)
{
  if (isoband.lolimit)
  {
    if (isoband.hilimit)
      return fmt::format(fmt::runtime(pattern), *isoband.lolimit, *isoband.hilimit);
    return fmt::format(fmt::runtime(pattern), *isoband.lolimit, "inf");
  }

  if (isoband.hilimit)
    return fmt::format(fmt::runtime(pattern), "-inf", *isoband.hilimit);

  return fmt::format(fmt::runtime(pattern), "nan", "nan");
}

void apply_autoqid(std::vector<Isoband>& isobands, const std::string& pattern)
{
  for (auto& isoband : isobands)
  {
    if (!pattern.empty() && isoband.qid.empty())
      isoband.qid = format_auto(isoband, pattern);

    boost::replace_all(isoband.qid, ".", ",");  // replace decimal dots with ,
  }
}

// Generate a class attribute for those missing one
void apply_autoclass(std::vector<Isoband>& isobands, const std::string& pattern)
{
  if (pattern.empty())
    return;

  for (auto& isoband : isobands)
  {
    if (isoband.attributes.value("class").empty())
    {
      auto name = format_auto(isoband, pattern);
      boost::replace_all(name, ".", ",");  // replace decimal dots with ,
      isoband.attributes.add("class", name);
    }
  }
}

// Contour limits of the isobands, missing limits are replaced by huge values
void contour_limits(const std::vector<Isoband>& isobands,
                    T::ParamValue_vec& contourLowValues,
                    T::ParamValue_vec& contourHighValues)
{
  for (const Isoband& isoband : isobands)
  {
    if (isoband.lolimit)
      contourLowValues.push_back(*isoband.lolimit);
    else
      contourLowValues.push_back(-1000000000.0);

    if (isoband.hilimit)
      contourHighValues.push_back(*isoband.hilimit);
    else
      contourHighValues.push_back(1000000000);
  }
}

// MVT feature attributes of an isoband
std::vector<std::pair<std::string, MVTValue>> mvt_attributes(const std::string& parameter,
                                                             const Isoband& isoband)
{
  std::vector<std::pair<std::string, MVTValue>> attrs;
  attrs.emplace_back("parameter", parameter);
  if (isoband.lolimit)
    attrs.emplace_back("lolimit", *isoband.lolimit);
  if (isoband.hilimit)
    attrs.emplace_back("hilimit", *isoband.hilimit);
  const auto cls = isoband.attributes.value("class");
  if (!cls.empty())
    attrs.emplace_back("class", cls);
  return attrs;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void IsobandLayer::init(Json::Value& theJson,
                        const State& theState,
                        const Config& theConfig,
                        const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);
    precision = theState.getPrecision("isoband");

    if (theState.getType() == "topojson")
      precision = theState.getPrecision("topojson");

    // Extract member values

    JsonTools::remove_string(paraminfo.parameter, theJson, "parameter");

    auto json = JsonTools::remove(theJson, "isobands");
    if (!json.isNull())
      JsonTools::extract_array("isobands", isobands, json, theConfig);

    std::string autoqid;
    JsonTools::remove_string(autoqid, theJson, "autoqid");
    apply_autoqid(isobands, autoqid);

    std::string autoclass;
    JsonTools::remove_string(autoclass, theJson, "autoclass");
    apply_autoclass(isobands, autoclass);

    JsonTools::remove_string(interpolation, theJson, "interpolation");
    JsonTools::remove_int(extrapolation, theJson, "extrapolation");

    json = JsonTools::remove(theJson, "smoother");
    smoother.init(json, theConfig);

    JsonTools::remove_double(precision, theJson, "precision");
    JsonTools::remove_double(minarea, theJson, "minarea");
    JsonTools::remove_string(areaunit, theJson, "areaunit");

    JsonTools::remove_bool(closed_range, theJson, "closed_range");
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

    json = JsonTools::remove(theJson, "filter");
    filter.init(json);

    json = JsonTools::remove(theJson, "intersect");
    intersections.init(json, theConfig);

    json = JsonTools::remove(theJson, "heatmap");
    heatmap.init(json, theConfig);

    json = JsonTools::remove(theJson, "tfp");
    if (!json.isNull())
      tfp = ComputedFields::parseTfpOptions(json);

    if (areaunit != "km^2" && areaunit != "px^2")
      throw Fmi::Exception(BCP, "Unknown areaunit '" + areaunit + '"');
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

// Add the observation points inside the image to the heatmap
void IsobandLayer::addHeatmapPoints(heatmap_t* hm,
                                    const heatmap_stamp_t* hms,
                                    const TS::TimeSeriesVector& values,
                                    std::size_t nrows,
                                    const Fmi::SpatialReference& crs,
                                    const NFmiArea& area,
                                    const Fmi::Box& box,
                                    NFmiGrid& grid) const
{
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
    if (crs.isGeographic() == 0)
    {
      auto xy = area.LatLonToWorldXY(NFmiPoint(x, y));
      x = xy.X();
      y = xy.Y();
    }

    // To pixel coordinate
    box.transform(x, y);

    // Skip if not inside desired area
    if (Properties::inside(box, x, y))
    {
      NFmiPoint gridPoint = grid.LatLonToGrid(lon, lat);
      heatmap_add_point_with_stamp(hm, lround(gridPoint.X()), lround(gridPoint.Y()), hms);
    }
  }
}

std::shared_ptr<Engine::Querydata::QImpl> IsobandLayer::buildHeatmap(
    const Spine::Parameter& theParameter, const Fmi::DateTime& theTime, const State& theState)
{
  try
  {
    if (!isFlashOrMobileProducer(*paraminfo.producer))
      throw Fmi::Exception(BCP, "Heatmap requires flash or mobile data!");

    auto valid_time_period = getValidTimePeriod();
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    Engine::Observation::Settings settings;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();
    settings.starttimeGiven = true;
    settings.stationtype = *paraminfo.producer;
    settings.timezone = "UTC";

    // Get actual data (flash coordinates plus parameter column values)
    auto& obsengine = theState.getObsEngine();
    settings.parameters.push_back(TS::makeParameter("longitude"));
    settings.parameters.push_back(TS::makeParameter("latitude"));
    settings.parameters.push_back(TS::makeParameter(paraminfo.parameter));

    settings.boundingBox = getClipBoundingBox(box, crs);

    auto result = obsengine.values(settings);

    // Establish new projection and the required grid size of the desired resolution

    std::unique_ptr<NFmiArea> newarea(NFmiArea::CreateFromBBox(
        crs, NFmiPoint(box.xmin(), box.ymin()), NFmiPoint(box.xmax(), box.ymax())));

    double datawidth = std::abs(newarea->WorldXYWidth());  // in native units
    double dataheight = std::abs(newarea->WorldXYHeight());

    // The test against 360 is due to legacy NFmiLatLonArea, which is sort of geographic
    // but WorldXYWidth returns equidistant cylindrical width instead. This kludge effectively
    // breaks metric projection heatmaps for areas smaller than 360 meters, but such analysis
    // is highly unlikely to happen.

    if (newarea->SpatialReference().isGeographic() && datawidth < 360)
    {
      datawidth *= kRearth * kPii / 180 / 1000;  // degrees to kilometers
      dataheight *= kRearth * kPii / 180 / 1000;
    }
    else
    {
      datawidth /= 1000;  // meters to kilometers
      dataheight /= 1000;
    }

    // Compute the grid size in floating point first so that a very small resolution
    // cannot overflow the unsigned int width/height (which would both truncate the
    // dimensions and wrap the width*height product, bypassing the max_points cap).
    const double dwidth = lround(datawidth / *heatmap.resolution);
    const double dheight = lround(dataheight / *heatmap.resolution);

    if (dwidth < 1 || dheight < 1 ||
        dwidth * dheight > static_cast<double>(heatmap.max_points))
      throw Fmi::Exception(
          BCP,
          (std::string("Heatmap too big (") + Fmi::to_string(dwidth * dheight) + " points, max " +
           Fmi::to_string(heatmap.max_points) + "), increase resolution"));

    auto width = static_cast<unsigned int>(dwidth);
    auto height = static_cast<unsigned int>(dheight);

    // Must use at least two grid points, value 1 would cause a segmentation fault in here

    width = std::max(width, 2U);
    height = std::max(height, 2U);

    NFmiGrid grid(newarea.get(), width, height);
    std::unique_ptr<heatmap_t, void (*)(heatmap_t*)> hm(nullptr, heatmap_free);
    unsigned radius = 0;

    if (result)
    {
      const auto& values = *result;

      if (!values.empty())
      {
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

        addHeatmapPoints(hm.get(), hms.get(), values, nrows, crs, *newarea, box, grid);
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
    std::shared_ptr<NFmiQueryData> data(NFmiQueryDataUtil::CreateEmptyData(info));
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
    Fmi::hash_combine(hash, Fmi::hash_value(theParameter.originalName()));
    Fmi::hash_combine(hash, Fmi::hash_value(valid_time_period.begin()));
    Fmi::hash_combine(hash, Fmi::hash_value(valid_time_period.end()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.xmin()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.ymin()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.xmax()));
    Fmi::hash_combine(hash, Fmi::hash_value(box.ymax()));
    Fmi::hash_combine(hash, Dali::hash_value(heatmap, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(radius));
    Fmi::hash_combine(hash, crs.hashValue());

    auto model = Engine::Querydata::Model::create(data, hash);
    return std::make_shared<Engine::Querydata::QImpl>(model);
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

    if (isobands.empty())
      return;

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    if (paraminfo.source == std::string("grid"))
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!")
        .addParameter("qid", qid)
        .addParameter("Producer", *paraminfo.producer)
        .addParameter("Parameter", paraminfo.parameter);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch the inside and outside map shapes clipped to the clipping box
 */
// ----------------------------------------------------------------------

void IsobandLayer::getMapShapes(const Engine::Gis::Engine& gis,
                                const Fmi::SpatialReference& crs,
                                const Fmi::Box& clipbox,
                                OGRGeometryPtr& inshape,
                                OGRGeometryPtr& outshape) const
{
  if (inside)
  {
    inshape = gis.getShape(&crs, inside->options);
    if (!inshape)
      throw Fmi::Exception(BCP, "Received empty inside-shape from database!");

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
 * \brief Clip the isoband and do the requested intersections
 */
// ----------------------------------------------------------------------

OGRGeometryPtr IsobandLayer::clipIsoband(const OGRGeometry& geom,
                                         const Fmi::Box& clipbox,
                                         const OGRGeometryPtr& inshape,
                                         const OGRGeometryPtr& outshape) const
{
  OGRGeometryPtr geom1(geom.clone());
  Fmi::OGR::normalizeWindingOrder(geom1.get());
  OGRGeometryPtr geom2(Fmi::OGR::polyclip(*geom1, clipbox));

  // Do intersections if so requested

  if (geom2 && geom2->IsEmpty() == 0 && inshape)
    geom2.reset(geom2->Intersection(inshape.get()));

  if (geom2 && geom2->IsEmpty() == 0 && outshape)
    geom2.reset(geom2->Difference(outshape.get()));

  // Intersect with data too

  return intersections.intersect(geom2);
}

// ----------------------------------------------------------------------
/*!
 * \brief Add a single isoband to the output
 */
// ----------------------------------------------------------------------

void IsobandLayer::addIsoband(CTPP::CDT& theGlobals,
                              CTPP::CDT& object_cdt,
                              CTPP::CDT& group_cdt,
                              State& theState,
                              const OGRGeometry& geom,
                              const Isoband& isoband,
                              const Fmi::DateTime& valid_time,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box)
{
  // Store the path with unique ID
  std::string iri = qid + (qid.empty() ? "" : ".") + isoband.getQid(theState);

  if (!theState.addId(iri))
    throw Fmi::Exception(BCP, "Non-unique ID assigned to isoband").addParameter("ID", iri);

  std::string arcNumbers;
  std::string arcCoordinates;
  std::string pointCoordinates;

  CTPP::CDT isoband_cdt(CTPP::CDT::HASH_VAL);
  isoband_cdt["iri"] = iri;
  isoband_cdt["time"] = Fmi::to_iso_extended_string(valid_time);
  isoband_cdt["parameter"] = paraminfo.parameter;

  // Use Bezier SVG for SVG output when enabled
  if (filter.bezierEnabled() && theState.getType() != "topojson" &&
      theState.getType() != "geojson" && theState.getType() != "kml")
  {
    pointCoordinates =
        filter.toBezierSvg(geom, box, precision, &theState.getBezierCache());
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
    isoband_cdt["data"] = pointCoordinates;

  isoband_cdt["type"] = Geometry::name(geom, theState.getType());
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

  if (theState.getType() == "topojson")
  {
    if (!arcNumbers.empty())
      isoband_cdt["arcs"] = arcNumbers;

    if (!arcCoordinates.empty())
    {
      CTPP::CDT arc_cdt(CTPP::CDT::HASH_VAL);
      arc_cdt["data"] = arcCoordinates;
      theGlobals["arcs"][theState.insertCounter] = arc_cdt;
      theState.insertCounter++;
    }
    object_cdt["paths"][iri] = isoband_cdt;
  }
  else
  {
    theGlobals["paths"][iri] = isoband_cdt;
  }

  // theGlobals["paths"][iri] = isoband_cdt;

  // Add the SVG use element
  CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
  tag_cdt["start"] = "<use";
  tag_cdt["end"] = "/>";
  theState.addAttributes(theGlobals, tag_cdt, isoband.attributes);
  tag_cdt["attributes"]["xlink:href"] = "#" + iri;
  group_cdt["tags"].PushBack(tag_cdt);
}

void IsobandLayer::generate_gridEngine(CTPP::CDT& theGlobals,
                                       CTPP::CDT& theLayersCdt,
                                       State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    if (paraminfo.parameter.empty())
      throw Fmi::Exception(BCP, "Parameter not set for isoband-layer");

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Establish the parameter
    //
    // Heatmap does not use the parameter currently (only flash or mobile coordinates)
    // bool allowUnknownParam = (theState.isObservation(producer) &&
    //                          isFlashOrMobileProducer(*producer) && heatmap.resolution);

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*paraminfo.producer);

    // Establish the valid time
    auto valid_time = getValidTime();

    T::ParamValue_vec contourLowValues;
    T::ParamValue_vec contourHighValues;
    contour_limits(isobands, contourLowValues, contourHighValues);

    auto crs = projection.getCRS();
    const auto& box = projection.getBox();
    const auto clipbox = getClipBox(box);

    std::string wkt = *projection.crs;

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
        wkt = crs.WKT();

      // std::cout << wkt << "\n";

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

    for (auto& p : originalGridQuery->mQueryParameterList)
    {
      p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      p.mType = QueryServer::QueryParameter::Type::Isoband;
      p.mContourLowValues = contourLowValues;
      p.mContourHighValues = contourHighValues;

      set_query_parameter_selections(p, paraminfo);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    add_grid_size_attributes(*originalGridQuery, projection);

    add_projection_attributes(*originalGridQuery, projection);

    if (wkt == "data")
      add_data_bbox_attribute(*originalGridQuery, projection);

    add_contour_attributes(*originalGridQuery, smoother, subdivide, contour_threads);

    auto interpolation_type = parse_contour_interpolation(interpolation);
    if (!interpolation_type)
      throw Fmi::Exception(BCP, "Unknown isoband interpolation method '" + interpolation + "'!");
    originalGridQuery->mAttributeList.addAttribute("contour.interpolation.type",
                                                   Fmi::to_string((int)*interpolation_type));

    /*
    if (minarea)
    {
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

    // The Query object after the query execution.
    // query.print(std::cout,0,0);

    // Converting the returned WKB-isolines into OGRGeometry objects.

    std::vector<OGRGeometryPtr> geoms = contour_geometries(*query, minarea, areaunit, box, crs);

    // Convert filter pixel distance to metric distance for smoothing
    filter.bbox(box);

    // Smoothen the isobands
    filter.apply(geoms, true);

    // Extracting the projection information from the query result.

    update_projection_size(projection, *query);

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    update_data_projection(projection, *query);

    if (wkt == "data")
      return;

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape;
    OGRGeometryPtr outshape;
    getMapShapes(gis, crs, clipbox, inshape, outshape);

    // Logical operations with isobands are initialized before hand

    intersections.init(paraminfo.producer, gridEngine, projection, valid_time, theState);

    CTPP::CDT object_cdt;
    std::string objectKey = "isoband:" + paraminfo.parameter + ":" + qid;
    object_cdt["objectKey"] = objectKey;

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
      const OGRGeometryPtr& geom = geoms[i];
      if (geom && geom->IsEmpty() == 0)
      {
        OGRGeometryPtr geom2 = clipIsoband(*geom, clipbox, inshape, outshape);

        // Finally produce output if we still have something left
        if (geom2 && geom2->IsEmpty() == 0)
          addIsoband(
              theGlobals, object_cdt, group_cdt, theState, *geom2, isobands[i], valid_time, crs, box);
      }
    }

    theGlobals["bbox"] = Fmi::to_string(box.xmin()) + "," + Fmi::to_string(box.ymin()) + "," +
                         Fmi::to_string(box.xmax()) + "," + Fmi::to_string(box.ymax());
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -static_cast<int>(precision));

    theGlobals["objects"][objectKey] = object_cdt;

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
 * \brief Contouring options for the querydata engine
 */
// ----------------------------------------------------------------------

Engine::Contour::Options IsobandLayer::contourOptions(const Spine::Parameter& param,
                                                      const Fmi::DateTime& valid_time,
                                                      const Fmi::Box& box) const
{
  std::vector<Engine::Contour::Range> limits;
  limits.reserve(isobands.size());
  for (const Isoband& isoband : isobands)
    limits.emplace_back(isoband.lolimit, isoband.hilimit);

  Engine::Contour::Options options(param, valid_time, limits);
  options.level = paraminfo.level;
  options.bbox = Fmi::BBox(box);

  if (multiplier || offset)
    options.transformation(multiplier ? *multiplier : 1.0, offset ? *offset : 0.0);

  options.minarea = minarea;
  if (minarea)
  {
    if (areaunit == "px^2")
      options.minarea = box.areaFactor() * *minarea;
  }

  options.filter_size = smoother.size;
  options.filter_degree = smoother.degree;
  options.smoother = smoother.trax_options;

  options.extrapolation = extrapolation;

  auto interpolation_type = parse_contour_interpolation(interpolation);
  if (!interpolation_type)
    throw Fmi::Exception(BCP, "Unknown isoband interpolation method '" + interpolation + "'!");
  options.interpolation = *interpolation_type;

  options.closed_range = closed_range;
  options.strict = strict;
  options.validate = validate;
  options.desliver = desliver;
  options.subdivide = subdivide;
  options.threads = contour_threads;

  return options;
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the first level or the requested level of the data
 */
// ----------------------------------------------------------------------

void IsobandLayer::selectLevel(const Engine::Querydata::Q& q) const
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
 * \brief Sample the data to a higher resolution or build a heatmap if requested
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q IsobandLayer::sampleData(Engine::Querydata::Q q,
                                              const Spine::Parameter& param,
                                              const Fmi::DateTime& valid_time,
                                              const Fmi::SpatialReference& crs,
                                              const Fmi::Box& box,
                                              const std::optional<double>& sampleresolution,
                                              const State& theState)
{
  if (sampleresolution)
  {
    if (!q)
      throw Fmi::Exception(BCP, "Cannot resample without gridded data");

    if (heatmap.resolution)
      throw Fmi::Exception(BCP, "Isoband-layer can't use both sampling and heatmap!");

    std::unique_ptr<boost::timer::auto_cpu_timer> timer2;
    if (theState.useTimer())
    {
      std::string report2 = "IsobandLayer::resample finished in %t sec CPU, %w sec real\n";
      timer2 = std::make_unique<boost::timer::auto_cpu_timer>(2, report2);
    }

    q = q->sample(param,
                  valid_time,
                  crs,
                  box.xmin(),
                  box.ymin(),
                  box.xmax(),
                  box.ymax(),
                  *sampleresolution);
  }
  else if (heatmap.resolution)
  {
    q = buildHeatmap(param, valid_time, theState);
  }
  else if (theState.isObservation(paraminfo.producer))
    throw Fmi::Exception(
        BCP, "Can't produce isobandlayer from observation data without heatmap configuration!");
  return q;
}

void IsobandLayer::generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IsobandLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Establish the data
    auto q = getModel(theState);

    if (q && !(q->isGrid()))
      throw Fmi::Exception(BCP, "Isoband-layer can't use point data!");

    // Establish the parameter
    //
    // Heatmap does not use the parameter currently (only flash or mobile coordinates)
    bool allowUnknownParam = (theState.isObservation(paraminfo.producer) &&
                              isFlashOrMobileProducer(*paraminfo.producer) && heatmap.resolution);

    if (paraminfo.parameter.empty())
      throw Fmi::Exception(BCP, "Parameter not set for isoband-layer!");

    // When configured as a TFP metaparameter, fetch the underlying scalar
    // field instead and compute TFP on it below before contouring.
    const bool tfp_mode = tfp && ComputedFields::isTfpParameter(paraminfo.parameter);
    if (tfp_mode && heatmap.resolution)
      throw Fmi::Exception(BCP, "TFP cannot be combined with heatmap mode");
    const std::string& fetch_name = tfp_mode ? tfp->field : paraminfo.parameter;
    auto param = TS::ParameterFactory::instance().parse(fetch_name, allowUnknownParam);

    // Establish the valid time

    auto valid_time = getValidTime();

    // Establish the level

    selectLevel(q);

    // Get projection details

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // And the box needed for clipping
    const auto clipbox = getClipBox(box);

    // Sample to higher resolution if necessary

    auto sampleresolution = sampling.getResolution(projection);
    q = sampleData(q, param, valid_time, crs, box, sampleresolution, theState);

    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate isobands without gridded data");

    // Logical operations with maps require shapes

    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape;
    OGRGeometryPtr outshape;
    getMapShapes(gis, crs, clipbox, inshape, outshape);

    // Logical operations with isobands are initialized before hand

    intersections.init(paraminfo.producer, projection, valid_time, theState);

    // Calculate the isobands and store them into the template engine

    const auto& contourer = theState.getContourEngine();
    Engine::Contour::Options options = contourOptions(param, valid_time, box);

    // Do the actual contouring, either full grid or just
    // a sampled section

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    Fmi::hash_combine(valueshash, options.data_hash_value());

    // Select the data

    if (heatmap.resolution)
    {
      // Heatmap querydata has just 1 fixed parameter (1/pressure)
      options.parameter = TS::ParameterFactory::instance().parse("1");
    }

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

    // Avoid reprojecting data when sampling has been used for better speed (and accuracy)
    CoordinatesPtr coords =
        (sampleresolution ? qEngine.getWorldCoordinates(q) : qEngine.getWorldCoordinates(q, crs));

    // When TFP mode is active, replace the fetched underlying field with
    // its Thermal Front Parameter derivative before contouring. The
    // contour cache key gets salted with the TFP options so different
    // TFP configs don't share cache entries.
    if (tfp_mode && matrix)
      ComputedFields::applyTfp(matrix, qhash, q, qEngine, *tfp);

    // Gate bilinear subdivision on output pixel density: if the projected data
    // cells are sub-pixel the interior samples cannot possibly be visible, so
    // skip them. See SubdivideGate.h for details.
    if (coords)
      options.subdivide = effective_subdivide(subdivide, subdivide_min_cell_pixels, *coords, box);

    std::vector<OGRGeometryPtr> geoms =
        contourer.contour(qhash, crs, *matrix, *coords, clipbox, options);

    filter.bbox(box);
    filter.apply(geoms, true);

    CTPP::CDT object_cdt;
    std::string objectKey = "isoband:" + paraminfo.parameter + ":" + qid;
    object_cdt["objectKey"] = objectKey;

    // Update the globals

    // if (css)
    //{
    //  std::string name = theState.getCustomer() + "/" + *css;
    //  theGlobals["css"][name] = theState.getStyle(*css);
    // }

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
      OGRGeometryPtr& geom = geoms[i];

      if (geom && geom->IsEmpty() == 0)
      {
        OGRGeometryPtr geom2 = clipIsoband(*geom, clipbox, inshape, outshape);

        // Finally produce output if we still have something left
        if (geom2 && geom2->IsEmpty() == 0)
          addIsoband(
              theGlobals, object_cdt, group_cdt, theState, *geom2, isobands[i], valid_time, crs, box);
      }
    }
    theGlobals["objects"][objectKey] = object_cdt;
    theGlobals["bbox"] = Fmi::to_string(box.xmin()) + "," + Fmi::to_string(box.ymin()) + "," +
                         Fmi::to_string(box.xmax()) + "," + Fmi::to_string(box.ymax());
    if (precision >= 1.0)
      theGlobals["precision"] = pow(10.0, -static_cast<int>(precision));

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
 * \brief GetFeatureInfo
 */
// ----------------------------------------------------------------------

void IsobandLayer::getFeatureInfo(CTPP::CDT& theInfo, const State& theState)
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
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void IsobandLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
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
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t IsobandLayer::hash_value(const State& theState) const
{
  try
  {
    // if (paraminfo.source == std::string("grid"))
    // return Fmi::bad_hash;

    auto hash = Layer::hash_value(theState);

    if (!theState.isObservation(paraminfo.producer) && !(paraminfo.source == std::string("grid")))
      Fmi::hash_combine(hash, getModelHashValueOrEmpty(theState));

    Fmi::hash_combine(hash, countParameterHash(theState, paraminfo.parameter));
    Fmi::hash_combine(hash, Dali::hash_value(isobands, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(interpolation));
    Fmi::hash_combine(hash, Dali::hash_value(smoother, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(extrapolation));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));
    Fmi::hash_combine(hash, Fmi::hash_value(minarea));
    Fmi::hash_combine(hash, Fmi::hash_value(areaunit));
    Fmi::hash_combine(hash, Dali::hash_value(outside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(inside, theState));
    Fmi::hash_combine(hash, Dali::hash_value(sampling, theState));
    Fmi::hash_combine(hash, Dali::hash_value(intersections, theState));
    Fmi::hash_combine(hash, filter.hash_value());
    Fmi::hash_combine(hash, Dali::hash_value(heatmap, theState));
    if (tfp)
      ComputedFields::hashTfpOptions(hash, *tfp);
    Fmi::hash_combine(hash, Fmi::hash_value(closed_range));
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

std::string IsobandLayer::generateGeoTiff(State& theState)
{
  try
  {
    return gridDataGeoTiff(*this, paraminfo.parameter, "linear", theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "IsobandLayer::generateGeoTiff failed!");
  }
}

std::string IsobandLayer::generateDataTile(State& theState)
{
  try
  {
    return gridDataTile(*this, paraminfo.parameter, "linear", theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "IsobandLayer::generateDataTile failed!");
  }
}

void IsobandLayer::addMVTLayer(MVTTileBuilder& theBuilder, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;
    if (paraminfo.parameter.empty())
      return;
    if (isobands.empty())
      return;

    // Grid engine source is not yet supported for MVT output
    if (paraminfo.source == std::string("grid"))
      return;

    // Establish the data
    auto q = getModel(theState);

    if (q && !(q->isGrid()))
      throw Fmi::Exception(BCP, "IsobandLayer::addMVTLayer: can't use point data!");

    bool allowUnknownParam = (theState.isObservation(paraminfo.producer) &&
                              isFlashOrMobileProducer(*paraminfo.producer) && heatmap.resolution);

    auto param = TS::ParameterFactory::instance().parse(paraminfo.parameter, allowUnknownParam);
    auto valid_time = getValidTime();

    selectLevel(q);

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();
    const auto clipbox = getClipBox(box);

    // Sampling support
    auto sampleresolution = sampling.getResolution(projection);
    if (sampleresolution)
    {
      if (!q)
        throw Fmi::Exception(BCP, "Cannot resample without gridded data");
      q = q->sample(param,
                    valid_time,
                    crs,
                    box.xmin(),
                    box.ymin(),
                    box.xmax(),
                    box.ymax(),
                    *sampleresolution);
    }
    else if (heatmap.resolution)
    {
      q = buildHeatmap(param, valid_time, theState);
    }
    else if (theState.isObservation(paraminfo.producer))
      throw Fmi::Exception(
          BCP, "Can't produce isobandlayer from observation data without heatmap configuration!");

    if (!q)
      throw Fmi::Exception(BCP, "Cannot generate isobands without gridded data");

    // Logical operations with maps require shapes
    const auto& gis = theState.getGisEngine();

    OGRGeometryPtr inshape;
    OGRGeometryPtr outshape;
    getMapShapes(gis, crs, clipbox, inshape, outshape);

    intersections.init(paraminfo.producer, projection, valid_time, theState);

    // Build contour options
    const auto& contourer = theState.getContourEngine();
    Engine::Contour::Options options = contourOptions(param, valid_time, box);

    std::size_t qhash = Engine::Querydata::hash_value(q);
    auto valueshash = qhash;
    Fmi::hash_combine(valueshash, options.data_hash_value());

    const auto& qEngine = theState.getQEngine();
    auto matrix = qEngine.getValues(q, options.parameter, valueshash, options.time);

    CoordinatesPtr coords =
        (sampleresolution ? qEngine.getWorldCoordinates(q) : qEngine.getWorldCoordinates(q, crs));

    if (coords)
      options.subdivide = effective_subdivide(subdivide, subdivide_min_cell_pixels, *coords, box);

    std::vector<OGRGeometryPtr> geoms =
        contourer.contour(qhash, crs, *matrix, *coords, clipbox, options);

    filter.bbox(box);
    filter.apply(geoms, true);

    const std::string layerName = qid.empty() ? paraminfo.parameter : qid;
    auto& mvtLayer = theBuilder.layer(layerName);

    for (size_t i = 0; i < geoms.size() && i < isobands.size(); i++)
    {
      OGRGeometryPtr& geom = geoms[i];
      if (!geom || geom->IsEmpty())
        continue;

      OGRGeometryPtr geom2 = clipIsoband(*geom, clipbox, inshape, outshape);

      if (!geom2 || geom2->IsEmpty())
        continue;

      mvtLayer.addFeature(*geom2, mvt_attributes(paraminfo.parameter, isobands[i]));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "IsobandLayer::addMVTLayer failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
