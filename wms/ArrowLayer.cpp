//======================================================================
#include "ArrowLayer.h"
#include "AggregationUtility.h"
#include "Config.h"
#include "GridDataGeoTiff.h"
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

namespace
{
using PointValues = std::vector<PointData>;

// ----------------------------------------------------------------------
/*!
 * \brief Convert U/V wind components to speed and direction.
 *
 * When uvtransformation is provided the U/V components are relative
 * to the grid and must be rotated to true north before computing the
 * meteorological wind direction.
 *
 * Returns nullopt if either input is missing or the rotation fails.
 */
// ----------------------------------------------------------------------

struct SpeedAndDirection
{
  double speed;
  double direction;
};

std::optional<SpeedAndDirection> uv_to_speed_and_direction(
    double u,
    double v,
    const std::shared_ptr<Fmi::CoordinateTransformation>& uvtransformation,
    double lon,
    double lat)
{
  if (u == kFloatMissing || v == kFloatMissing)
    return std::nullopt;

  double wspd = sqrt(u * u + v * v);
  double wdir = 0;

  if (u != 0 || v != 0)
  {
    if (!uvtransformation)
    {
      wdir = fmod(180 + 180 / pi * atan2(u, v), 360);
    }
    else
    {
      auto rot = Fmi::OGR::gridNorth(*uvtransformation, lon, lat);
      if (!rot)
        return std::nullopt;
      wdir = fmod(180 - *rot + 180 / pi * atan2(u, v), 360);
    }
  }

  return SpeedAndDirection{wspd, wdir};
}

// ----------------------------------------------------------------------
/*!
 * \brief Build the SVG transform attribute string for an arrow.
 */
// ----------------------------------------------------------------------

std::string make_arrow_transform(int x, int y, int nrotate, double xscale, double yscale)
{
  if (nrotate == 0)
  {
    if (xscale == 1 && yscale == 1)
      return fmt::sprintf("translate(%d %d)", x, y);
    if (xscale == yscale)
      return fmt::sprintf("translate(%d %d) scale(%g)", x, y, xscale);
    return fmt::sprintf("translate(%d %d) scale(%g %g)", x, y, xscale, yscale);
  }

  if (xscale == 1 && yscale == 1)
    return fmt::sprintf("translate(%d %d) rotate(%d)", x, y, nrotate);
  if (xscale == yscale)
    return fmt::sprintf("translate(%d %d) rotate(%d) scale(%g)", x, y, nrotate, xscale);
  return fmt::sprintf("translate(%d %d) rotate(%d) scale(%g %g)", x, y, nrotate, xscale, yscale);
}

// ----------------------------------------------------------------------
/*!
 * \brief Read forecast data from qEngine for all arrow positions.
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
    }
    if (layer.speed)
    {
      speed_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*layer.speed);
      speedparam = speed_funcs->parameter;
    }
    if (layer.u)
    {
      u_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*layer.u);
      uparam = u_funcs->parameter;
    }
    if (layer.v)
    {
      v_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*layer.v);
      vparam = v_funcs->parameter;
    }

    if (speedparam && !q->param(speedparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + speedparam->name() + " not available in the arrow layer querydata");

    if (dirparam && !q->param(dirparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + dirparam->name() + " not available in the arrow layer querydata");

    // We may need to convert relative U/V components to true north
    std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
    if (uparam && vparam && q->isRelativeUV())
      uvtransformation =
          std::make_shared<Fmi::CoordinateTransformation>("WGS84", q->SpatialReference());

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode, state);

    PointValues pointvalues;

    // Q API requires these helpers
    std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::LocalDateTime localdatetime(met_time, Fmi::TimeZonePtr::utc);
    std::string tmp;
    const auto& mylocale = std::locale::classic();
    NFmiPoint dummy;

    for (const auto& point : points)
    {
      if (!layer.inside(box, point.x, point.y))
        continue;

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

        auto uresult = AggregationUtility::get_qengine_value(q, up, localdatetime, u_funcs);
        auto vresult = AggregationUtility::get_qengine_value(q, vp, localdatetime, v_funcs);

        const double* u_ptr = std::get_if<double>(&uresult);
        const double* v_ptr = std::get_if<double>(&vresult);

        if (u_ptr && v_ptr)
        {
          auto result = uv_to_speed_and_direction(
              *u_ptr, *v_ptr, uvtransformation, point.latlon.X(), point.latlon.Y());
          if (result)
          {
            wspd = result->speed;
            wdir = result->direction;
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

      if (wdir == kFloatMissing || wspd == kFloatMissing)
        continue;

      pointvalues.push_back(PointData{point, wspd, wdir});
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read forecasts from querydata");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read forecast data from gridEngine for all arrow positions.
 */
// ----------------------------------------------------------------------

PointValues read_gridForecasts(const ArrowLayer& layer,
                               const Engine::Grid::Engine* gridEngine,
                               QueryServer::Query& query,
                               std::optional<std::string> dirParam,
                               std::optional<std::string> speedParam,
                               std::optional<std::string> uParam,
                               std::optional<std::string> vParam,
                               const Fmi::SpatialReference& crs,
                               const Fmi::Box& box,
                               const State& state)
{
  try
  {
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(nullptr, crs, box, forecast_mode, state);

    PointValues pointvalues;

    T::GridValueList* dirValues = nullptr;
    T::GridValueList* speedValues = nullptr;
    T::GridValueList* uValues = nullptr;
    T::GridValueList* vValues = nullptr;

    for (const auto& param : query.mQueryParameterList)
    {
      for (const auto& val : param.mValueList)
      {
        if (val->mValueList.getLength() != points.size())
          continue;

        if (dirParam && param.mParam == *dirParam)
          dirValues = &val->mValueList;
        else if (speedParam && param.mParam == *speedParam)
          speedValues = &val->mValueList;
        else if (uParam && param.mParam == *uParam)
          uValues = &val->mValueList;
        else if (vParam && param.mParam == *vParam)
          vValues = &val->mValueList;
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

        if (wdir == ParamValueMissing)
          continue;

        T::ParamValue wspeed = 0;
        if (speedValues)
        {
          T::GridValue* srec = speedValues->getGridValuePtrByIndex(t);
          wspeed = (srec ? srec->mValue : 10);
        }
        else
          wspeed = 10;

        if (wspeed != ParamValueMissing)
          pointvalues.push_back(PointData{point, wspeed, wdir});
      }
      return pointvalues;
    }

    if (uValues && vValues && uValues->getLength() && vValues->getLength())
    {
      int relativeUV = 0;
      const char* relativeUVStr =
          query.mAttributeList.getAttributeValue("grid.original.relativeUV");
      const char* originalCrs = query.mAttributeList.getAttributeValue("grid.original.crs");

      if (relativeUVStr)
        relativeUV = Fmi::stoi(relativeUVStr);

      std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
      if (relativeUV && originalCrs)
        uvtransformation = std::make_shared<Fmi::CoordinateTransformation>("WGS84", originalCrs);

      uint len = vValues->getLength();
      for (uint t = 0; t < len; t++)
      {
        auto point = points[t];
        T::GridValue* urec = uValues->getGridValuePtrByIndex(t);
        T::GridValue* vrec = vValues->getGridValuePtrByIndex(t);

        auto result = uv_to_speed_and_direction(
            urec->mValue, vrec->mValue, uvtransformation, point.latlon.X(), point.latlon.Y());

        if (result)
          pointvalues.push_back(PointData{point, result->speed, result->direction});
      }
      return pointvalues;
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read forecasts from grid engine");
  }
}

}  // namespace

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
 * \brief Build a grid engine query for the given parameters and coordinates.
 *
 * Handles all the repetitive boilerplate: bounding box, parameter string,
 * time attributes, level/pressure/forecast flags, and projection size.
 */
// ----------------------------------------------------------------------

std::shared_ptr<QueryServer::Query> ArrowLayer::build_grid_query(
    const std::vector<std::string>& paramNames,
    const T::Coordinate_vec& coordinates,
    const State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    std::shared_ptr<QueryServer::Query> gridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*paraminfo.producer);

