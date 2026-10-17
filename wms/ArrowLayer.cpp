//======================================================================
#include "ArrowLayer.h"
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

// The wind parameters of an arrow layer and their aggregation functions
struct WindParameters
{
  std::optional<Spine::Parameter> dirparam;
  std::optional<Spine::Parameter> speedparam;
  std::optional<Spine::Parameter> uparam;
  std::optional<Spine::Parameter> vparam;

  std::optional<TS::ParameterAndFunctions> speed_funcs;
  std::optional<TS::ParameterAndFunctions> dir_funcs;
  std::optional<TS::ParameterAndFunctions> u_funcs;
  std::optional<TS::ParameterAndFunctions> v_funcs;
};

// Parse an optional parameter name with its aggregation functions
void parse_wind_parameter(const std::optional<std::string>& name,
                          std::optional<TS::ParameterAndFunctions>& funcs,
                          std::optional<Spine::Parameter>& param)
{
  if (name)
  {
    funcs = TS::ParameterFactory::instance().parseNameAndFunctions(*name);
    param = funcs->parameter;
  }
}

// Parse the direction, speed, u and v parameters
WindParameters parse_wind_parameters(const std::optional<std::string>& direction,
                                     const std::optional<std::string>& speed,
                                     const std::optional<std::string>& u,
                                     const std::optional<std::string>& v)
{
  WindParameters params;
  parse_wind_parameter(direction, params.dir_funcs, params.dirparam);
  parse_wind_parameter(speed, params.speed_funcs, params.speedparam);
  parse_wind_parameter(u, params.u_funcs, params.uparam);
  parse_wind_parameter(v, params.v_funcs, params.vparam);
  return params;
}

// Reads point values from querydata, aggregated if functions are given. The ParameterOptions
// hold references to their arguments, hence the arguments are kept as members.
class PointValueReader
{
 public:
  PointValueReader(const Engine::Querydata::Q& theQ,
                   const NFmiMetTime& theMetTime,
                   const Fmi::LocalDateTime& theLocalTime)
      : q(theQ), met_time(theMetTime), localdatetime(theLocalTime)
  {
  }

  // Aggregated value at the location, or nullopt if the result is not a number
  std::optional<double> aggregated(const Spine::Parameter& param,
                                   const Spine::Location& loc,
                                   const std::optional<TS::ParameterAndFunctions>& funcs)
  {
    auto options = Engine::Querydata::ParameterOptions(
        param, tmp, loc, tmp, tmp, *timeformatter, tmp, tmp, mylocale, tmp, nearest, 0, dummy);
    auto result = AggregationUtility::get_qengine_value(q, options, localdatetime, funcs);
    if (const double* ptr = std::get_if<double>(&result))
      return *ptr;
    return std::nullopt;
  }

  // Aggregated value if an inner function is given, otherwise the interpolated value.
  // Returns the fallback value if the aggregated value is not a number.
  double value(const Spine::Parameter& param,
               const std::optional<TS::ParameterAndFunctions>& funcs,
               const Spine::Location& loc,
               const NFmiPoint& latlon,
               double fallback)
  {
    if (funcs && funcs->functions.innerFunction.exists())
      return aggregated(param, loc, funcs).value_or(fallback);

    q->param(param.number());
    return q->interpolate(latlon, met_time, 180);
  }

  // Wind speed and direction at the point. The speed defaults to 0 and the direction
  // to missing.
  void wind(const WindParameters& params,
            const std::shared_ptr<Fmi::CoordinateTransformation>& uvtransformation,
            const Spine::Location& loc,
            const NFmiPoint& latlon,
            double& wspd,
            double& wdir)
  {
    wdir = kFloatMissing;
    wspd = 0;

    if (params.uparam && params.vparam)
    {
      auto uresult = aggregated(*params.uparam, loc, params.u_funcs);
      auto vresult = aggregated(*params.vparam, loc, params.v_funcs);
      if (uresult && vresult)
      {
        auto result =
            uv_to_speed_and_direction(*uresult, *vresult, uvtransformation, latlon.X(), latlon.Y());
        if (result)
        {
          wspd = result->speed;
          wdir = result->direction;
        }
      }
      return;
    }

    wdir = value(*params.dirparam, params.dir_funcs, loc, latlon, wdir);

    if (params.speedparam)
      wspd = value(*params.speedparam, params.speed_funcs, loc, latlon, wspd);
  }

