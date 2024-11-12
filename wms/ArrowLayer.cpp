//======================================================================
#include "ArrowLayer.h"
#include "AggregationUtility.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "JsonTools.h"
#include "Layer.h"
#include "ObservationReader.h"
#include "PointData.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"
#include <boost/math/constants/constants.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/querydata/Q.h>
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

const double pi = boost::math::constants::pi<double>();

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

PointValues read_forecasts(const ArrowLayer& layer,
                           const Engine::Querydata::Q& q,
                           const Fmi::SpatialReference& crs,
                           const Fmi::Box& box,
                           const Fmi::DateTime& valid_time,
                           const State& state)
{
  try
  {
    NFmiMetTime met_time = valid_time;

    std::optional<Spine::Parameter> dirparam;
    std::optional<Spine::Parameter> speedparam;
    std::optional<Spine::Parameter> uparam;
    std::optional<Spine::Parameter> vparam;

    std::optional<TS::ParameterAndFunctions> speed_funcs;
    std::optional<TS::ParameterAndFunctions> dir_funcs;
    std::optional<TS::ParameterAndFunctions> u_funcs;
    std::optional<TS::ParameterAndFunctions> v_funcs;

    if (layer.direction)
    {
      dir_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*layer.direction);
      dirparam = dir_funcs->parameter;
      //	dirparam = TS::ParameterFactory::instance().parse(*layer.direction);
    }
    if (layer.speed)
    {
      speed_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*layer.speed);
      speedparam = speed_funcs->parameter;
      // speedparam = TS::ParameterFactory::instance().parse(*layer.speed);
    }
    if (layer.u)
    {
      u_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*layer.u);
      uparam = u_funcs->parameter;
      //		uparam = TS::ParameterFactory::instance().parse(*layer.u);
    }
    if (layer.v)
    {
      v_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*layer.v);
      vparam = v_funcs->parameter;
      // vparam = TS::ParameterFactory::instance().parse(*layer.v);
    }

    if (speedparam && !q->param(speedparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + speedparam->name() + " not available in the arrow layer querydata");

    if (dirparam && !q->param(dirparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + dirparam->name() + " not available in the arrow layer querydata");

    // WindUMS and WindVMS are metaparameters, cannot check their existence here

    // We may need to convert relative U/V components to true north

    std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
    if (uparam && vparam && q->isRelativeUV())
      uvtransformation.reset(new Fmi::CoordinateTransformation("WGS84", q->SpatialReference()));

    // Generate the coordinates for the arrows

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode, state);

    PointValues pointvalues;

    // Q API SUCKS
    std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::LocalDateTime localdatetime(met_time, Fmi::TimeZonePtr::utc);
    std::string tmp;
    auto mylocale = std::locale::classic();
    NFmiPoint dummy;

    if (speedparam && !q->param(speedparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + speedparam->name() + " not available in the arrow layer querydata");

    if (dirparam && !q->param(dirparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + dirparam->name() + " not available in the arrow layer querydata");

    for (const auto& point : points)
    {
      if (!layer.inside(box, point.x, point.y))
        continue;

      // printf("POS %d,%d  %f %f\n",point.x, point.y,point.latlon.X(), point.latlon.Y());

      // Arrow direction and speed
      double wdir = kFloatMissing;
      double wspd = 0;
      Spine::Location loc(point.latlon.X(), point.latlon.Y());

      if (uparam && vparam)
      {
        auto up = Engine::Querydata::ParameterOptions(*uparam,
                                                      tmp,
                                                      loc,
                                                      tmp,
                                                      tmp,
                                                      *timeformatter,
                                                      tmp,
                                                      tmp,
                                                      mylocale,
                                                      tmp,
                                                      false,
                                                      dummy,
                                                      dummy);

        auto uresult = AggregationUtility::get_qengine_value(q, up, localdatetime, u_funcs);
        //        auto uresult = q->value(up, localdatetime);

        auto vp = Engine::Querydata::ParameterOptions(*vparam,
                                                      tmp,
                                                      loc,
                                                      tmp,
                                                      tmp,
                                                      *timeformatter,
                                                      tmp,
                                                      tmp,
                                                      mylocale,
                                                      tmp,
                                                      false,
                                                      dummy,
                                                      dummy);

        auto vresult = AggregationUtility::get_qengine_value(q, vp, localdatetime, v_funcs);

        //        auto vresult = q->value(vp, localdatetime);

        const double* u_ptr = std::get_if<double>(&uresult);
        const double* v_ptr = std::get_if<double>(&vresult);
        if (u_ptr && v_ptr)
        {
          auto uspd = *u_ptr;
          auto vspd = *v_ptr;

          if (uspd != kFloatMissing && vspd != kFloatMissing)
          {
            wspd = sqrt(uspd * uspd + vspd * vspd);
            if (uspd != 0 || vspd != 0)
            {
              // Note: qengine fixes orientation automatically
              wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
            }
          }
        }
      }
      else
      {
        if (dir_funcs && dir_funcs->functions.innerFunction.exists())
        {
          auto dp = Engine::Querydata::ParameterOptions(*dirparam,
                                                        tmp,
                                                        loc,
                                                        tmp,
                                                        tmp,
                                                        *timeformatter,
                                                        tmp,
                                                        tmp,
                                                        mylocale,
                                                        tmp,
                                                        false,
                                                        dummy,
                                                        dummy);

          auto dir_result = AggregationUtility::get_qengine_value(q, dp, localdatetime, dir_funcs);
          if (const double* ptr = std::get_if<double>(&dir_result))
            wdir = *ptr;
        }
        else
        {
          q->param(dirparam->number());
          wdir = q->interpolate(point.latlon, met_time, 180);
        }
        if (speedparam)
        {
          if (speed_funcs && speed_funcs->functions.innerFunction.exists())
          {
            auto sp = Engine::Querydata::ParameterOptions(*speedparam,
                                                          tmp,
                                                          loc,
                                                          tmp,
                                                          tmp,
                                                          *timeformatter,
                                                          tmp,
                                                          tmp,
                                                          mylocale,
                                                          tmp,
                                                          false,
                                                          dummy,
                                                          dummy);

            auto speed_result =
                AggregationUtility::get_qengine_value(q, sp, localdatetime, speed_funcs);
            if (const double* ptr = std::get_if<double>(&speed_result))
              wspd = *ptr;
          }
          else
          {
            q->param(speedparam->number());
            wspd = q->interpolate(point.latlon, met_time, 180);
          }
        }
      }

      // Skip points with invalid values
      if (wdir == kFloatMissing || wspd == kFloatMissing)
        continue;

      PointData value{point, wspd, wdir};
      pointvalues.push_back(value);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read observations from the database");
  }
}  // namespace Dali

PointValues read_gridForecasts(const ArrowLayer& layer,
                               const Engine::Grid::Engine* gridEngine,
                               QueryServer::Query& query,
                               std::optional<std::string> dirParam,
                               std::optional<std::string> speedParam,
                               std::optional<std::string> uParam,
                               std::optional<std::string> vParam,
                               const Fmi::SpatialReference& crs,
                               const Fmi::Box& box,
                               const Fmi::DateTime& valid_time,
                               const State& state)
{
  try
  {
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    // Generate the coordinates for the symbols

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(nullptr, crs, box, forecast_mode, state);

    PointValues pointvalues;

    T::GridValueList* dirValues = nullptr;
    T::GridValueList* speedValues = nullptr;
    T::GridValueList* vValues = nullptr;
    T::GridValueList* uValues = nullptr;
    // uint originalGeometryId = 0;

    for (const auto& param : query.mQueryParameterList)
    {
      for (const auto& val : param.mValueList)
      {
        if (val->mValueList.getLength() == points.size())
        {
          // originalGeometryId = val->mGeometryId;

          if (dirParam && param.mParam == *dirParam)
            dirValues = &val->mValueList;
          else if (speedParam && param.mParam == *speedParam)
            speedValues = &val->mValueList;
          else if (vParam && param.mParam == *vParam)
            vValues = &val->mValueList;
          else if (uParam && param.mParam == *uParam)
            uValues = &val->mValueList;
        }
      }
    }

    if (dirValues && dirValues->getLength())
    {
      uint len = dirValues->getLength();
      for (uint t = 0; t < len; t++)
      {
        auto point = points[t];
        T::GridValue* rec = dirValues->getGridValuePtrByIndex(t);
        T::ParamValue wdir = rec->mValue;

        if (wdir != ParamValueMissing)
        {
          T::ParamValue wspeed = 0;
          if (speedValues)
          {
            T::GridValue* srec = speedValues->getGridValuePtrByIndex(t);
            if (srec)
              wspeed = srec->mValue;
            else
              wspeed = 10;
          }
          else
            wspeed = 10;

          if (wspeed != ParamValueMissing)
          {
            PointData value{point, wspeed, wdir};
            pointvalues.push_back(value);
            // printf("POS %d,%d  %f %f\n",point.x, point.y,point.latlon.X(), point.latlon.Y());
          }
        }
      }
      return pointvalues;
    }

    if (uValues && vValues && uValues->getLength() && vValues->getLength())
    {
      int relativeUV = 0;
      const char* originalRelativeUVStr =
          query.mAttributeList.getAttributeValue("grid.original.relativeUV");
      const char* originalCrs = query.mAttributeList.getAttributeValue("grid.original.crs");

      if (originalRelativeUVStr)
        relativeUV = Fmi::stoi(originalRelativeUVStr);

      // We may need to convert relative U/V components to true north
      std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;

      if (relativeUV && originalCrs)
        uvtransformation.reset(new Fmi::CoordinateTransformation("WGS84", originalCrs));

      uint len = vValues->getLength();
      for (uint t = 0; t < len; t++)
      {
        auto point = points[t];
        T::GridValue* vrec = vValues->getGridValuePtrByIndex(t);
        T::ParamValue v = vrec->mValue;

        T::GridValue* urec = uValues->getGridValuePtrByIndex(t);
        T::ParamValue u = urec->mValue;

        double wdir = ParamValueMissing;
        double wspeed = 0;

        if (v != ParamValueMissing && u != ParamValueMissing)
        {
          wspeed = sqrt(u * u + v * v);

          if (!uvtransformation)
            wdir = fmod(180 + 180 / pi * atan2(u, v), 360);
          else
          {
            auto rot = Fmi::OGR::gridNorth(*uvtransformation, point.latlon.X(), point.latlon.Y());
            if (!rot)
              continue;
            wdir = fmod(180 - *rot + 180 / pi * atan2(u, v), 360);
          }
        }

        if (wdir != ParamValueMissing && wspeed != ParamValueMissing)
        {
          PointData value{point, wspeed, wdir};
          pointvalues.push_back(value);
        }
      }
      return pointvalues;
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read observations from the database");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void ArrowLayer::init(Json::Value& theJson,
                      const State& theState,
                      const Config& theConfig,
                      const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Arrow-layer JSON is not an object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(direction, theJson, "direction");
    JsonTools::remove_string(speed, theJson, "speed");
    JsonTools::remove_string(u, theJson, "u");
    JsonTools::remove_string(v, theJson, "v");
    JsonTools::remove_double(fixedspeed, theJson, "fixedspeed");
    JsonTools::remove_double(fixeddirection, theJson, "fixeddirection");
    JsonTools::remove_string(symbol, theJson, "symbol");
    JsonTools::remove_double(scale, theJson, "scale");
    JsonTools::remove_bool(southflop, theJson, "southflop");
    JsonTools::remove_bool(northflop, theJson, "northflop");
    JsonTools::remove_bool(flip, theJson, "flip");

    auto json = JsonTools::remove(theJson, "positions");
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }

    JsonTools::remove_int(dx, theJson, "dx");
    JsonTools::remove_int(dy, theJson, "dy");
    JsonTools::remove_int(minvalues, theJson, "minvalues");
    JsonTools::remove_double(maxdistance, theJson, "maxdistance");
    JsonTools::remove_string(unit_conversion, theJson, "unit_conversion");
    JsonTools::remove_double(multiplier, theJson, "multiplier");
    JsonTools::remove_double(offset, theJson, "offset");
    JsonTools::remove_double(minrotationspeed, theJson, "minrotationspeed");

    json = JsonTools::remove(theJson, "arrows");
    if (!json.isNull())
      JsonTools::extract_array("arrows", arrows, json, theConfig);

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

void ArrowLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // if (!validLayer(theState))
    //  return;

    if (source && *source == "grid")
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Operation failed!", nullptr);
    exception.addParameter("qid", qid);

    if (producer)
      exception.addParameter("Producer", *producer);
    if (direction)
      exception.addParameter("Direction", *direction);
    if (speed)
      exception.addParameter("Speed", *speed);
    if (v)
      exception.addParameter("V", *v);
    if (u)
      exception.addParameter("U", *u);
    throw exception;
  }
}

void ArrowLayer::generate_gridEngine(CTPP::CDT& theGlobals,
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
      std::string report = "ArrowLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // A symbol must be defined either globally or for speed ranges

    if (!symbol && arrows.empty())
      throw Fmi::Exception(
          BCP, "Must define arrow with 'symbol' or 'arrows' to define the symbol for arrows");

    // Make sure position generation is initialized

    if (!positions)
      positions = Positions{};

    // Add layer margins to position generation
    positions->addMargins(xmargin, ymargin);

    // Establish the parameters

    std::vector<std::string> paramList;

    std::optional<std::string> dirparam;
    std::optional<std::string> speedparam;
    std::optional<std::string> uparam;
    std::optional<std::string> vparam;

    if (direction)
      paramList.push_back(*direction);
    if (speed)
      paramList.push_back(*speed);
    if (u)
      paramList.push_back(*u);
    if (v)
      paramList.push_back(*v);

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);

    auto valid_time = getValidTime();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time;

    std::string wkt = *projection.crs;
    if (wkt != "data")
    {
      // Getting the bounding box and the WKT of the requested projection.

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

      positions->init(producer, projection, valid_time, theState);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means
      // that we we do not know the actual projection yet and we have to wait that the grid-engine
      // delivers us the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    std::string paramBuf;

    for (auto& pName : paramList)
    {
      auto pos = pName.find(".raw");
      if (pos != std::string::npos)
      {
        attributeList.addAttribute("areaInterpolationMethod",
                                   std::to_string(T::AreaInterpolationMethod::Nearest));
        pName.erase(pos, 4);
      }

      std::string param = gridEngine->getParameterString(producerName, pName);

      if (!projection.projectionParameter)
        projection.projectionParameter = param;

      if (param == pName && originalGridQuery->mProducerNameList.empty())
      {
        gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
        if (originalGridQuery->mProducerNameList.empty())
          originalGridQuery->mProducerNameList.push_back(producerName);
      }

      if (!paramBuf.empty())
        paramBuf += ',';

      paramBuf += param;
    }

    attributeList.addAttribute("param", paramBuf);

    std::string forecastTime = Fmi::to_iso_string(getValidTime());
    attributeList.addAttribute("startTime", forecastTime);
    attributeList.addAttribute("endTime", forecastTime);
    attributeList.addAttribute("timelist", forecastTime);
    attributeList.addAttribute("timezone", "UTC");

    if (origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery, attributeList);

    // Filling information into the query object.

    if (positions)
    {
      const bool forecast_mode = true;
      const auto& box = projection.getBox();
      auto points =
          positions->getPoints(nullptr, projection.getCRS(), box, forecast_mode, theState);

      T::Coordinate_vec coordinates;
      for (const auto& point : points)
        coordinates.emplace_back(point.latlon.X(), point.latlon.Y());

      originalGridQuery->mAreaCoordinates.push_back(coordinates);
      originalGridQuery->mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;
    }

    for (auto& param : originalGridQuery->mQueryParameterList)
    {
      if (positions)
      {
        param.mLocationType = QueryServer::QueryParameter::LocationType::Point;
        param.mType = QueryServer::QueryParameter::Type::PointValues;
      }
      else
      {
        param.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
        param.mType = QueryServer::QueryParameter::Type::Vector;
        param.mFlags = QueryServer::QueryParameter::Flags::NoReturnValues;
      }

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

    // Get projection details

    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    if (wkt == "data")
      return;

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time, theState);

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the arrows
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Establish the relevant numbers

    PointValues pointvalues = read_gridForecasts(
        *this, gridEngine, *query, direction, speed, u, v, crs, box, valid_time, theState);

    // Coordinate transformation from WGS84 to output SRS so that we can rotate
    // winds according to map north

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    // Alter units if requested

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    pointvalues = prioritize(pointvalues, point_value_options);

    // Render the collected values

    int valid_count = 0;

    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point();

      // Select arrow based on speed or U- and V-components, if available
      bool check_speeds = (!arrows.empty() && (speed || (u && v)));

      double wspd = pointvalue[0];
      double wdir = pointvalue[1];

      if (wdir == kFloatMissing)
        continue;

      if (check_speeds && wspd == kFloatMissing)
        continue;

      ++valid_count;

      // printf("POINT %d,%d %f,%f\n",point.x,point.y,wdir,wspd);

      // Unit transformation
      double xmultiplier = (multiplier ? *multiplier : 1.0);
      double xoffset = (offset ? *offset : 0.0);
      if (wspd != kFloatMissing)
        wspd = xmultiplier * wspd + xoffset;

      // Apply final rotation to output coordinate system
      auto fix = Fmi::OGR::gridNorth(transformation, point.latlon.X(), point.latlon.Y());
      if (!fix)
        continue;

      wdir = fmod(wdir + *fix, 360);

      // North wind blows, rotate 180 degrees
      double rotate = fmod(wdir + 180, 360);

      // Disable rotation for slow wind speeds if a limit is set
      int nrotate = lround(rotate);
      if (wspd != kFloatMissing && minrotationspeed && wspd < *minrotationspeed)
        nrotate = 0;

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      // librsvg cannot handle scale + transform, must move former into latter

      std::optional<double> rescale;

      // Determine the symbol to be used
      std::string iri;

      if (!check_speeds)
      {
        if (symbol)
          iri = *symbol;
      }
      else
      {
        auto selection = Select::attribute(arrows, wspd);
        if (selection)
        {
          if (selection->symbol)
            iri = *selection->symbol;
          else if (symbol)
            iri = *symbol;

          auto scaleattr = selection->attributes.remove("scale");
          if (scaleattr)
            rescale = Fmi::stod(*scaleattr);

          theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
        }
      }

      if (!iri.empty())
      {
        std::string IRI = Iri::normalize(iri);
        bool flop =
            ((southflop && (point.latlon.Y() < 0)) || (northflop && (point.latlon.Y() > 0)));

        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

        // Using x and y attributes messes up rotation, hence we
        // must use the translate transformation instead.
        std::string transform;

        double yscale = (flip ? -1 : 1) * (scale ? *scale : 1.0);
        double xscale = (flop ? -yscale : yscale);

        if (rescale)
        {
          xscale *= *rescale;
          yscale *= *rescale;
        }

        int x = point.x + point.dx + (dx ? *dx : 0);
        int y = point.y + point.dy + (dy ? *dy : 0);

        // printf("-- POINT %d,%d %f,%f\n",x,y,wdir,wspd);

        if (nrotate == 0)
        {
          if (xscale == 1 && yscale == 1)
            transform = fmt::sprintf("translate(%d %d)", x, y);
          else if (xscale == yscale)
            transform = fmt::sprintf("translate(%d %d) scale(%g)", x, y, xscale);
          else
            transform = fmt::sprintf("translate(%d %d) scale(%g %g)", x, y, xscale, yscale);
        }
        else
        {
          if (xscale == 1 && yscale == 1)
            transform = fmt::sprintf("translate(%d %d) rotate(%d)", x, y, nrotate);
          else if (xscale == yscale)
            transform =
                fmt::sprintf("translate(%d %d) rotate(%d) scale(%g)", x, y, nrotate, xscale);
          else
            transform = fmt::sprintf(
                "translate(%d %d) rotate(%d) scale(%g %g)", x, y, nrotate, xscale, yscale);
        }

        tag_cdt["attributes"]["transform"] = transform;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in arrow layer")
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

void ArrowLayer::generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // Time execution

    std::string report = "ArrowLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // A symbol must be defined either globally or for speed ranges

    if (!symbol && arrows.empty())
      throw Fmi::Exception(
          BCP, "Must define arrow with 'symbol' or 'arrows' to define the symbol for arrows");

    // Establish the data

    bool use_observations = theState.isObservation(producer);
    Engine::Querydata::Q q = getModel(theState);

    // Make sure position generation is initialized

    if (!positions)
    {
      positions = Positions{};
      if (use_observations)
        positions->layout = Positions::Layout::Data;
    }

    // Add layer margins to position generation
    positions->addMargins(xmargin, ymargin);

    // Establish the level

    if (level)
    {
      if (use_observations)
        throw std::runtime_error("Cannot set level value for observations in NumberLayer");

      bool match = false;
      for (q->resetLevel(); !match && q->nextLevel();)
        match = (q->levelValue() == *level);
      if (!match)
        throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available");
    }

    // Get projection details

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Establish the valid time

    auto valid_time = getValidTime();
    auto valid_time_period = getValidTimePeriod();

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time, theState);

    // The parameters. TODO: Allow metaparameters, needs better Q API

    if ((u && (direction || speed)) || (v && (direction || speed)))
      throw Fmi::Exception(
          BCP, "ArrowLayer cannot define direction, speed and u- and v-components simultaneously");

    if ((u && !v) || (v && !u))
      throw Fmi::Exception(BCP, "ArrowLayer must define both U- and V-components");

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the arrows
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Establish the relevant numbers

    PointValues pointvalues;

    if (!use_observations)
      pointvalues = read_forecasts(*this, q, crs, box, valid_time, theState);
#ifndef WITHOUT_OBSERVATION
    else
    {
      if (Layer::isFlashOrMobileProducer(*producer))
        throw Fmi::Exception(BCP, "Cannot use flash or mobile producer in ArrowLayer");

      bool use_uv_components = (u && v);

      if (use_uv_components)
        pointvalues = ObservationReader::read(theState,
                                              {*u, *v},
                                              *this,
                                              *positions,
                                              maxdistance,
                                              crs,
                                              box,
                                              valid_time,
                                              valid_time_period);
      else
      {
        if (!speed)
          speed = "WindSpeedMS";  // default for arrows
        pointvalues = ObservationReader::read(theState,
                                              {*speed, *direction},
                                              *this,
                                              *positions,
                                              maxdistance,
                                              crs,
                                              box,
                                              valid_time,
                                              valid_time_period);
      }

      if (use_uv_components)
      {
        // Switch U/V to speed & direction
        for (auto& pointvalue : pointvalues)
        {
          auto uspd = pointvalue[0];
          auto vspd = pointvalue[1];

          if (uspd != kFloatMissing && vspd != kFloatMissing)
          {
            auto wspd = sqrt(uspd * uspd + vspd * vspd);
            auto wdir = kFloatMissing;
            if (uspd != 0 || vspd != 0)
              wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
            pointvalue[0] = wspd;
            pointvalue[1] = wdir;
          }
          else
          {
            pointvalue[0] = kFloatMissing;
            pointvalue[1] = kFloatMissing;
          }
        }
      }
    }
#endif

    pointvalues = prioritize(pointvalues, point_value_options);

    // Coordinate transformation from WGS84 to output SRS so that we can rotate
    // winds according to map north

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    // Alter units if requested

    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    // Render the collected values

    int valid_count = 0;

    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point();

      // Select arrow based on speed or U- and V-components, if available
      bool check_speeds = (!arrows.empty() && (speed || (u && v)));

      double wspd = pointvalue[0];
      double wdir = pointvalue[1];

      if (wdir == kFloatMissing)
        continue;

      if (check_speeds && wspd == kFloatMissing)
        continue;

      ++valid_count;

      // Fake values
      if (fixedspeed)
        wspd = *fixedspeed;
      if (fixeddirection)
        wdir = *fixeddirection;

      // Unit transformation
      double xmultiplier = (multiplier ? *multiplier : 1.0);
      double xoffset = (offset ? *offset : 0.0);
      if (wspd != kFloatMissing)
        wspd = xmultiplier * wspd + xoffset;

      // Apply final rotation to output coordinate system

      // TODO: Clean up the API on constness. GDAL is not properly const correct
      auto fix = Fmi::OGR::gridNorth(transformation, point.latlon.X(), point.latlon.Y());
      if (!fix)
        continue;
      wdir = fmod(wdir + *fix, 360);

      // North wind blows, rotate 180 degrees
      double rotate = fmod(wdir + 180, 360);

      // Disable rotation for slow wind speeds if a limit is set
      int nrotate = lround(rotate);
      if (wspd != kFloatMissing && minrotationspeed && wspd < *minrotationspeed)
        nrotate = 0;

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      // librsvg cannot handle scale + transform, must move former into latter

      std::optional<double> rescale;

      // Determine the symbol to be used
      std::string iri;

      if (!check_speeds)
      {
        if (symbol)
          iri = *symbol;
      }
      else
      {
        auto selection = Select::attribute(arrows, wspd);
        if (selection)
        {
          if (selection->symbol)
            iri = *selection->symbol;
          else if (symbol)
            iri = *symbol;

          auto scaleattr = selection->attributes.remove("scale");
          if (scaleattr)
            rescale = Fmi::stod(*scaleattr);

          theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
        }
      }

      if (!iri.empty())
      {
        std::string IRI = Iri::normalize(iri);
        bool flop =
            ((southflop && (point.latlon.Y() < 0)) || (northflop && (point.latlon.Y() > 0)));

        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

        // Using x and y attributes messes up rotation, hence we
        // must use the translate transformation instead.
        std::string transform;

        double yscale = (flip ? -1 : 1) * (scale ? *scale : 1.0);
        double xscale = (flop ? -yscale : yscale);

        if (rescale)
        {
          xscale *= *rescale;
          yscale *= *rescale;
        }

        int x = point.x + point.dx + (dx ? *dx : 0);
        int y = point.y + point.dy + (dy ? *dy : 0);

        if (nrotate == 0)
        {
          if (xscale == 1 && yscale == 1)
            transform = fmt::sprintf("translate(%d %d)", x, y);
          else if (xscale == yscale)
            transform = fmt::sprintf("translate(%d %d) scale(%g)", x, y, xscale);
          else
            transform = fmt::sprintf("translate(%d %d) scale(%g %g)", x, y, xscale, yscale);
        }
        else
        {
          if (xscale == 1 && yscale == 1)
            transform = fmt::sprintf("translate(%d %d) rotate(%d)", x, y, nrotate);
          else if (xscale == yscale)
            transform =
                fmt::sprintf("translate(%d %d) rotate(%d) scale(%g)", x, y, nrotate, xscale);
          else
            transform = fmt::sprintf(
                "translate(%d %d) rotate(%d) scale(%g %g)", x, y, nrotate, xscale, yscale);
        }

        tag_cdt["attributes"]["transform"] = transform;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in arrow layer")
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

void ArrowLayer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  if (theState.isObservation(producer))
    return;

  if (direction)
  {
    ParameterInfo info(*direction);
    info.producer = producer;
    info.level = level;
    add(infos, info);
  }
  if (speed)
  {
    ParameterInfo info(*speed);
    info.producer = producer;
    info.level = level;
    add(infos, info);
  }
  if (u)
  {
    ParameterInfo info(*u);
    info.producer = producer;
    info.level = level;
    add(infos, info);
  }
  if (v)
  {
    ParameterInfo info(*v);
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

std::size_t ArrowLayer::hash_value(const State& theState) const
{
  try
  {
    // Disable caching of observation layers
    if (theState.isObservation(producer))
      return Fmi::bad_hash;

    auto hash = Layer::hash_value(theState);

    if (!(source && *source == "grid"))
    {
      auto q = getModel(theState);
      if (q)
        Fmi::hash_combine(hash, Engine::Querydata::hash_value(q));
    }

    Fmi::hash_combine(hash, Fmi::hash_value(direction));
    Fmi::hash_combine(hash, Fmi::hash_value(speed));
    Fmi::hash_combine(hash, Fmi::hash_value(u));
    Fmi::hash_combine(hash, Fmi::hash_value(v));
    Fmi::hash_combine(hash, Fmi::hash_value(fixedspeed));
    Fmi::hash_combine(hash, Fmi::hash_value(fixeddirection));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Fmi::hash_value(minrotationspeed));
    Fmi::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(scale));
    Fmi::hash_combine(hash, Fmi::hash_value(southflop));
    Fmi::hash_combine(hash, Fmi::hash_value(northflop));
    Fmi::hash_combine(hash, Fmi::hash_value(flip));
    Fmi::hash_combine(hash, Dali::hash_value(positions, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Fmi::hash_value(minvalues));
    Fmi::hash_combine(hash, Fmi::hash_value(maxdistance));
    Fmi::hash_combine(hash, Dali::hash_value(arrows, theState));
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
