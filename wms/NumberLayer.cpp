//======================================================================

#include "NumberLayer.h"
#include "AggregationUtility.h"
#include "Config.h"
#include "DataTile.h"
#include "GridDataGeoTiff.h"
#include "GridQueryTools.h"
#include "Hash.h"
#include "Iri.h"
#include "JsonTools.h"
#include "Layer.h"
#include "MapboxVectorTile.h"
#include "ObservationReader.h"
#include "PointData.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <spine/Convenience.h>
#include <spine/Json.h>
#include <timeseries/ParameterTools.h>
#include <timeseries/TimeSeriesGeneratorCache.h>
#include <timeseries/TimeSeriesOutput.h>
#include <iomanip>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{

using PointValues = std::vector<PointData>;

// ----------------------------------------------------------------------
/*!
 * \brief Forecast reader
 */
// ----------------------------------------------------------------------

PointValues read_forecasts(const NumberLayer& layer,
                           const Engine::Querydata::Q& q,
                           const Fmi::SpatialReference& crs,
                           const Fmi::Box& box,
                           const Fmi::DateTime& valid_time,
                           const State& state)
{
  try
  {
    // Generate the coordinates for the numbers

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode, state);

    // The parameters. This *must* be done after the call to positions generation

    std::optional<Spine::Parameter> param;
    if (layer.param_funcs)
      param = layer.param_funcs->parameter;

    std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::LocalDateTime localdatetime(valid_time, Fmi::TimeZonePtr::utc);

    PointValues pointvalues;

    const auto& mylocale = std::locale::classic();
    NFmiPoint dummy;

    for (const auto& point : points)
    {
      if (layer.inside(box, point.x, point.y))
      {
        Spine::Location loc(point.latlon.X(), point.latlon.Y());

        // Q API SUCKS!!
        Engine::Querydata::ParameterOptions options(
            *param, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, 0, dummy);

        TS::Value result =
            AggregationUtility::get_qengine_value(q, options, localdatetime, layer.param_funcs);

        if (const double* tmp = std::get_if<double>(&result))
        {
          pointvalues.emplace_back(point, *tmp);
          // printf("Point %d,%d  => %f,%f  = %f\n",point.x,point.y,point.latlon.X(),
          // point.latlon.Y(),*tmp);
        }
        else if (const int* ptr = std::get_if<int>(&result))
        {
          double tmp = *ptr;
          pointvalues.emplace_back(point, tmp);
          // printf("Point %d,%d  => %f,%f  = %f\n",point.x,point.y,point.latlon.X(),
          // point.latlon.Y(),tmp);
        }
        else
        {
          PointData missingvalue{point, kFloatMissing};
          pointvalues.push_back(missingvalue);
        }
      }
      else
      {
        // printf("*** Outside %d,%d  => %f,%f\n",point.x,point.y,point.latlon.X(),
        // point.latlon.Y());
      }
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Grid Forecast reader
 */
// ----------------------------------------------------------------------

PointValues read_gridForecasts(const NumberLayer& layer,
                               const Engine::Grid::Engine* /* gridEngine */,
                               const QueryServer::Query& query,
                               const Fmi::SpatialReference& crs,
                               const Fmi::Box& box,
                               const Fmi::DateTime& /* valid_time */,
                               const State& state)
{
  try
  {
    // Generate the coordinates for the symbols

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(nullptr, crs, box, forecast_mode, state);

    PointValues pointvalues;
    T::GridValueList* values = nullptr;
    for (const auto& param : query.mQueryParameterList)
    {
      for (const auto& val : param.mValueList)
      {
        if (val->mValueList.getLength() == points.size())
          values = &val->mValueList;
      }
    }

    if (values && values->getLength())
    {
      uint len = values->getLength();
      for (uint t = 0; t < len; t++)
      {
        T::GridValue* rec = values->getGridValuePtrByIndex(t);
        auto point = points[t];

        if (rec->mValue != ParamValueMissing)
          pointvalues.emplace_back(point, rec->mValue);
        else
          pointvalues.emplace_back(point, kFloatMissing);
      }
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Symbol translation and scaling
std::string symbol_transform(int x,
                             int y,
                             const std::optional<double>& scale,
                             const std::optional<double>& rescale)
{
  std::string tmp = fmt::sprintf("translate(%d,%d)", x, y);

  double newscale = (scale ? *scale : 1.0) * (rescale ? *rescale : 1.0);
  if (newscale != 1.0)
    tmp += fmt::sprintf(" scale(%g)", newscale);
  return tmp;
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Add the symbols of the numbers into the group
 */
// ----------------------------------------------------------------------

void NumberLayer::addSymbols(CTPP::CDT& theGlobals,
                             CTPP::CDT& group_cdt,
                             State& theState,
                             const PointValues& pointvalues,
                             double xmultiplier,
                             double xoffset) const
{
  for (const auto& pointvalue : pointvalues)
  {
    // Start generating the hash

    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";

    const auto& point = pointvalue.point();
    float value = pointvalue[0];

    if (value != kFloatMissing)
      value = xmultiplier * value + xoffset;

    std::string iri;
    if (symbol)
      iri = *symbol;

    auto selection = Select::attribute(numbers, value);
    if (selection && selection->symbol)
      iri = *selection->symbol;

    // librsvg cannot handle scale + transform, must move former into latter
    std::optional<double> rescale;
    if (selection)
    {
      auto scaleattr = selection->attributes.remove("scale");
      if (scaleattr)
        rescale = Fmi::stod(*scaleattr);
    }

    if (iri.empty())
      continue;

    std::string IRI = Iri::normalize(iri);
    if (theState.addId(IRI))
      theGlobals["includes"][iri] = theState.getSymbol(iri);

    // Lack of CSS3 transform support forces us to use a direct transformation
    // which may override user settings
    tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

    tag_cdt["attributes"]["transform"] = symbol_transform(point.x, point.y, scale, rescale);

    group_cdt["tags"].PushBack(tag_cdt);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the numbers as text layers, returns the number of valid values
 */
// ----------------------------------------------------------------------

int NumberLayer::addNumbers(CTPP::CDT& theGlobals,
                            CTPP::CDT& theLayersCdt,
                            State& theState,
                            const PointValues& pointvalues,
                            double xmultiplier,
                            double xoffset) const
{
  int valid_count = 0;
  for (const auto& pointvalue : pointvalues)
  {
    const auto& point = pointvalue.point();
    float value = pointvalue[0];

    if (value != kFloatMissing)
    {
      ++valid_count;
      value = xmultiplier * value + xoffset;
    }

    std::string txt = label.print(value);

    if (!txt.empty())
    {
      // Generate a text tag
      CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
      text_cdt["start"] = "<text";
      text_cdt["end"] = "</text>";
      text_cdt["cdata"] = Fmi::safexmlescape(txt);

      auto selection = Select::attribute(numbers, value);

      if (selection)
      {
        selection->attributes.remove("scale");
        theState.addAttributes(theGlobals, text_cdt, selection->attributes);
      }

      auto xpos = lround(point.x + point.dx + label.dx);
      auto ypos = lround(point.y + point.dy + label.dy);

      text_cdt["attributes"]["x"] = Fmi::to_string(xpos);
      text_cdt["attributes"]["y"] = Fmi::to_string(ypos);
      theLayersCdt.PushBack(text_cdt);
    }
  }
  return valid_count;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set the point locations or the geometry to the grid query
 */
// ----------------------------------------------------------------------

void NumberLayer::setQueryLocations(QueryServer::Query& query, const State& theState)
{
  if (positions)
  {
    const bool forecast_mode = true;
    const auto& box = projection.getBox();
    auto points =
        positions->getPoints(nullptr, projection.getCRS(), box, forecast_mode, theState);

    T::Coordinate_vec coordinates;
    for (const auto& point : points)
      coordinates.emplace_back(point.latlon.X(), point.latlon.Y());

    query.mAreaCoordinates.push_back(coordinates);
    query.mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;
  }

  for (auto& queryparam : query.mQueryParameterList)
  {
    if (positions)
    {
      queryparam.mLocationType = QueryServer::QueryParameter::LocationType::Point;
      queryparam.mType = QueryServer::QueryParameter::Type::PointValues;
    }
    else
    {
      queryparam.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      queryparam.mType = QueryServer::QueryParameter::Type::Vector;
      queryparam.mFlags = QueryServer::QueryParameter::Flags::NoReturnValues;
    }

    set_query_parameter_selections(queryparam, paraminfo);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void NumberLayer::init(Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Number-layer JSON is not a JSON object");

    // Must be extracted before Layer::init which accepts only a single parameter name
    sources.init(theJson);

    // Iterate through all the members

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(paraminfo.parameter, theJson, "parameter");
    if (!sources.empty())
      paraminfo.parameter = sources.sources.front().parameter;
    if (!paraminfo.parameter.empty() && paraminfo.producer &&
        !isFlashOrMobileProducer(*paraminfo.producer))
    {
      try
      {
        param_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(paraminfo.parameter);
      }
      catch (...)
      {
      }
    }

    auto json = JsonTools::remove(theJson, "positions");
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }

    JsonTools::remove_int(minvalues, theJson, "minvalues");

    json = JsonTools::remove(theJson, "label");
    label.init(json, theConfig);

    JsonTools::remove_string(symbol, theJson, "symbol");
    JsonTools::remove_double(scale, theJson, "scale");

    json = JsonTools::remove(theJson, "numbers");
    if (!json.isNull())
      JsonTools::extract_array("numbers", numbers, json, theConfig);

    point_value_options.init(theJson);

    if (paraminfo.parameter.empty())
      throw Fmi::Exception(BCP, "NumberLayer parameter is not set");
    if (!paraminfo.producer)
      throw Fmi::Exception(BCP, "NumberLayer producer is not set");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void NumberLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // if (!validLayer(theState))
    //      return;

    if (!theState.isObservation(paraminfo.producer))
    {
      if (!sources.empty())
        throw Fmi::Exception(BCP, "Parameter lists are supported only for observations");
      if (!point_value_options.conditional_priorities.empty())
        throw Fmi::Exception(BCP, "Conditional priorities are supported only for observations");
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

void NumberLayer::generate_gridEngine(CTPP::CDT& theGlobals,
                                      CTPP::CDT& theLayersCdt,
                                      State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    // Time execution

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "NumberLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Make sure position generation is initialized

    if (!positions)
      positions = Positions{};

    // Add layer margins to position generation
    positions->addMargins(xmargin, ymargin);

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*paraminfo.producer);

    auto valid_time = getValidTime();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time;

    std::string wkt = *projection.crs;
    // std::cout << wkt << "\n";

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
        wkt = projection.getCRS().WKT();

      // std::cout << wkt << "\n";

      // Adding the bounding box information into the query.

      const auto& box = projection.getBox();
      const auto clipbox = getClipBox(box);

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      auto bbox = fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      bbox = fmt::format(
          "{},{},{},{}", clipbox.xmin(), clipbox.ymin(), clipbox.xmax(), clipbox.ymax());
      // bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);

      positions->init(paraminfo.producer, projection, valid_time, theState);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means
      // that we we do not know the actual projection yet and we have to wait that the grid-engine
      // delivers us the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    std::string pName = paraminfo.parameter;
    auto pos = pName.find(".raw");
    if (pos != std::string::npos)
    {
      attributeList.addAttribute("grid.areaInterpolationMethod",
                                 Fmi::to_string(T::AreaInterpolationMethod::Nearest));
      pName.erase(pos, 4);
    }

    std::string param = gridEngine->getParameterString(producerName, pName);
    attributeList.addAttribute("param", param);

    if (!projection.projectionParameter)
      projection.projectionParameter = param;

    if (param == paraminfo.parameter && originalGridQuery->mProducerNameList.empty())
    {
      gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
      if (originalGridQuery->mProducerNameList.empty())
        originalGridQuery->mProducerNameList.push_back(producerName);
    }

    add_time_attributes(attributeList, getValidTime(), origintime);

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery, attributeList);

    // Fullfilling information into the query object.

    setQueryLocations(*originalGridQuery, theState);

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    add_grid_size_attributes(*originalGridQuery, projection);

    if (wkt == "data")
      add_data_bbox_attribute(*originalGridQuery, projection);

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    // query.print(std::cout,0,0);

    // Extracting the projection information from the query result.

    update_projection_size(projection, *query);

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    update_data_projection(projection, *query);

    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    if (wkt == "data")
      return;

    // Initialize inside/outside shapes and intersection isobands

    positions->init(paraminfo.producer, projection, valid_time, theState);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    PointValues pointvalues;
    pointvalues =
        read_gridForecasts(*this, gridEngine, *originalGridQuery, crs, box, valid_time, theState);

    pointvalues = prioritize(pointvalues, point_value_options);

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate numbers as text layers inside <g>..</g>
    // Tags do not work, they do not have cdata enabled in the
    // template

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Data conversion settings

    double xmultiplier = (multiplier ? *multiplier : 1.0);
    double xoffset = (offset ? *offset : 0.0);

    // Add attributes to the group, not the text tags
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Symbols first

    addSymbols(theGlobals, group_cdt, theState, pointvalues, xmultiplier, xoffset);
    theLayersCdt.PushBack(group_cdt);

    // Then numbers

    int valid_count =
        addNumbers(theGlobals, theLayersCdt, theState, pointvalues, xmultiplier, xoffset);

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in number layer")
          .addParameter("valid values", Fmi::to_string(valid_count))
          .addParameter("minimum count", Fmi::to_string(minvalues));

    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
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

void NumberLayer::generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // Time execution

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "NumberLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Establish the data

    bool use_observations = theState.isObservation(paraminfo.producer);
    auto q = getModel(theState);

    // Make sure position generation is initialized

    if (!positions)
    {
      positions = Positions{};
      if (use_observations)
        positions->layout =
            Positions::Layout::Data;  // default layout for observations is Data (bbox)
    }

    // Add layer margins to position generation
    positions->addMargins(xmargin, ymargin);

    // Establish the valid time

    auto valid_time = getValidTime();
    auto valid_time_period = getValidTimePeriod();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time;

    // Establish the level

    if (q && !q->firstLevel())
      throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

    if (paraminfo.level)
    {
      if (use_observations)
        throw std::runtime_error("Cannot set level value for observations in NumberLayer");

      if (!q->selectLevel(*paraminfo.level))
        throw Fmi::Exception(
            BCP, "Level value " + Fmi::to_string(*paraminfo.level) + " is not available!");
    }

    // Get projection details

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Initialize inside/outside shapes and intersection isobands

    positions->init(paraminfo.producer, projection, valid_time, theState);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    PointValues pointvalues;

    if (!use_observations)
      pointvalues = read_forecasts(*this, q, crs, box, valid_time, theState);
#ifndef WITHOUT_OBSERVATION
    else
    {
      // Conditional priorities need extra parameters after the actual one
      auto extra_parameters = point_value_options.extraParameters();
      if (!sources.empty())
      {
        pointvalues = ObservationReader::read(theState,
                                              sources,
                                              extra_parameters,
                                              *this,
                                              *positions,
                                              maxdistance_km,
                                              crs,
                                              box,
                                              valid_time,
                                              valid_time_period);
      }
      else
      {
        auto parameters = extra_parameters;
        parameters.insert(parameters.begin(), paraminfo.parameter);
        pointvalues = ObservationReader::read(theState,
                                              parameters,
                                              *this,
                                              *positions,
                                              maxdistance_km,
                                              crs,
                                              box,
                                              valid_time,
                                              valid_time_period);
      }
    }
#endif

    pointvalues = prioritize(pointvalues, point_value_options);

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate numbers as text layers inside <g>..</g>
    // Tags do not work, they do not have cdata enabled in the
    // template

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Add attributes to the group, not the text tags
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Data conversion settings

    double xmultiplier = (multiplier ? *multiplier : 1.0);
    double xoffset = (offset ? *offset : 0.0);

    // Symbols first

    addSymbols(theGlobals, group_cdt, theState, pointvalues, xmultiplier, xoffset);
    theLayersCdt.PushBack(group_cdt);

    // Then numbers

    int valid_count =
        addNumbers(theGlobals, theLayersCdt, theState, pointvalues, xmultiplier, xoffset);

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in number layer")
          .addParameter("valid values", Fmi::to_string(valid_count))
          .addParameter("minimum count", Fmi::to_string(minvalues));

    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate NumberLayer");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void NumberLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
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

void NumberLayer::getFeatureInfo(CTPP::CDT& theInfo, const State& theState)
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
 * \brief Generate MVT layer with point features (qEngine path only).
 */
// ----------------------------------------------------------------------

void NumberLayer::addMVTLayer(MVTTileBuilder& theBuilder, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;
    if (paraminfo.parameter.empty())
      return;

    // Skip observations — no qEngine model available for them
    if (theState.isObservation(paraminfo.producer))
      return;

    // gridEngine path not yet supported for MVT; fall back to qEngine data
    if (paraminfo.source == std::string("grid"))
      return;

    auto q = getModel(theState);
    if (!q)
      return;

    if (!positions)
      positions = Positions{};
    positions->addMargins(xmargin, ymargin);

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    const auto valid_time = getValidTime();
    positions->init(paraminfo.producer, projection, valid_time, theState);

    auto pointvalues = read_forecasts(*this, q, crs, box, valid_time, theState);
    pointvalues = prioritize(pointvalues, point_value_options);

    if (pointvalues.empty())
      return;

    Fmi::CoordinateTransformation xform("WGS84", crs);

    const std::string layerName = qid.empty() ? paraminfo.parameter : qid;
    auto& mvtLayer = theBuilder.layer(layerName);

    const double xmultiplier = (multiplier ? *multiplier : 1.0);
    const double xoffset = (offset ? *offset : 0.0);

    for (const auto& pointvalue : pointvalues)
    {
      auto value = pointvalue[0];
      if (value == kFloatMissing)
        continue;

      value = xmultiplier * value + xoffset;

      double x = pointvalue.point().latlon.X();
      double y = pointvalue.point().latlon.Y();
      if (!xform.transform(x, y))
        continue;

      OGRPoint pt(x, y);

      std::vector<std::pair<std::string, MVTValue>> attrs;
      attrs.emplace_back("parameter", paraminfo.parameter);
      attrs.emplace_back("value", value);
      attrs.emplace_back("text", label.print(static_cast<float>(value)));

      mvtLayer.addFeature(pt, attrs);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer::addMVTLayer failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t NumberLayer::hash_value(const State& theState) const
{
  try
  {
    // Disable caching of observation layers
    if (theState.isObservation(paraminfo.producer))
      return Fmi::bad_hash;

    auto hash = Layer::hash_value(theState);

    if (paraminfo.source != std::string("grid"))
    {
      auto model_hash = getModelHashValue(theState);
      if (model_hash)
        Fmi::hash_combine(hash, *model_hash);
    }

    Fmi::hash_combine(hash, countParameterHash(theState, paraminfo.parameter));
    Fmi::hash_combine(hash, Dali::hash_value(positions, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(minvalues));
    Fmi::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(scale));
    Fmi::hash_combine(hash, Dali::hash_value(label, theState));
    Fmi::hash_combine(hash, Dali::hash_value(numbers, theState));
    Fmi::hash_combine(hash, point_value_options.hash_value());
    Fmi::hash_combine(hash, sources.hash_value());
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Calculating hash_value for the layer failed!");
  }
}

std::string NumberLayer::generateGeoTiff(State& theState)
{
  try
  {
    return gridDataGeoTiff(*this, paraminfo.parameter, "linear", theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer::generateGeoTiff failed!");
  }
}

std::string NumberLayer::generateDataTile(State& theState)
{
  try
  {
    return gridDataTile(*this, paraminfo.parameter, "linear", theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer::generateDataTile failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