 private:
  const Engine::Querydata::Q& q;
  NFmiMetTime met_time;
  Fmi::LocalDateTime localdatetime;
  std::shared_ptr<Fmi::TimeFormatter> timeformatter{Fmi::TimeFormatter::create("iso")};
  const std::string tmp;
  const std::locale& mylocale = std::locale::classic();
  const bool nearest = false;
  NFmiPoint dummy;
};

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

    const auto params = parse_wind_parameters(layer.direction, layer.speed, layer.u, layer.v);
    const auto& speedparam = params.speedparam;
    const auto& dirparam = params.dirparam;

    if (speedparam && !q->param(speedparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + speedparam->name() + " not available in the arrow layer querydata");

    if (dirparam && !q->param(dirparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + dirparam->name() + " not available in the arrow layer querydata");

    // We may need to convert relative U/V components to true north
    std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
    if (params.uparam && params.vparam && q->isRelativeUV())
      uvtransformation =
          std::make_shared<Fmi::CoordinateTransformation>("WGS84", q->SpatialReference());

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode, state);

    PointValues pointvalues;

    PointValueReader reader(q, met_time, Fmi::LocalDateTime(met_time, Fmi::TimeZonePtr::utc));

    for (const auto& point : points)
    {
      if (!layer.inside(box, point.x, point.y))
        continue;

      double wdir = kFloatMissing;
      double wspd = 0;
      Spine::Location loc(point.latlon.X(), point.latlon.Y());
      reader.wind(params, uvtransformation, loc, point.latlon, wspd, wdir);

      if (wdir == kFloatMissing || wspd == kFloatMissing)
        continue;

      pointvalues.emplace_back(point, wspd, wdir);
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

// Point values from grid direction and optional speed values, the speed defaults to 10
PointValues direction_speed_values(const Positions::Points& points,
                                   T::GridValueList& dirValues,
                                   T::GridValueList* speedValues)
{
  PointValues pointvalues;
  uint len = dirValues.getLength();
  for (uint t = 0; t < len; t++)
  {
    auto point = points[t];
    T::GridValue* rec = dirValues.getGridValuePtrByIndex(t);
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
      pointvalues.emplace_back(point, wspeed, wdir);
  }
  return pointvalues;
}

// Point values from grid U and V components
PointValues uv_values(const Positions::Points& points,
                      T::GridValueList& uValues,
                      T::GridValueList& vValues,
                      const QueryServer::Query& query)
{
  PointValues pointvalues;
  int relativeUV = 0;
  const char* relativeUVStr = query.mAttributeList.getAttributeValue("grid.original.relativeUV");
  const char* originalCrs = query.mAttributeList.getAttributeValue("grid.original.crs");

  if (relativeUVStr)
    relativeUV = Fmi::stoi(relativeUVStr);

  std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
  if (relativeUV && originalCrs)
    uvtransformation = std::make_shared<Fmi::CoordinateTransformation>("WGS84", originalCrs);

  uint len = vValues.getLength();
  for (uint t = 0; t < len; t++)
  {
    auto point = points[t];
    T::GridValue* urec = uValues.getGridValuePtrByIndex(t);
    T::GridValue* vrec = vValues.getGridValuePtrByIndex(t);

    auto result = uv_to_speed_and_direction(
        urec->mValue, vrec->mValue, uvtransformation, point.latlon.X(), point.latlon.Y());

    if (result)
      pointvalues.emplace_back(point, result->speed, result->direction);
  }
  return pointvalues;
}

PointValues read_gridForecasts(const ArrowLayer& layer,
                               const Engine::Grid::Engine* gridEngine,
                               const QueryServer::Query& query,
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
      return direction_speed_values(points, *dirValues, speedValues);

    if (uValues && vValues && uValues->getLength() && vValues->getLength())
      return uv_values(points, *uValues, *vValues, query);

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read forecasts from grid engine");
  }
}

// Select the level, throws if it is not available
void select_level(const Engine::Querydata::Q& q, double level)
{
  bool match = false;
  for (q->resetLevel(); !match && q->nextLevel();)
    match = (q->levelValue() == level);
  if (!match)
    throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(level) + " is not available");
}

