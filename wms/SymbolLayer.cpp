//======================================================================
#include "SymbolLayer.h"
#include "AggregationUtility.h"
#include "Config.h"
#include "Hash.h"
#include "Intersections.h"
#include "Iri.h"
#include "JsonTools.h"
#include "Layer.h"
#include "ObservationReader.h"
#include "PointData.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/querydata/Engine.h>
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
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <timeseries/ParameterTools.h>
#include <iomanip>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Holder for data values
 */
// ----------------------------------------------------------------------

namespace
{
using PointValues = std::vector<PointData>;
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Forecast reader
 */
// ----------------------------------------------------------------------

PointValues read_forecasts(const SymbolLayer& layer,
                           const Engine::Querydata::Q& q,
                           const Fmi::SpatialReference& crs,
                           const Fmi::Box& box,
                           const Fmi::DateTime& valid_time,
                           const State& state)
{
  try
  {
    // Generate the coordinates for the symbols

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode, state);

    // querydata API for value() sucks

    std::optional<Spine::Parameter> param;
    if (layer.param_funcs)
      param = layer.param_funcs->parameter;

    std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::LocalDateTime localdatetime(valid_time, Fmi::TimeZonePtr::utc);

    PointValues pointvalues;
    auto mylocale = std::locale::classic();
    NFmiPoint dummy;

    for (const auto& point : points)
    {
      if (!layer.inside(box, point.x, point.y))
        continue;

      if (layer.symbols.empty())
      {
        PointData missingvalue{point, kFloatMissing};
        pointvalues.push_back(missingvalue);
      }
      else
      {
        Spine::Location loc(point.latlon.X(), point.latlon.Y());

        // Q API SUCKS!!
        Engine::Querydata::ParameterOptions options(
            *param, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);

        TS::Value result =
            AggregationUtility::get_qengine_value(q, options, localdatetime, layer.param_funcs);

        // auto result = q->value(options, localdatetime);
        if (boost::get<double>(&result) != nullptr)
        {
          double tmp = *boost::get<double>(&result);
          pointvalues.push_back(PointData{point, tmp});
        }
        else if (boost::get<int>(&result) != nullptr)
        {
          double tmp = *boost::get<int>(&result);
          pointvalues.push_back(PointData{point, tmp});
        }
        else
        {
          PointData missingvalue{point, kFloatMissing};
          pointvalues.push_back(missingvalue);
        }
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

PointValues read_gridForecasts(const SymbolLayer& layer,
                               const Engine::Grid::Engine* /* gridEngine */,
                               QueryServer::Query& query,
                               const Fmi::SpatialReference& crs,
                               const Fmi::Box& box,
                               const Fmi::DateTime& valid_time,
                               const State& state)
{
  try
  {
    // Generate the coordinates for the symbols

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(nullptr, crs, box, forecast_mode, state);

    PointValues pointvalues;

    int width = 0;
    int height = 0;

    const char* widthStr = query.mAttributeList.getAttributeValue("grid.width");
    const char* heightStr = query.mAttributeList.getAttributeValue("grid.height");

    if (widthStr)
      width = Fmi::stoi(widthStr);

    if (heightStr)
      height = Fmi::stoi(heightStr);

    T::ParamValue_vec* values = nullptr;

    for (const auto& param : query.mQueryParameterList)
    {
      for (const auto& val : param.mValueList)
      {
        if (!val->mValueVector.empty())
          values = &val->mValueVector;
      }
    }
    if (layer.symbols.empty())
    {
      for (const auto& point : points)
      {
        if (layer.inside(box, point.x, point.y))
        {
          PointData missingvalue{point, kFloatMissing};
          pointvalues.push_back(missingvalue);
        }
      }
    }
    else if (values && !values->empty())
    {
      for (const auto& point : points)
      {
        if (layer.inside(box, point.x, point.y))
        {
          size_t pos = (height - point.y - 1) * width + point.x;

          if (pos < values->size())
          {
            double tmp = (*values)[pos];
            if (tmp != ParamValueMissing)
            {
              pointvalues.push_back(PointData{point, tmp});
            }
            else
            {
              PointData missingvalue{point, kFloatMissing};
              pointvalues.push_back(missingvalue);
            }
            // printf("Point %d,%d  => %f,%f  = %f\n",point.x,point.y,point.latlon.X(),
            // point.latlon.Y(),tmp);
          }
          else
          {
            PointData missingvalue{point, kFloatMissing};
            pointvalues.push_back(missingvalue);
          }
        }
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
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void SymbolLayer::init(Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Symbol-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(parameter, theJson, "parameter");
    if (parameter && producer && !isFlashOrMobileProducer(*producer))
    {
      try
      {
        param_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*parameter);
      }
      catch (...)
      {
      }
    }

    JsonTools::remove_string(unit_conversion, theJson, "unit_conversion");
    JsonTools::remove_double(multiplier, theJson, "multiplier");
    JsonTools::remove_double(offset, theJson, "offset");

    auto json = JsonTools::remove(theJson, "positions");
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }

    JsonTools::remove_int(minvalues, theJson, "minvalues");
    JsonTools::remove_double(maxdistance, theJson, "maxdistance");
    JsonTools::remove_string(symbol, theJson, "symbol");
    JsonTools::remove_double(scale, theJson, "scale");
    JsonTools::remove_int(dx, theJson, "dx");
    JsonTools::remove_int(dy, theJson, "dy");

    json = JsonTools::remove(theJson, "symbols");
    if (!json.isNull())
      JsonTools::extract_array("symbols", symbols, json, theConfig);

    point_value_options.init(theJson);
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

void SymbolLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    if (source && *source == "grid")
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

void SymbolLayer::generate_gridEngine(CTPP::CDT& theGlobals,
                                      CTPP::CDT& theLayersCdt,
                                      State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    // Time execution
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "SymbolLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // A symbol must be defined either globally or for values

    if (!symbol && symbols.empty())
      throw Fmi::Exception(
          BCP,
          "Must define default symbol with 'symbol' or several 'symbols' for specific values in a "
          "symbol-layer");

    if (*projection.crs == "data" && !parameter)
      return;

    // Establish the data

    // bool is_legend = theGlobals.Exists("legend");

    // Make sure position generation is initialized

    if (!positions)
      positions = Positions{};

    // Add layer margins to position generation
    positions->addMargins(xmargin, ymargin);

    // Establish the parameter

    if (!symbols.empty())
    {
      if (!parameter)
        throw Fmi::Exception(BCP, "Parameter not set for symbol-layer");
    }

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);

    auto valid_time = getValidTime();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time;

    std::string wkt = *projection.crs;
    // std::cout << wkt << "\n";

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
      {
        auto crs = projection.getCRS();
        char* out = nullptr;
        crs.get()->exportToWkt(&out);
        wkt = out;
        CPLFree(out);
      }

      // std::cout << wkt << "\n";

      // Adding the bounding box information into the query.

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      auto bbox = fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      const auto& box = projection.getBox();
      bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means
      // that we we do not know the actual projection yet and we have to wait that the grid-engine
      // delivers us the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    if (!parameter && !projection.projectionParameter)
      return;

    std::string pName;

    if (parameter)
      pName = *parameter;
    else
      pName = *projection.projectionParameter;

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

    for (auto& param : originalGridQuery->mQueryParameterList)
    {
      param.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      param.mType = QueryServer::QueryParameter::Type::Vector;
      param.mFlags = QueryServer::QueryParameter::Flags::ReturnCoordinates;

      if (geometryId)
        param.mGeometryId = *geometryId;

      if (levelId)
      {
        param.mParameterLevelId = *levelId;
      }

      if (level)
      {
        param.mParameterLevel = C_INT(*level);
      }
      else if (pressure)
      {
        param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
        param.mParameterLevel = C_INT(*pressure);
      }

      if (elevation_unit)
      {
        if (*elevation_unit == "m")
          param.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;

        if (*elevation_unit == "p")
          param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
      }

      if (forecastType)
        param.mForecastType = C_INT(*forecastType);

      if (forecastNumber)
        param.mForecastNumber = C_INT(*forecastNumber);
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
        originalGridQuery->mAttributeList.addAttribute("grid.width",
                                                       std::to_string(*projection.xsize));

      if (projection.ysize)
        originalGridQuery->mAttributeList.addAttribute("grid.height",
                                                       std::to_string(*projection.ysize));
    }

    if (wkt == "data" && projection.x1 && projection.y1 && projection.x2 && projection.y2)
    {
      auto bbox = fmt::format(
          "{},{},{},{}", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    // query.print(std::cout,0,0);

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

    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    if (wkt == "data")
      return;

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time, theState);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Data conversion settings

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    double xmultiplier = (multiplier ? *multiplier : 1.0);
    double xoffset = (offset ? *offset : 0.0);

    // Set the proper symbol on if it is needed
    if (!symbols.empty())
    {
      if (!parameter)
        throw Fmi::Exception(
            BCP, "Parameter not set for SymbolLayer even though multiple symbols are in use");
    }

    // Establish the numbers to draw.

    PointValues pointvalues;
    pointvalues = read_gridForecasts(*this, gridEngine, *query, crs, box, valid_time, theState);

    pointvalues = prioritize(pointvalues, point_value_options);

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the symbols

    theState.addAttributes(theGlobals, group_cdt, attributes);

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

      // Start generating the hash

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      std::string iri;

      if (symbol)
        iri = *symbol;

      // librsvg cannot handle scale + transform, must move former into latter
      std::optional<double> rescale;

      if (!symbols.empty())
      {
        auto selection = Select::attribute(symbols, value);
        if (selection)
        {
          if (selection->symbol)
            iri = *selection->symbol;

          auto scaleattr = selection->attributes.remove("scale");
          if (scaleattr)
            rescale = Fmi::stod(*scaleattr);

          theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
        }
      }

      if (!iri.empty())
      {
        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        // Lack of CSS3 transform support forces us to use a direct transformation
        // which may override user settings
        tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

        int x = point.x + point.dx + (dx ? *dx : 0);
        int y = point.y + point.dy + (dy ? *dy : 0);

        std::string tmp = fmt::sprintf("translate(%d,%d)", x, y);

        double newscale = (scale ? *scale : 1.0) * (rescale ? *rescale : 1.0);
        if (newscale != 1.0)
          tmp += fmt::sprintf(" scale(%g)", newscale);

        tag_cdt["attributes"]["transform"] = tmp;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in symbol layer")
          .addParameter("valid values", std::to_string(valid_count))
          .addParameter("minimum count", std::to_string(minvalues));

    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void SymbolLayer::generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // Time execution
    std::string report = "SymbolLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // A symbol must be defined either globally or for values

    if (!symbol && symbols.empty())
      throw Fmi::Exception(
          BCP,
          "Must define default symbol with 'symbol' or several 'symbols' for specific values in a "
          "symbol-layer");

    // Establish the data

    bool is_legend = theGlobals.Exists("legend");
    bool use_observations = theState.isObservation(producer) && !is_legend;
    auto q = getModel(theState);

    // Make sure position generation is initialized

    if (!positions)
    {
      positions = Positions{};
      if (use_observations)
        positions->layout = Positions::Layout::Data;
    }

    // Add layer margins to position generation
    positions->addMargins(xmargin, ymargin);

    // Establish the valid time

    const auto now = Fmi::SecondClock::local_time();
    const auto valid_time = (!is_legend ? getValidTime() : now);
    const auto valid_time_period = (!is_legend ? getValidTimePeriod() : Fmi::TimePeriod(now, now));

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

    // Set the proper symbol on if it is needed
    if (!symbols.empty())
    {
      if (!parameter)
        throw Fmi::Exception(
            BCP, "Parameter not set for SymbolLayer even though multiple symbols are in use");
    }

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Data conversion settings

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    double xmultiplier = (multiplier ? *multiplier : 1.0);
    double xoffset = (offset ? *offset : 0.0);

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time, theState);

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    PointValues pointvalues;

    if (!use_observations)
      pointvalues = read_forecasts(*this, q, crs, box, valid_time, theState);
#ifndef WITHOUT_OBSERVATION
    else
      pointvalues = ObservationReader::read(theState,
                                            {*parameter},
                                            *this,
                                            *positions,
                                            maxdistance,
                                            crs,
                                            box,
                                            valid_time,
                                            valid_time_period);
#endif

    pointvalues = prioritize(pointvalues, point_value_options);

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the symbols

    theState.addAttributes(theGlobals, group_cdt, attributes);

    int valid_count = 0;

    for (const auto& pointvalue : pointvalues)
    {
      auto value = pointvalue[0];
      if (value != kFloatMissing)
      {
        ++valid_count;
        value = xmultiplier * value + xoffset;
      }

      const auto& point = pointvalue.point();

      // Start generating the hash

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      std::string iri;

      if (symbol)
        iri = *symbol;

      // librsvg cannot handle scale + transform, must move former into latter
      std::optional<double> rescale;

      if (!symbols.empty())
      {
        auto selection = Select::attribute(symbols, value);
        if (selection)
        {
          if (selection->symbol)
            iri = *selection->symbol;

          auto scaleattr = selection->attributes.remove("scale");
          if (scaleattr)
            rescale = Fmi::stod(*scaleattr);

          theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
        }
      }

      if (!iri.empty())
      {
        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        // Lack of CSS3 transform support forces us to use a direct transformation
        // which may override user settings
        tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

        int x = point.x + point.dx + (dx ? *dx : 0);
        int y = point.y + point.dy + (dy ? *dy : 0);

        std::string tmp = fmt::sprintf("translate(%d,%d)", x, y);

        double newscale = (scale ? *scale : 1.0) * (rescale ? *rescale : 1.0);
        if (newscale != 1.0)
          tmp += fmt::sprintf(" scale(%g)", newscale);

        tag_cdt["attributes"]["transform"] = tmp;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in symbol layer")
          .addParameter("valid values", std::to_string(valid_count))
          .addParameter("minimum count", std::to_string(minvalues));

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

void SymbolLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
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

std::size_t SymbolLayer::hash_value(const State& theState) const
{
  try
  {
    // Disable caching of very new observation layers
    if (theState.isObservation(producer))
    {
      const auto age = Fmi::SecondClock::universal_time() - getValidTime();
      if (age < Fmi::Minutes(5))
        return Fmi::bad_hash;
    }

    auto hash = Layer::hash_value(theState);

    if (!(source && *source == "grid"))
    {
      auto q = getModel(theState);
      if (q)
        Fmi::hash_combine(hash, Engine::Querydata::hash_value(q));
    }

    Fmi::hash_combine(hash, Fmi::hash_value(parameter));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Dali::hash_value(positions, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(maxdistance));
    Fmi::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(scale));
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Dali::hash_value(symbols, theState));
    Fmi::hash_combine(hash, point_value_options.hash_value());
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