    // Bounding box
    const auto& box = projection.getBox();
    auto bl = projection.bottomLeftLatLon();
    auto tr = projection.topRightLatLon();
    gridQuery->mAttributeList.addAttribute(
        "grid.llbox", fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y()));
    gridQuery->mAttributeList.addAttribute(
        "grid.bbox", fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax()));

    // CRS
    std::string wkt = *projection.crs;
    if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
      wkt = projection.getCRS().WKT();

    // Parameters
    std::string paramBuf;
    for (auto pName : paramNames)
    {
      auto pos = pName.find(".raw");
      if (pos != std::string::npos)
      {
        attributeList.addAttribute("grid.areaInterpolationMethod",
                                   Fmi::to_string(T::AreaInterpolationMethod::Nearest));
        pName.erase(pos, 4);
      }

      std::string param = gridEngine->getParameterString(producerName, pName);

      if (!projection.projectionParameter)
        projection.projectionParameter = param;

      if (param == pName && gridQuery->mProducerNameList.empty())
      {
        gridEngine->getProducerNameList(producerName, gridQuery->mProducerNameList);
        if (gridQuery->mProducerNameList.empty())
          gridQuery->mProducerNameList.push_back(producerName);
      }

      if (!paramBuf.empty())
        paramBuf += ',';
      paramBuf += param;
    }
    attributeList.addAttribute("param", paramBuf);

    // Time
    std::string forecastTime = Fmi::to_iso_string(getValidTime());
    attributeList.addAttribute("startTime", forecastTime);
    attributeList.addAttribute("endTime", forecastTime);
    attributeList.addAttribute("timelist", forecastTime);
    attributeList.addAttribute("timezone", "UTC");
    if (origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

    queryConfigurator.configure(*gridQuery, attributeList);

    // Coordinates
    gridQuery->mAreaCoordinates.push_back(coordinates);
    gridQuery->mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;

    // Per-parameter flags
    for (auto& param : gridQuery->mQueryParameterList)
    {
      param.mLocationType = QueryServer::QueryParameter::LocationType::Point;
      param.mType = QueryServer::QueryParameter::Type::PointValues;

      if (paraminfo.geometryId)
        param.mGeometryId = *paraminfo.geometryId;

      if (paraminfo.levelId)
        param.mParameterLevelId = *paraminfo.levelId;

      if (paraminfo.level)
        param.mParameterLevel = C_INT(*paraminfo.level);
      else if (paraminfo.pressure)
      {
        param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
        param.mParameterLevel = C_INT(*paraminfo.pressure);
      }

      if (paraminfo.elevation_unit)
      {
        if (*paraminfo.elevation_unit == "m")
          param.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;
        if (*paraminfo.elevation_unit == "p")
          param.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
      }

      if (paraminfo.forecastType)
        param.mForecastType = C_INT(*paraminfo.forecastType);

      if (paraminfo.forecastNumber)
        param.mForecastNumber = C_INT(*paraminfo.forecastNumber);
    }

    gridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    gridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    if (projection.size && *projection.size > 0)
      gridQuery->mAttributeList.addAttribute("grid.size", Fmi::to_string(*projection.size));
    else
    {
      if (projection.xsize)
        gridQuery->mAttributeList.addAttribute("grid.width", Fmi::to_string(*projection.xsize));
      if (projection.ysize)
        gridQuery->mAttributeList.addAttribute("grid.height", Fmi::to_string(*projection.ysize));
    }

    return gridEngine->executeQuery(gridQuery);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Render collected arrow point values into the CDT group.
 *
 * This is the shared inner loop used by both generate_gridEngine and
 * generate_qEngine. Applies unit conversion, north correction, symbol
 * selection, scale/flip/flop, and builds the SVG transform attribute.
 */
// ----------------------------------------------------------------------

void ArrowLayer::render_arrows(CTPP::CDT& theGlobals,
                               CTPP::CDT& group_cdt,
                               const PointValues& pointvalues,
                               const Fmi::CoordinateTransformation& transformation,
                               State& theState,
                               int& valid_count)
{
  for (const auto& pointvalue : pointvalues)
  {
    const auto& point = pointvalue.point();

    // Select arrow symbol based on speed when speed-ranged arrows are configured
    bool check_speeds = (!arrows.empty() && (speed || (u && v)));

    double wspd = pointvalue[0];
    double wdir = pointvalue[1];

    if (wdir == kFloatMissing)
      continue;
    if (check_speeds && wspd == kFloatMissing)
      continue;

    ++valid_count;

    // Override data values with fixed values if requested
    if (fixedspeed)
      wspd = *fixedspeed;
    if (fixeddirection)
      wdir = *fixeddirection;

    // Unit transformation
    const double xmultiplier = multiplier.value_or(1.0);
    const double xoffset = offset.value_or(0.0);
    if (wspd != kFloatMissing)
      wspd = xmultiplier * wspd + xoffset;

    // Rotate wind direction to output coordinate system north
    auto fix = Fmi::OGR::gridNorth(transformation, point.latlon.X(), point.latlon.Y());
    if (!fix)
      continue;
    wdir = fmod(wdir + *fix, 360);

    // North wind blows toward south, rotate 180 degrees to get arrow pointing into the wind
    double rotate = fmod(wdir + 180, 360);
    int nrotate = lround(rotate);

    // Disable rotation for slow wind speeds if a limit is set
    if (wspd != kFloatMissing && minrotationspeed && wspd < *minrotationspeed)
      nrotate = 0;

    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";

    std::optional<double> rescale;
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
        iri = selection->symbol.value_or(symbol.value_or(""));
        auto scaleattr = selection->attributes.remove("scale");
        if (scaleattr)
          rescale = Fmi::stod(*scaleattr);
        theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
      }
    }

    if (iri.empty())
      continue;

    std::string IRI = Iri::normalize(iri);
    bool flop = (southflop && point.latlon.Y() < 0) || (northflop && point.latlon.Y() > 0);

    if (theState.addId(IRI))
      theGlobals["includes"][iri] = theState.getSymbol(iri);

    tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

    double yscale = (flip ? -1.0 : 1.0) * scale.value_or(1.0);
    double xscale = flop ? -yscale : yscale;
    if (rescale)
    {
      xscale *= *rescale;
      yscale *= *rescale;
    }

    int x = point.x + point.dx + dx.value_or(0);
    int y = point.y + point.dy + dy.value_or(0);

    tag_cdt["attributes"]["transform"] = make_arrow_transform(x, y, nrotate, xscale, yscale);
    group_cdt["tags"].PushBack(tag_cdt);
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
    if (paraminfo.source == std::string("grid"))
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Operation failed!", nullptr);
    exception.addParameter("qid", qid);
    if (paraminfo.producer)
      exception.addParameter("Producer", *paraminfo.producer);
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

// ----------------------------------------------------------------------
/*!
 * \brief Generate arrow layer using the grid engine
 */
// ----------------------------------------------------------------------

void ArrowLayer::generate_gridEngine(CTPP::CDT& theGlobals,
                                     CTPP::CDT& theLayersCdt,
                                     State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "ArrowLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    if (!symbol && arrows.empty())
      throw Fmi::Exception(
          BCP, "Must define arrow with 'symbol' or 'arrows' to define the symbol for arrows");

    if (!positions)
      positions = Positions{};
    positions->addMargins(xmargin, ymargin);

    // Collect parameter names for the query
    std::vector<std::string> paramList;
    if (direction)
      paramList.push_back(*direction);
    if (speed)
      paramList.push_back(*speed);
    if (u)
      paramList.push_back(*u);
    if (v)
      paramList.push_back(*v);

    auto valid_time = getValidTime();

    std::string wkt = *projection.crs;
    if (wkt != "data")
    {
      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
        wkt = projection.getCRS().WKT();

      // const auto& box = projection.getBox();
      // const auto clipbox = getClipBox(box);

      // auto bl = projection.bottomLeftLatLon();
      // auto tr = projection.topRightLatLon();

      positions->init(paraminfo.producer, projection, valid_time, theState);
    }

    // Build coordinates for the query from positions
    const auto& box = projection.getBox();
    const bool forecast_mode = true;
    auto points = positions->getPoints(nullptr, projection.getCRS(), box, forecast_mode, theState);

    T::Coordinate_vec coordinates;
    for (const auto& point : points)
      coordinates.emplace_back(point.latlon.X(), point.latlon.Y());

    auto query = build_grid_query(paramList, coordinates, theState);

    // Extract projection dimensions from query result
    if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
    {
      const char* widthStr = query->mAttributeList.getAttributeValue("grid.width");
      const char* heightStr = query->mAttributeList.getAttributeValue("grid.height");
      if (widthStr)
        projection.xsize = Fmi::stoi(widthStr);
      if (heightStr)
        projection.ysize = Fmi::stoi(heightStr);
    }

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    if (*projection.crs == "data")
    {
      const char* crsStr = query->mAttributeList.getAttributeValue("grid.crs");
      const char* proj4Str = query->mAttributeList.getAttributeValue("grid.proj4");
      if (proj4Str && strstr(proj4Str, "+lon_wrap"))
        crsStr = proj4Str;

      if (crsStr)
      {
        projection.crs = crsStr;
        if (!projection.bboxcrs)
        {
          const char* bboxStr = query->mAttributeList.getAttributeValue("grid.bbox");
          if (bboxStr)
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

    if (wkt == "data")
      return;

    positions->init(paraminfo.producer, projection, valid_time, theState);

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    addClipRect(theLayersCdt, theGlobals, box, theState);

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    theState.addAttributes(theGlobals, group_cdt, attributes);

    PointValues pointvalues =
        read_gridForecasts(*this, gridEngine, *query, direction, speed, u, v, crs, box, theState);

    pointvalues = prioritize(pointvalues, point_value_options);

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    int valid_count = 0;
    render_arrows(theGlobals, group_cdt, pointvalues, transformation, theState, valid_count);

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in arrow layer")
          .addParameter("valid values", Fmi::to_string(valid_count))
          .addParameter("minimum count", Fmi::to_string(minvalues));

    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate arrow layer using the querydata engine
 */
// ----------------------------------------------------------------------

void ArrowLayer::generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "ArrowLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    if (!symbol && arrows.empty())
      throw Fmi::Exception(
          BCP, "Must define arrow with 'symbol' or 'arrows' to define the symbol for arrows");

    bool use_observations = theState.isObservation(paraminfo.producer);
    Engine::Querydata::Q q = getModel(theState);

    if (!positions)
    {
      positions = Positions{};
      if (use_observations)
        positions->layout = Positions::Layout::Data;
    }
    positions->addMargins(xmargin, ymargin);

    if (paraminfo.level)
    {
      if (use_observations)
        throw std::runtime_error("Cannot set level value for observations in ArrowLayer");

      bool match = false;
      for (q->resetLevel(); !match && q->nextLevel();)
        match = (q->levelValue() == *paraminfo.level);
      if (!match)
        throw Fmi::Exception(
            BCP, "Level value " + Fmi::to_string(*paraminfo.level) + " is not available");
    }

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    auto valid_time = getValidTime();
    auto valid_time_period = getValidTimePeriod();

    positions->init(paraminfo.producer, projection, valid_time, theState);

    if ((u && (direction || speed)) || (v && (direction || speed)))
      throw Fmi::Exception(
          BCP, "ArrowLayer cannot define direction, speed and u/v-components simultaneously");
    if ((u && !v) || (v && !u))
      throw Fmi::Exception(BCP, "ArrowLayer must define both U- and V-components");

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    addClipRect(theLayersCdt, theGlobals, box, theState);

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";
    theState.addAttributes(theGlobals, group_cdt, attributes);

    PointValues pointvalues;

    if (!use_observations)
    {
      pointvalues = read_forecasts(*this, q, crs, box, valid_time, theState);
    }
#ifndef WITHOUT_OBSERVATION
    else
    {
      if (Layer::isFlashOrMobileProducer(*paraminfo.producer))
        throw Fmi::Exception(BCP, "Cannot use flash or mobile producer in ArrowLayer");

      bool use_uv_components = (u && v);

      if (use_uv_components)
      {
        pointvalues = ObservationReader::read(theState,
                                              {*u, *v},
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
        if (!speed)
          speed = "WindSpeedMS";  // default for arrows
        pointvalues = ObservationReader::read(theState,
                                              {*speed, *direction},
                                              *this,
                                              *positions,
                                              maxdistance_km,
                                              crs,
                                              box,
                                              valid_time,
                                              valid_time_period);
      }

      if (use_uv_components)
      {
        // Convert U/V observations to speed & direction
        for (auto& pointvalue : pointvalues)
        {
          auto uspd = pointvalue[0];
          auto vspd = pointvalue[1];

          auto result = uv_to_speed_and_direction(uspd, vspd, nullptr, 0, 0);
          if (result)
          {
            pointvalue[0] = result->speed;
            pointvalue[1] = result->direction;
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

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    int valid_count = 0;
    render_arrows(theGlobals, group_cdt, pointvalues, transformation, theState, valid_count);

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in arrow layer")
          .addParameter("valid values", Fmi::to_string(valid_count))
          .addParameter("minimum count", Fmi::to_string(minvalues));

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
  if (theState.isObservation(paraminfo.producer))
    return;

  auto add_info = [&](const std::optional<std::string>& param)
  {
    if (!param)
      return;
    ParameterInfo info(*param);
    info.producer = paraminfo.producer;
    info.level = paraminfo.level;
    add(infos, info);
  };

  add_info(direction);
  add_info(speed);
  add_info(u);
  add_info(v);
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo dispatch
 */
// ----------------------------------------------------------------------

void ArrowLayer::getFeatureInfo(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    std::cerr << "ArrowLayer::getFeatureInfo\n";

    if (!validLayer(theState))
      return;

    if (!direction && (!u || !v))
      return;

    if (theState.isObservation(paraminfo.producer))
      getObservationValue(theInfo, theState);
    else if (paraminfo.source == std::string("grid"))
      getGridValue(theInfo, theState);
    else
      getQuerydataValue(theInfo, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo for querydata
 */
// ----------------------------------------------------------------------

void ArrowLayer::getQuerydataValue(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    auto q = getModel(theState);
    if (!q || !q->isGrid())
      return;

    std::optional<Spine::Parameter> dirparam;
    std::optional<Spine::Parameter> speedparam;
    std::optional<Spine::Parameter> uparam;
    std::optional<Spine::Parameter> vparam;

    std::optional<TS::ParameterAndFunctions> speed_funcs;
    std::optional<TS::ParameterAndFunctions> dir_funcs;
    std::optional<TS::ParameterAndFunctions> u_funcs;
    std::optional<TS::ParameterAndFunctions> v_funcs;

    if (direction)
    {
      dir_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*direction);
      dirparam = dir_funcs->parameter;
    }
    if (speed)
    {
      speed_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*speed);
      speedparam = speed_funcs->parameter;
    }
    if (u)
    {
      u_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*u);
      uparam = u_funcs->parameter;
    }
    if (v)
    {
      v_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*v);
      vparam = v_funcs->parameter;
    }

    if (speedparam && !q->param(speedparam->number()))
      return;
    if (dirparam && !q->param(dirparam->number()))
      return;

    std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
    if (uparam && vparam && q->isRelativeUV())
      uvtransformation =
          std::make_shared<Fmi::CoordinateTransformation>("WGS84", q->SpatialReference());

    auto valid_time = getValidTime();

    if (!q->firstLevel())
      return;
    if (paraminfo.level && !q->selectLevel(*paraminfo.level))
      return;

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    Fmi::CoordinateTransformation transformation(crs, "WGS84");
    double lon = theInfo["x"].GetFloat();
    double lat = theInfo["y"].GetFloat();
    box.itransform(lon, lat);
    if (!transformation.transform(lon, lat))
      return;

    std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::LocalDateTime localdatetime(valid_time, Fmi::TimeZonePtr::utc);
    auto mylocale = std::locale::classic();
    NFmiPoint dummy;
    Spine::Location loc(lon, lat);

    double wdir = kFloatMissing;
    double wspd = 0;

    if (uparam && vparam)
    {
      auto up = Engine::Querydata::ParameterOptions(
          *uparam, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);
      auto vp = Engine::Querydata::ParameterOptions(
          *vparam, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);

      auto uresult = AggregationUtility::get_qengine_value(q, up, localdatetime, u_funcs);
      auto vresult = AggregationUtility::get_qengine_value(q, vp, localdatetime, v_funcs);

      const double* u_ptr = std::get_if<double>(&uresult);
      const double* v_ptr = std::get_if<double>(&vresult);
      if (u_ptr && v_ptr)
      {
        auto result = uv_to_speed_and_direction(*u_ptr, *v_ptr, uvtransformation, lon, lat);
        if (result)
        {
          wspd = result->speed;
          wdir = result->direction;
        }
      }
    }
    else
    {
      if (dir_funcs && dir_funcs->functions.innerFunction.exists())
      {
        auto dp = Engine::Querydata::ParameterOptions(
            *dirparam, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);
        auto dir_result = AggregationUtility::get_qengine_value(q, dp, localdatetime, dir_funcs);
        if (const double* ptr = std::get_if<double>(&dir_result))
          wdir = *ptr;
      }
      else
      {
        q->param(dirparam->number());
        wdir = q->interpolate(NFmiPoint(lon, lat), valid_time, 180);
      }

      if (speedparam)
      {
        if (speed_funcs && speed_funcs->functions.innerFunction.exists())
        {
          auto sp = Engine::Querydata::ParameterOptions(*speedparam,
                                                        "",
                                                        loc,
                                                        "",
                                                        "",
                                                        *timeformatter,
                                                        "",
                                                        "",
                                                        mylocale,
                                                        "",
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
          wspd = q->interpolate(NFmiPoint(lon, lat), valid_time, 180);
        }
      }
    }

    if (wdir == kFloatMissing || wspd == kFloatMissing)
      return;

    wspd = multiplier.value_or(1.0) * wspd + offset.value_or(0.0);

    theInfo["features"]["Speed"] = wspd;
    theInfo["features"]["Direction"] = wdir;
    theInfo["time"] = Fmi::to_iso_string(valid_time);
    theInfo["longitude"] = std::round(lon * 1e5) / 1e5;
    theInfo["latitude"] = std::round(lat * 1e5) / 1e5;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo for grid engine
 */
// ----------------------------------------------------------------------

void ArrowLayer::getGridValue(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      return;

    if (!direction && (!u || !v))
      return;

    // Collect parameter names
    std::vector<std::string> paramNames;
    if (direction)
      paramNames.push_back(*direction);
    if (speed)
      paramNames.push_back(*speed);
    if (u)
      paramNames.push_back(*u);
    if (v)
      paramNames.push_back(*v);

    auto valid_time = getValidTime();
    const auto& box = projection.getBox();
    auto crs = projection.getCRS();

    // Convert pixel coordinate to WGS84
    Fmi::CoordinateTransformation transformation(crs, "WGS84");
    double lon = theInfo["x"].GetFloat();
    double lat = theInfo["y"].GetFloat();
    box.itransform(lon, lat);
    if (!transformation.transform(lon, lat))
      return;

    T::Coordinate_vec coordinates;
    coordinates.emplace_back(lon, lat);

    auto query = build_grid_query(paramNames, coordinates, theState);

    // Update projection dimensions from query result if needed
    if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
    {
      const char* widthStr = query->mAttributeList.getAttributeValue("grid.width");
      const char* heightStr = query->mAttributeList.getAttributeValue("grid.height");
      if (widthStr)
        projection.xsize = Fmi::stoi(widthStr);
      if (heightStr)
        projection.ysize = Fmi::stoi(heightStr);
    }

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    // Extract the single-point results per parameter
    T::ParamValue dirValue = ParamValueMissing;
    T::ParamValue speedValue = ParamValueMissing;
    T::ParamValue uValue = ParamValueMissing;
    T::ParamValue vValue = ParamValueMissing;

    for (const auto& param : query->mQueryParameterList)
    {
      for (const auto& val : param.mValueList)
      {
        if (val->mValueList.getLength() != 1)
          continue;

        T::GridValue* rec = val->mValueList.getGridValuePtrByIndex(0);
        if (!rec)
          continue;

        if (direction && param.mParam == *direction)
          dirValue = rec->mValue;
        else if (speed && param.mParam == *speed)
          speedValue = rec->mValue;
        else if (u && param.mParam == *u)
          uValue = rec->mValue;
        else if (v && param.mParam == *v)
          vValue = rec->mValue;
      }
    }

    double wspd = ParamValueMissing;
    double wdir = ParamValueMissing;

    if (u && v)
    {
      // Check whether U/V are grid-relative and need rotating to true north
      int relativeUV = 0;
      const char* relativeUVStr =
          query->mAttributeList.getAttributeValue("grid.original.relativeUV");
      const char* originalCrs = query->mAttributeList.getAttributeValue("grid.original.crs");

      if (relativeUVStr)
        relativeUV = Fmi::stoi(relativeUVStr);

      std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
      if (relativeUV && originalCrs)
        uvtransformation = std::make_shared<Fmi::CoordinateTransformation>("WGS84", originalCrs);

      auto result = uv_to_speed_and_direction(uValue, vValue, uvtransformation, lon, lat);
      if (!result)
        return;

      wspd = result->speed;
      wdir = result->direction;
    }
    else
    {
      if (dirValue != ParamValueMissing)
        wdir = dirValue;
      if (speedValue != ParamValueMissing)
        wspd = speedValue;
    }

    if (wdir == ParamValueMissing || wspd == ParamValueMissing)
      return;

    wspd = multiplier.value_or(1.0) * wspd + offset.value_or(0.0);

    theInfo["features"]["Speed"] = wspd;
    theInfo["features"]["Direction"] = wdir;
    theInfo["time"] = Fmi::to_iso_string(valid_time);
    theInfo["longitude"] = std::round(lon * 1e5) / 1e5;
    theInfo["latitude"] = std::round(lat * 1e5) / 1e5;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo for observations
 */
// ----------------------------------------------------------------------

void ArrowLayer::getObservationValue(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    if (!speed || !direction)
      return;

    auto valid_time = getValidTime();
    auto valid_time_period = getValidTimePeriod();

    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    Fmi::CoordinateTransformation transformation(crs, "WGS84");
    double lon = theInfo["x"].GetFloat();
    double lat = theInfo["y"].GetFloat();
    box.itransform(lon, lat);
    if (!transformation.transform(lon, lat))
      return;

    Station station;
    station.longitude = lon;
    station.latitude = lat;

    Positions positions;
    positions.layout = Positions::Layout::Station;
    positions.stations.stations.emplace_back(station);

    auto pointvalues = ObservationReader::read(theState,
                                               {*speed, *direction, "fmisid"},
                                               *this,
                                               positions,
                                               maxdistance_km,
                                               crs,
                                               box,
                                               valid_time,
                                               valid_time_period);

    if (pointvalues.empty())
      return;

    const auto& values = pointvalues.front();

    double wspd = values[0];
    if (wspd == kFloatMissing)
      wspd = std::numeric_limits<double>::quiet_NaN();
    wspd = multiplier.value_or(1.0) * wspd + offset.value_or(0.0);

    double wdir = values[1];
    int fmisid = values[2];

    theInfo["time"] = Fmi::to_iso_string(valid_time);
    theInfo["longitude"] = std::round(lon * 1e5) / 1e5;
    theInfo["latitude"] = std::round(lat * 1e5) / 1e5;
    theInfo["features"]["Speed"] = wspd;
    theInfo["features"]["Direction"] = wdir;
    theInfo["features"]["Fmisid"] = fmisid;
    theInfo["features"]["URL"] = "https://hav.fmi.fi/hav/asema/?fmisid=" + std::to_string(fmisid);

    Engine::Observation::Settings settings;
    settings.maxdistance = maxdistance_km * 1000;
    settings.taggedFMISIDs.emplace_back("tag", values[1]);

    Spine::Stations stations;
    theState.getObsEngine().getStations(stations, settings);
    if (stations.size() >= 1 && stations.front().fmisid == fmisid)
      theInfo["features"]["StationName"] = stations.front().formal_name_fi;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    if (theState.isObservation(paraminfo.producer))
      return Fmi::bad_hash;

    auto hash = Layer::hash_value(theState);

    if (paraminfo.source != std::string("grid"))
    {
      auto q = getModel(theState);
      if (q)
        Fmi::hash_combine(hash, Engine::Querydata::hash_value(q));
    }

    Fmi::hash_combine(hash, countParameterHash(theState, direction));
    Fmi::hash_combine(hash, countParameterHash(theState, speed));
    Fmi::hash_combine(hash, countParameterHash(theState, u));
    Fmi::hash_combine(hash, countParameterHash(theState, v));
    Fmi::hash_combine(hash, Fmi::hash_value(fixedspeed));
    Fmi::hash_combine(hash, Fmi::hash_value(fixeddirection));
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
    Fmi::hash_combine(hash, Dali::hash_value(arrows, theState));
    Fmi::hash_combine(hash, point_value_options.hash_value());
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string ArrowLayer::generateGeoTiff(State& theState)
{
  try
  {
    // Single-parameter cases: delegate to the shared utility.
    if (!direction && !speed && !u && !v)
      return gridDataGeoTiff(*this, paraminfo.parameter, "linear", theState);
    if (direction && !speed)
      return gridDataGeoTiff(*this, *direction, "linear", theState);
    if (speed && !direction && !u && !v)
      return gridDataGeoTiff(*this, *speed, "linear", theState);

    // Two-parameter case: direction+speed  OR  u+v (converted to direction+speed).
    // Build a geometry-mode grid query for both parameters.

    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "GeoTiff output requires the grid engine to be enabled");
    if (!paraminfo.producer)
      throw Fmi::Exception(BCP, "Producer not set for GeoTiff generation");
    if (!projection.crs || *projection.crs == "data")
      throw Fmi::Exception(BCP, "GeoTiff output requires an explicit CRS");

    std::string producerName = gridEngine->getProducerName(*paraminfo.producer);
    auto crs_ref = projection.getCRS();
    const auto& box = projection.getBox();
    std::string wkt = *projection.crs;
    if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
      wkt = crs_ref.WKT();

    auto originalGridQuery = std::make_shared<QueryServer::Query>();
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    auto bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
    auto bl = projection.bottomLeftLatLon();
    auto tr = projection.topRightLatLon();
    if (projection.x1 == bl.X() && projection.y1 == bl.Y() &&
        projection.x2 == tr.X() && projection.y2 == tr.Y())
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);
    originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);

    const bool uv_mode = static_cast<bool>(u && v);
    const std::string& p1name = uv_mode ? *u : *direction;
    const std::string& p2name = uv_mode ? *v : *speed;

    auto param1 = gridEngine->getParameterString(producerName, p1name);
    auto param2 = gridEngine->getParameterString(producerName, p2name);

    if (!projection.projectionParameter)
      projection.projectionParameter = param1;

    if (param1 == p1name && originalGridQuery->mProducerNameList.empty())
    {
      gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
      if (originalGridQuery->mProducerNameList.empty())
        originalGridQuery->mProducerNameList.push_back(producerName);
    }

    attributeList.addAttribute("param", param1 + "," + param2);

    std::string forecastTime = Fmi::to_iso_string(getValidTime());
    attributeList.addAttribute("startTime", forecastTime);
    attributeList.addAttribute("endTime", forecastTime);
    attributeList.addAttribute("timelist", forecastTime);
    attributeList.addAttribute("timezone", "UTC");
    if (origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

    queryConfigurator.configure(*originalGridQuery, attributeList);

    for (auto& p : originalGridQuery->mQueryParameterList)
    {
      p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      p.mType = QueryServer::QueryParameter::Type::Vector;
      p.mFlags |= QueryServer::QueryParameter::Flags::ReturnCoordinates;
      p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Linear;
      p.mTimeInterpolationMethod = T::TimeInterpolationMethod::Linear;
      p.mLevelInterpolationMethod = T::LevelInterpolationMethod::Linear;

      if (paraminfo.geometryId)
        p.mGeometryId = *paraminfo.geometryId;
      if (paraminfo.levelId)
        p.mParameterLevelId = *paraminfo.levelId;
      if (paraminfo.level)
        p.mParameterLevel = C_INT(*paraminfo.level);
      else if (paraminfo.pressure)
      {
        p.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
        p.mParameterLevel = C_INT(*paraminfo.pressure);
      }
      if (paraminfo.elevation_unit)
      {
        if (*paraminfo.elevation_unit == "m")
          p.mFlags |= QueryServer::QueryParameter::Flags::MetricLevels;
        if (*paraminfo.elevation_unit == "p")
          p.mFlags |= QueryServer::QueryParameter::Flags::PressureLevels;
      }
      if (paraminfo.forecastType)
        p.mForecastType = C_INT(*paraminfo.forecastType);
      if (paraminfo.forecastNumber)
        p.mForecastNumber = C_INT(*paraminfo.forecastNumber);
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    if (projection.size && *projection.size > 0)
      originalGridQuery->mAttributeList.addAttribute("grid.size",
                                                      Fmi::to_string(*projection.size));
    else
    {
      if (projection.xsize)
        originalGridQuery->mAttributeList.addAttribute("grid.width",
                                                        Fmi::to_string(*projection.xsize));
      if (projection.ysize)
        originalGridQuery->mAttributeList.addAttribute("grid.height",
                                                        Fmi::to_string(*projection.ysize));
    }

    if (projection.bboxcrs)
      originalGridQuery->mAttributeList.addAttribute("grid.bboxcrs", *projection.bboxcrs);

    auto query = gridEngine->executeQuery(originalGridQuery);

    // Update projection dimensions from result if needed
    if ((projection.size && *projection.size > 0) || (!projection.xsize && !projection.ysize))
    {
      const char* widthStr = query->mAttributeList.getAttributeValue("grid.width");
      const char* heightStr = query->mAttributeList.getAttributeValue("grid.height");
      if (widthStr)
        projection.xsize = Fmi::stoi(widthStr);
      if (heightStr)
        projection.ysize = Fmi::stoi(heightStr);
    }

    if (!projection.xsize || !projection.ysize)
      throw Fmi::Exception(BCP, "Grid size is unknown after query");

    const int width = *projection.xsize;
    const int height = *projection.ysize;

    // Extract value vectors for both parameters
    std::shared_ptr<QueryServer::ParameterValues> pval1, pval2;
    const T::Coordinate_vec* coordinates = nullptr;

    for (const auto& qp : query->mQueryParameterList)
    {
      for (const auto& val : qp.mValueList)
      {
        if (!val->mValueVector.empty())
        {
          if (!pval1)
          {
            pval1 = val;
            if (!qp.mCoordinates.empty())
              coordinates = &qp.mCoordinates;
          }
          else if (!pval2)
          {
            pval2 = val;
          }
          break;
        }
      }
    }

    if (!pval1 || pval1->mValueVector.empty())
      throw Fmi::Exception(BCP, "No data returned for first parameter in GeoTiff generation");
    if (!pval2 || pval2->mValueVector.empty())
      throw Fmi::Exception(BCP, "No data returned for second parameter in GeoTiff generation");

    if (static_cast<int>(pval1->mValueVector.size()) != width * height ||
        static_cast<int>(pval2->mValueVector.size()) != width * height)
      throw Fmi::Exception(BCP, "Value vector size mismatch in GeoTiff generation");

    // Detect row order from coordinates
    const bool south_up = (coordinates &&
                           static_cast<int>(coordinates->size()) > 10 * width &&
                           (*coordinates)[0].y() < (*coordinates)[10 * width].y());

    const float nodata = static_cast<float>(ParamValueMissing);

    std::vector<float> band1(width * height);
    std::vector<float> band2(width * height);

    for (int row = 0; row < height; ++row)
    {
      const int src_row = south_up ? (height - 1 - row) : row;
      for (int col = 0; col < width; ++col)
      {
        const int src = src_row * width + col;
        const int dst = row * width + col;
        const float v1 = pval1->mValueVector[src];
        const float v2 = pval2->mValueVector[src];

        if (uv_mode)
        {
          // u/v → meteorological direction (band 1) + speed (band 2)
          if (v1 == nodata || v2 == nodata)
          {
            band1[dst] = nodata;
            band2[dst] = nodata;
          }
          else
          {
            band2[dst] = static_cast<float>(std::sqrt(double(v1) * v1 + double(v2) * v2));
            band1[dst] = static_cast<float>(
                std::fmod(180.0 + 180.0 / pi * std::atan2(double(v1), double(v2)), 360.0));
          }
        }
        else
        {
          // direction (band 1) + speed (band 2) with optional unit conversion on speed
          band1[dst] = v1;
          if (v2 == nodata)
            band2[dst] = nodata;
          else
          {
            double spd = v2;
            if (multiplier && *multiplier != 1.0)
              spd *= *multiplier;
            if (offset && *offset)
              spd += *offset;
            band2[dst] = static_cast<float>(spd);
          }
        }
      }
    }

    return writeGeoTiffBands(projection, wkt, {band1, band2});
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer::generateGeoTiff failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