// MVT layer name: the qid, or the speed or direction parameter name, or "arrows"
std::string mvt_layer_name(const std::string& qid,
                           const std::optional<std::string>& speed,
                           const std::optional<std::string>& direction)
{
  std::string layerName = qid;
  if (layerName.empty())
  {
    if (speed)
      layerName = *speed;
    else if (direction)
      layerName = *direction;
    else
      layerName = "arrows";
  }
  return layerName;
}

// Add the symbol selected for the speed, or the default symbol, to MVT feature attributes
void add_symbol_attribute(std::vector<std::pair<std::string, MVTValue>>& attrs,
                          const std::vector<AttributeSelection>& arrows,
                          const std::optional<std::string>& symbol,
                          double wspd)
{
  if (!arrows.empty())
  {
    auto selection = Select::attribute(arrows, wspd);
    if (selection && selection->symbol)
      attrs.emplace_back("symbol", *selection->symbol);
  }
  else if (symbol)
  {
    attrs.emplace_back("symbol", *symbol);
  }
}

// The first two non-empty value vectors in the query result, and the coordinates of the first
void two_value_vectors(const QueryServer::Query& query,
                       std::shared_ptr<QueryServer::ParameterValues>& pval1,
                       std::shared_ptr<QueryServer::ParameterValues>& pval2,
                       const T::Coordinate_vec*& coordinates)
{
  for (const auto& qp : query.mQueryParameterList)
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
}

// Check the two value vectors are available and of the correct size
void check_two_value_vectors(const std::shared_ptr<QueryServer::ParameterValues>& pval1,
                             const std::shared_ptr<QueryServer::ParameterValues>& pval2,
                             int width,
                             int height,
                             const std::string& what)
{
  if (!pval1 || pval1->mValueVector.empty())
    throw Fmi::Exception(BCP, "No data returned for first parameter in " + what);
  if (!pval2 || pval2->mValueVector.empty())
    throw Fmi::Exception(BCP, "No data returned for second parameter in " + what);

  if (static_cast<int>(pval1->mValueVector.size()) != width * height ||
      static_cast<int>(pval2->mValueVector.size()) != width * height)
    throw Fmi::Exception(BCP, "Value vector size mismatch in " + what);
}

// Speed with optional unit conversion, missing values are kept as is
inline float converted_speed(float v2,
                             float nodata,
                             const std::optional<double>& multiplier,
                             const std::optional<double>& offset)
{
  if (v2 == nodata)
    return nodata;
  double spd = v2;
  if (multiplier && *multiplier != 1.0)
    spd *= *multiplier;
  if (offset && *offset)
    spd += *offset;
  return static_cast<float>(spd);
}

// u/v -> meteorological direction and speed
inline void uv_to_direction_speed(float v1, float v2, float nodata, float& dir, float& spd)
{
  if (v1 == nodata || v2 == nodata)
  {
    dir = nodata;
    spd = nodata;
  }
  else
  {
    spd = static_cast<float>(std::sqrt(double(v1) * v1 + double(v2) * v2));
    dir = static_cast<float>(
        std::fmod(180.0 + 180.0 / pi * std::atan2(double(v1), double(v2)), 360.0));
  }
}

// Apply the optional speed specific scale
void rescale_arrow(const std::optional<double>& rescale, double& xscale, double& yscale)
{
  if (rescale)
  {
    xscale *= *rescale;
    yscale *= *rescale;
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
    if (!point_value_options.conditional_priorities.empty())
      throw Fmi::Exception(BCP, "Conditional priorities are not supported in arrow layers");
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
    add_time_attributes(attributeList, getValidTime(), origintime);

    queryConfigurator.configure(*gridQuery, attributeList);

    // Coordinates
    gridQuery->mAreaCoordinates.push_back(coordinates);
    gridQuery->mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;

    // Per-parameter flags
    for (auto& param : gridQuery->mQueryParameterList)
    {
      param.mLocationType = QueryServer::QueryParameter::LocationType::Point;
      param.mType = QueryServer::QueryParameter::Type::PointValues;

      set_query_parameter_selections(param, paraminfo);
    }

    gridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    gridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    add_grid_size_attributes(*gridQuery, projection);

    return gridEngine->executeQuery(gridQuery);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the arrow symbol, possibly based on the wind speed
 *
 * Speed specific attributes are added to the tag, and the optional speed
 * specific scale is returned in rescale.
 */
// ----------------------------------------------------------------------

std::string ArrowLayer::selectSymbol(CTPP::CDT& theGlobals,
                                     CTPP::CDT& tag_cdt,
                                     const State& theState,
                                     bool check_speeds,
                                     double wspd,
                                     std::optional<double>& rescale) const
{
  std::string iri;

  if (!check_speeds)
  {
    if (symbol)
      iri = *symbol;
    return iri;
  }

  auto selection = Select::attribute(arrows, wspd);
  if (selection)
  {
    iri = selection->symbol.value_or(symbol.value_or(""));
    auto scaleattr = selection->attributes.remove("scale");
    if (scaleattr)
      rescale = Fmi::stod(*scaleattr);
    theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
  }
  return iri;
}

// ----------------------------------------------------------------------
/*!
 * \brief Apply fixed values and unit conversion, and calculate the arrow rotation
 *
 * Returns false if the grid north cannot be established.
 */
// ----------------------------------------------------------------------

bool ArrowLayer::arrowRotation(const Fmi::CoordinateTransformation& transformation,
                               const NFmiPoint& latlon,
                               double& wspd,
                               double& wdir,
                               int& nrotate) const
{
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
  auto fix = Fmi::OGR::gridNorth(transformation, latlon.X(), latlon.Y());
  if (!fix)
    return false;
  wdir = fmod(wdir + *fix, 360);

  // North wind blows toward south, rotate 180 degrees to get arrow pointing into the wind
  double rotate = fmod(wdir + 180, 360);
  nrotate = lround(rotate);

  // Disable rotation for slow wind speeds if a limit is set
  if (wspd != kFloatMissing && minrotationspeed && wspd < *minrotationspeed)
    nrotate = 0;
  return true;
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
                               const State& theState,
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

    int nrotate = 0;
    if (!arrowRotation(transformation, point.latlon, wspd, wdir, nrotate))
      continue;

    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";

    std::optional<double> rescale;
    std::string iri = selectSymbol(theGlobals, tag_cdt, theState, check_speeds, wspd, rescale);

    if (iri.empty())
      continue;

    std::string IRI = Iri::normalize(iri);
    bool flop = (southflop && point.latlon.Y() < 0) || (northflop && point.latlon.Y() > 0);

    if (theState.addId(IRI))
      theGlobals["includes"][iri] = theState.getSymbol(iri);

    tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

    double yscale = (flip ? -1.0 : 1.0) * scale.value_or(1.0);
    double xscale = flop ? -yscale : yscale;
    rescale_arrow(rescale, xscale, yscale);

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
                                     const State& theState)
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
    update_projection_size(projection, *query);

    if (!projection.xsize && !projection.ysize)
      throw Fmi::Exception(BCP, "The projection size is unknown!");

    update_data_projection(projection, *query);

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

void ArrowLayer::generate_qEngine(CTPP::CDT& theGlobals,
                                  CTPP::CDT& theLayersCdt,
                                  const State& theState)
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

      select_level(q, *paraminfo.level);
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

    const auto params = parse_wind_parameters(direction, speed, u, v);
    const auto& speedparam = params.speedparam;
    const auto& dirparam = params.dirparam;

    if (speedparam && !q->param(speedparam->number()))
      return;
    if (dirparam && !q->param(dirparam->number()))
      return;

    std::shared_ptr<Fmi::CoordinateTransformation> uvtransformation;
    if (params.uparam && params.vparam && q->isRelativeUV())
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

    PointValueReader reader(q, valid_time, Fmi::LocalDateTime(valid_time, Fmi::TimeZonePtr::utc));
    Spine::Location loc(lon, lat);

    double wdir = kFloatMissing;
    double wspd = 0;
    reader.wind(params, uvtransformation, loc, NFmiPoint(lon, lat), wspd, wdir);

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
    update_projection_size(projection, *query);

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
    if (!stations.empty() && stations.front().fmisid == fmisid)
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
      auto model_hash = getModelHashValue(theState);
      if (model_hash)
        Fmi::hash_combine(hash, *model_hash);
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

// ----------------------------------------------------------------------
/*!
 * \brief Query the two wind vector components (u+v or direction+speed) for the grid
 */
// ----------------------------------------------------------------------

std::shared_ptr<QueryServer::Query> ArrowLayer::queryWindVectors(
    const Engine::Grid::Engine& gridEngine, bool uv_mode, std::string& wkt)
{
  std::string producerName = gridEngine.getProducerName(*paraminfo.producer);
  auto crs_ref = projection.getCRS();
  const auto& box = projection.getBox();
  wkt = *projection.crs;
  if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
    wkt = crs_ref.WKT();

  auto originalGridQuery = std::make_shared<QueryServer::Query>();
  QueryServer::QueryConfigurator queryConfigurator;
  T::AttributeList attributeList;

  auto bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
  auto bl = projection.bottomLeftLatLon();
  auto tr = projection.topRightLatLon();
  if (projection.x1 == bl.X() && projection.y1 == bl.Y() && projection.x2 == tr.X() &&
      projection.y2 == tr.Y())
    originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);
  originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);

  const std::string& p1name = uv_mode ? *u : *direction;
  const std::string& p2name = uv_mode ? *v : *speed;

  auto param1 = gridEngine.getParameterString(producerName, p1name);
  auto param2 = gridEngine.getParameterString(producerName, p2name);

  if (!projection.projectionParameter)
    projection.projectionParameter = param1;

  if (param1 == p1name && originalGridQuery->mProducerNameList.empty())
  {
    gridEngine.getProducerNameList(producerName, originalGridQuery->mProducerNameList);
    if (originalGridQuery->mProducerNameList.empty())
      originalGridQuery->mProducerNameList.push_back(producerName);
  }

  attributeList.addAttribute("param", param1 + "," + param2);

  add_time_attributes(attributeList, getValidTime(), origintime);

  queryConfigurator.configure(*originalGridQuery, attributeList);

  for (auto& p : originalGridQuery->mQueryParameterList)
  {
    p.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
    p.mType = QueryServer::QueryParameter::Type::Vector;
    p.mFlags |= QueryServer::QueryParameter::Flags::ReturnCoordinates;
    p.mAreaInterpolationMethod = T::AreaInterpolationMethod::Linear;
    p.mTimeInterpolationMethod = T::TimeInterpolationMethod::Linear;
    p.mLevelInterpolationMethod = T::LevelInterpolationMethod::Linear;

    set_query_parameter_selections(p, paraminfo);
  }

  originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
  originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

  add_grid_size_attributes(*originalGridQuery, projection);

  if (projection.bboxcrs)
    originalGridQuery->mAttributeList.addAttribute("grid.bboxcrs", *projection.bboxcrs);

  auto query = gridEngine.executeQuery(originalGridQuery);

  // Update projection dimensions from result if needed
  update_projection_size(projection, *query);

  if (!projection.xsize || !projection.ysize)
    throw Fmi::Exception(BCP, "Grid size is unknown after query");

  return query;
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

    const bool uv_mode = (u && v);
    std::string wkt;
    auto query = queryWindVectors(*gridEngine, uv_mode, wkt);

    const int width = *projection.xsize;
    const int height = *projection.ysize;

    // Extract value vectors for both parameters
    std::shared_ptr<QueryServer::ParameterValues> pval1;
    std::shared_ptr<QueryServer::ParameterValues> pval2;
    const T::Coordinate_vec* coordinates = nullptr;
    two_value_vectors(*query, pval1, pval2, coordinates);
    check_two_value_vectors(pval1, pval2, width, height, "GeoTiff generation");

    // Detect row order from coordinates
    const bool south_up = is_south_up(coordinates, width);

    const auto nodata = static_cast<float>(ParamValueMissing);

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
          uv_to_direction_speed(v1, v2, nodata, band1[dst], band2[dst]);
        }
        else
        {
          // direction (band 1) + speed (band 2) with optional unit conversion on speed
          band1[dst] = v1;
          band2[dst] = converted_speed(v2, nodata, multiplier, offset);
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

// ----------------------------------------------------------------------
/*!
 * \brief Generate datatile PNG for wind data.
 *
 * Single-parameter cases delegate to gridDataTile().  The two-parameter
 * case (u+v or direction+speed) reuses the grid query logic from
 * generateGeoTiff() but produces a dual-band datatile PNG.
 *
 * For u+v mode the raw U and V components are encoded directly (not
 * converted to direction+speed) since the client-side particle system
 * needs the original vector components.
 */
// ----------------------------------------------------------------------

std::string ArrowLayer::generateDataTile(State& theState)
{
  try
  {
    // Single-parameter cases: delegate to the shared utility.
    if (!direction && !speed && !u && !v)
      return gridDataTile(*this, paraminfo.parameter, "linear", theState);
    if (direction && !speed)
      return gridDataTile(*this, *direction, "linear", theState);
    if (speed && !direction && !u && !v)
      return gridDataTile(*this, *speed, "linear", theState);

    // Two-parameter case: direction+speed  OR  u+v.
    // Build a geometry-mode grid query for both parameters (same as generateGeoTiff).

    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "Datatile output requires the grid engine to be enabled");
    if (!paraminfo.producer)
      throw Fmi::Exception(BCP, "Producer not set for datatile generation");
    if (!projection.crs || *projection.crs == "data")
      throw Fmi::Exception(BCP, "Datatile output requires an explicit CRS");

    const bool uv_mode = (u && v);
    std::string wkt;
    auto query = queryWindVectors(*gridEngine, uv_mode, wkt);

    const int width = *projection.xsize;
    const int height = *projection.ysize;

    // Extract value vectors for both parameters
    std::shared_ptr<QueryServer::ParameterValues> pval1;
    std::shared_ptr<QueryServer::ParameterValues> pval2;
    const T::Coordinate_vec* coordinates = nullptr;
    two_value_vectors(*query, pval1, pval2, coordinates);
    check_two_value_vectors(pval1, pval2, width, height, "datatile generation");

    // Detect row order from coordinates
    const bool south_up = is_south_up(coordinates, width);

    const auto nd = static_cast<float>(ParamValueMissing);

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
          // Encode raw U and V directly — the client needs vector components
          band1[dst] = v1;
          band2[dst] = v2;
        }
        else
        {
          // direction + speed with optional unit conversion on speed
          band1[dst] = v1;
          band2[dst] = converted_speed(v2, nd, multiplier, offset);
        }
      }
    }

    return writeDualBandDataTile(width, height, band1, band2, uv_mode ? "uv" : "dirspeed");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer::generateDataTile failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate MVT layer with point features (qEngine path only).
 *
 * Each point is emitted as a POINT feature with speed, direction, and
 * optionally symbol attributes.  PointData[0] = speed, [1] = direction,
 * matching the convention in read_forecasts().
 */
// ----------------------------------------------------------------------

void ArrowLayer::addMVTLayer(MVTTileBuilder& theBuilder, State& theState)
{
  try
  {
    if (!validLayer(theState))
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

    if (paraminfo.level)
      select_level(q, *paraminfo.level);

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

    auto& mvtLayer = theBuilder.layer(mvt_layer_name(qid, speed, direction));

    for (const auto& pointvalue : pointvalues)
    {
      if (pointvalue.size() < 2)
        continue;

      const double wspd = pointvalue[0];
      const double wdir = pointvalue[1];

      if (wspd == kFloatMissing || wdir == kFloatMissing)
        continue;

      double x = pointvalue.point().latlon.X();
      double y = pointvalue.point().latlon.Y();
      if (!xform.transform(x, y))
        continue;

      OGRPoint pt(x, y);

      std::vector<std::pair<std::string, MVTValue>> attrs;
      attrs.emplace_back("speed", wspd);
      attrs.emplace_back("direction", wdir);

      add_symbol_attribute(attrs, arrows, symbol, wspd);

      mvtLayer.addFeature(pt, attrs);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer::addMVTLayer failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
