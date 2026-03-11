// ======================================================================

#include "Layer.h"
#include "AggregationUtility.h"
#include "Config.h"
#include "Defs.h"
#include "Hash.h"
#include "JsonTools.h"
#include "LayerFactory.h"
#include "ObservationReader.h"
#include "PointData.h"
#include "State.h"
#include "View.h"
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/Cache.h>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>
#include <spine/Json.h>
#include <timeseries/ParameterFactory.h>
#include <ogr_spatialref.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
// Cache for the latlon bbox corresponding to the clipbox

const int default_cache_size = 10000;
using BBoxCache = Fmi::Cache::Cache<std::size_t, std::map<std::string, double>>;
BBoxCache g_bbox_cache(default_cache_size);

using PointValues = std::vector<PointData>;

// ----------------------------------------------------------------------
/*!
 * \brief Grid Forecast reader
 */
// ----------------------------------------------------------------------

PointValues read_gridForecasts(const Positions::Point& point,
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

    Positions::Points points;
    points.push_back(point);

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

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void Layer::init(Json::Value& theJson,
                 const State& theState,
                 const Config& theConfig,
                 const Properties& theProperties)
{
  try
  {
    Properties::init(theJson, theState, theConfig, theProperties);

    JsonTools::remove_string(qid, theJson, "qid");
    JsonTools::remove_double(minresolution, theJson, "minresolution");
    JsonTools::remove_double(maxresolution, theJson, "maxresolution");

    auto json = JsonTools::remove(theJson, "enable");
    if (!json.isNull())
      JsonTools::extract_set("enable", enable, json);

    json = JsonTools::remove(theJson, "disable");
    if (!json.isNull())
      JsonTools::extract_set("disable", disable, json);

    JsonTools::remove_string(css, theJson, "css");

    json = JsonTools::remove(theJson, "attributes");
    attributes.init(json, theConfig);

    json = JsonTools::remove(theJson, "layers");
    layers.init(json, theState, theConfig, *this);

    JsonTools::remove_string(type, theJson, "layer_type");

    JsonTools::remove_double(multiplier, theJson, "multiplier");
    JsonTools::remove_double(offset, theJson, "offset");

    // Alter units if requested
    JsonTools::remove_string(unit_conversion, theJson, "unit_conversion");
    if (!unit_conversion.empty())
    {
      auto conv = theState.getConfig().unitConversion(unit_conversion);
      multiplier = conv.multiplier;
      offset = conv.offset;
    }

    JsonTools::remove_double(maxdistance_km, theJson, "maxdistance");

    if (paraminfo.producer)
    {
      std::vector<std::string> partList;
      splitString(*paraminfo.producer, ':', partList);
      if (partList.size() == 2)
      {
        paraminfo.producer = partList[0];
        paraminfo.geometryId = toUInt32(partList[1]);
      }
    }

    const auto& request = theState.getRequest();

    std::optional<std::string> v = request.getParameter("geometryId");
    if (v)
      paraminfo.geometryId = toInt32(*v);

    // Not used in plain requests
    json = JsonTools::remove(theJson, "legend_url_layer");

    JsonTools::remove_bool(visible, theJson, "visible");
    JsonTools::remove_bool(animation_enabled, theJson, "animation_enabled");

    json = JsonTools::remove(theJson, "animation_effects");
    if (!json.isNull())
      animation_effects.init(json);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the layer generation is OK
 */
// ----------------------------------------------------------------------

bool Layer::validLayer(const State& theState) const
{
  try
  {
    if (!validResolution())
      return false;
    return validType(theState.getType());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the model data
 *
 * Note: It is not an error to request a model when the producer
 * refers to observations, we'll simply return an empty model instead.
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q Layer::getModel(const State& theState) const
{
  try
  {
    std::string model =
        (paraminfo.producer ? *paraminfo.producer : theState.getConfig().defaultModel());

    if (theState.isObservation(model))
      return {};

    if (paraminfo.source == std::string("grid"))
      return {};

    // chosen valid time is not relevant here if an origintime is set, the data may or may not
    // contain the time
    if (origintime)
      return theState.getModel(model, *origintime);

    // otherwise we try to select the minimal amount of querydata files based on the time or the
    // time interval
    if (!hasValidTime())
      return theState.getModel(model);

    return theState.getModel(model, getValidTimePeriod());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to get required model data!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the projection resolution is in the allowed range
 */
// ----------------------------------------------------------------------

bool Layer::validResolution() const
{
  try
  {
    // Valid if there are no requirements
    if (!minresolution && !maxresolution)
      return true;

    if (!projection.resolution)
      throw Fmi::Exception(
          BCP,
          "Projection resolution has not been specified, cannot use min/maxresolution settings");

    // resolution must be >= minresolution
    if (minresolution && *minresolution <= *projection.resolution)
      return false;

    // resolution must be < maxresolution
    if (maxresolution && *maxresolution > *projection.resolution)
      return false;

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void Layer::setProjection(const Projection& proj)
{
  try
  {
    projection = proj;
    layers.setProjection(proj);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Test if the set format is allowed for the layer
 *
 * The disabled setting is used first.

 */
// ----------------------------------------------------------------------

bool Layer::validType(const std::string& theType) const
{
  try
  {
    if (!enable.empty() && !disable.empty())
      throw Fmi::Exception(BCP,
                           "Setting disable and enable image formats simultaneously is an error");

    if (!disable.empty())
      return disable.find(theType) == disable.end();

    if (!enable.empty())
      return enable.find(theType) != enable.end();

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a clipping rectangle for the current layer.
 */
// ----------------------------------------------------------------------

void Layer::addClipRect(CTPP::CDT& theCdt,
                        CTPP::CDT& theGlobals,
                        const Fmi::Box& theBox,
                        const State& theState)
{
  // Generate nothing of clipping is not requested
  if (!clip)
    return;

  // Generate nothing if user has overridden the clipping path
  if (!attributes.value("clip-path").empty())
    return;

  // Unique id for the clipPath
  auto id = theState.generateUniqueId();

  // Use if for the current layer
  attributes.add("clip-path", "url(#" + id + ')');

  // Add the SVG clipPath element in front of the actual layer

  CTPP::CDT clip_cdt(CTPP::CDT::HASH_VAL);
  clip_cdt["start"] = "<clipPath";
  clip_cdt["end"] = "</clipPath>";

  Attributes clip_attributes;
  clip_attributes.add("id", id);
  theState.addAttributes(theGlobals, clip_cdt, clip_attributes);

  // The alternative would be to create an inner layer but it would be more code.
  // Note that margin settings are not for this clipRect, they are for clipping
  // of isolines number etc to be slightly larger than this clipRect so that things
  // slightly outside the layer are may still appear in the layer if the rendering
  // is large enough.

  std::string clip_rect = R"(<rect x="0" y="0" width=")" + Fmi::to_string(theBox.width()) +
                          R"(" height=")" + Fmi::to_string(theBox.height()) + R"("/>)";

  clip_cdt["cdata"] = clip_rect;

  theCdt.PushBack(clip_cdt);
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate bounding box for clipping paths
 *
 */
// ----------------------------------------------------------------------

Fmi::Box Layer::getClipBox(const Fmi::Box& theBox) const
{
  // Nothing to modify if the margings are zero
  if (xmargin == 0 && ymargin == 0)
    return theBox;

  auto w = theBox.width();
  auto h = theBox.height();

  // We need to expand the box by the amount specified for the margins

  auto xmin = theBox.xmin();
  auto xmax = theBox.xmax();
  auto ymin = theBox.ymin();
  auto ymax = theBox.ymax();

  // Expanding the clipping rect is a linear operation
  auto dx = (xmax - xmin) * xmargin / w;
  auto dy = (ymax - ymin) * ymargin / h;

  // No need to alter width and height, they are not used for geometry clipping,
  // only for projecting to pixel coordinates.

  return Fmi::Box{xmin - dx, ymin - dy, xmax + dx, ymax + dy, w, h};
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate bounding box for fetching latlon based data
 *
 * 1. expand current box by the margins
 * 2. sample the box boundaries
 * 3. project the sampled coordinates to WGS84
 * 4. establish the WGS84 bounding box from the projected coordinates
 *
 */
// ----------------------------------------------------------------------

std::map<std::string, double> Layer::getClipBoundingBox(const Fmi::Box& theBox,
                                                        const Fmi::SpatialReference& theCRS) const
{
  // Expand world coordinates by the margin settings
  const auto clipbox = getClipBox(theBox);

  // Seek the cache first
  auto hash = clipbox.hashValue();
  Fmi::hash_combine(hash, theCRS.hashValue());

  const auto& obj = g_bbox_cache.find(hash);
  if (obj)
    return *obj;

  // Observations are in WGS84 coordinates

  Fmi::CoordinateTransformation transformation(theCRS, "WGS84");

  // Establish the number of samples along direction. We wish to sample at roughly 5 pixel
  // intervals.

  const std::size_t npixels = 5;
  const std::size_t minsamples = 2;
  const std::size_t w = theBox.width() + 2U * xmargin;
  const std::size_t h = theBox.height() + 2U * ymargin;

  // If the CRS is geographic we need only the corners

  std::size_t wsamples = minsamples;
  std::size_t hsamples = minsamples;

  // Otherwise we need to sample the full image since the poles may be inside the image
  if (!theCRS.isGeographic())
  {
    wsamples = std::max(wsamples, w / npixels);
    hsamples = std::max(hsamples, h / npixels);
  }

  // Sample the image in world coordinates
  std::vector<double> x;
  std::vector<double> y;

  x.reserve(wsamples * hsamples);
  y.reserve(wsamples * hsamples);

  const double dx = (clipbox.xmax() - clipbox.xmin()) / (wsamples - 1);
  const double dy = (clipbox.ymax() - clipbox.ymin()) / (hsamples - 1);
  const double x0 = clipbox.xmin();
  const double y0 = clipbox.ymin();

  for (auto i = 0UL; i < wsamples; i++)
  {
    auto px = x0 + i * dx;
    for (auto j = 0UL; j < hsamples; j++)
    {
      auto py = y0 + j * dy;
      x.push_back(px);
      y.push_back(py);
    }
  }

  // Project all coordinates at once

  if (!theCRS.isGeographic())
    if (!transformation.transform(x, y))
      return {};

  double minlat = 0;
  double minlon = 0;
  double maxlat = 0;
  double maxlon = 0;

  bool lon_missing = true;
  for (auto lon : x)
  {
    if (!std::isnan(lon))
    {
      minlon = (lon_missing ? lon : std::min(minlon, lon));
      maxlon = (lon_missing ? lon : std::max(maxlon, lon));
      lon_missing = false;
    }
  }

  bool lat_missing = true;
  for (auto lat : y)
  {
    if (!std::isnan(lat))
    {
      minlat = (lat_missing ? lat : std::min(minlon, lat));
      maxlat = (lat_missing ? lat : std::max(maxlon, lat));
      lat_missing = false;
    }
  }

  if (lon_missing || lat_missing)
    return {};

  // We may miss the poles since the sample step is 5 pixels. Include the poles explicitly when it
  // seems to be close enough.
  if (minlat < -89)
    minlat = -90;
  if (maxlat > 89)
    maxlat = 90;

  // Build the result in the form wanted by obsengine
  std::map<std::string, double> ret;
  ret["minx"] = minlon;
  ret["maxx"] = maxlon;
  ret["miny"] = minlat;
  ret["maxy"] = maxlat;

  g_bbox_cache.insert(hash, ret);

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish the parameter for the layer
 *
 * This method is not used by layers which require no specific
 * parameter. The layers whic do require one call this and get an
 * exception if no parameter has been specified.
 */
// ----------------------------------------------------------------------

#if 0
Parameter Layer::getParameter() const
{
  if(!parameter)
	throw Fmi::Exception(BCP,"Parameter has not been set for all layers");
  return Spine::ParameterFactory::instance().parse(*parameter);
}

#endif

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void Layer::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  // By default a layer uses no parameters
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate warnings if needed
 */
// ----------------------------------------------------------------------

void Layer::check_warnings(Warnings& warnings) const
{
  try
  {
    if (!qid.empty())
      ++warnings.qid_counts[qid];
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data value for the given pixel or NaN
 */
// ----------------------------------------------------------------------

void Layer::getFeatureValue(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
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
 * \brief Get data value for the given pixel or NaN
 */
// ----------------------------------------------------------------------

void Layer::getQuerydataValue(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    // The code here is a drastically reduced version of the respective GetMap code

    // Establish the data
    auto q = getModel(theState);
    if (!q)
      return;
    if (!q->isGrid())
      return;

    // Extract optional parameter aggregation settings
    std::optional<TS::ParameterAndFunctions> param_funcs;
    param_funcs = TS::ParameterFactory::instance().parseNameAndFunctions(paraminfo.parameter);

    // Establish the valid time
    auto valid_time = getValidTime();

    // Establish the level
    if (!q->firstLevel())
      return;
    if (paraminfo.level)
    {
      if (!q->selectLevel(*paraminfo.level))
        return;
    }

    // Get projection details
    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Pixel coordinate to latlon
    Fmi::CoordinateTransformation transformation(crs, "WGS84");
    double x = theInfo["x"].GetFloat();
    double y = theInfo["y"].GetFloat();
    double lon = x;
    double lat = y;
    box.itransform(lon, lat);
    if (!transformation.transform(lon, lat))
      return;

    // Q API sucks (copy paste this everywhere)

    std::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    Fmi::LocalDateTime localdatetime(valid_time, Fmi::TimeZonePtr::utc);
    auto mylocale = std::locale::classic();
    NFmiPoint dummy;

    Spine::Location loc(lon, lat);
    Engine::Querydata::ParameterOptions options(param_funcs->parameter,
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

    TS::Value result =
        AggregationUtility::get_qengine_value(q, options, localdatetime, param_funcs);

    // Extract variant content
    double value = std::numeric_limits<double>::quiet_NaN();
    if (const double* tmp = std::get_if<double>(&result))
      value = *tmp;
    if (const int* ptr = std::get_if<int>(&result))
      value = *ptr;

    const double xmul = (multiplier ? *multiplier : 1.0);
    const double xoff = (offset ? *offset : 0.0);
    value = xmul * value + xoff;

    theInfo["features"][paraminfo.parameter] = value;
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
 * \brief Get data value for the given pixel or NaN
 */
// ----------------------------------------------------------------------

void Layer::getGridValue(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      return;

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*paraminfo.producer);

    auto valid_time = getValidTime();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time;

    std::string wkt = *projection.crs;
    // std::cout << wkt << "\n";

    // Getting WKT and the bounding box of the requested projection.
    // WMS does not support CRS=data, no need to handle it

    if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
      wkt = projection.getCRS().WKT();

    // std::cout << wkt << "\n";

    // Adding the bounding box information into the query.

    const auto& box = projection.getBox();

    auto bl = projection.bottomLeftLatLon();
    auto tr = projection.topRightLatLon();
    auto bbox = fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y());
    originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

    bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
    originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);

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

    originalGridQuery->mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;

    // Requested point: convert from pixel coordinate to latlon

    auto crs = projection.getCRS();

    Fmi::CoordinateTransformation transformation(crs, "WGS84");
    double x = theInfo["x"].GetFloat();
    double y = theInfo["y"].GetFloat();

    double lon = x;
    double lat = y;
    box.itransform(lon, lat);
    if (!transformation.transform(lon, lat))
      return;

    T::Coordinate_vec coordinates;
    coordinates.emplace_back(lon, lat);
    originalGridQuery->mAreaCoordinates.push_back(coordinates);
    originalGridQuery->mFlags |= QueryServer::Query::Flags::GeometryHitNotRequired;

    for (auto& param : originalGridQuery->mQueryParameterList)
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

    // Establish the numbers

    Positions::Point point(x, y, {lon, lat});

    auto pointvalues =
        read_gridForecasts(point, gridEngine, *originalGridQuery, crs, box, valid_time, theState);

    if (pointvalues.size() >= 1)
    {
      float value = pointvalues.front()[0];

      const double xmul = (multiplier ? *multiplier : 1.0);
      const double xoff = (offset ? *offset : 0.0);

      if (value == kFloatMissing)
        value = std::numeric_limits<double>::quiet_NaN();
      value = xmul * value + xoff;

      // Output results
      theInfo["time"] = Fmi::to_iso_string(valid_time);
      theInfo["features"][paraminfo.parameter] = value;
      theInfo["longitude"] = std::round(lon * 1e5) / 1e5;
      theInfo["latitude"] = std::round(lat * 1e5) / 1e5;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get data value for the given pixel or NaN
 */
// ----------------------------------------------------------------------

void Layer::getObservationValue(CTPP::CDT& theInfo, const State& theState)
{
  try
  {
    // Establish the valid time
    auto valid_time = getValidTime();
    auto valid_time_period = getValidTimePeriod();

    // Get projection details

    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Pixel coordinate to latlon
    Fmi::CoordinateTransformation transformation(crs, "WGS84");
    double x = theInfo["x"].GetFloat();
    double y = theInfo["y"].GetFloat();
    double lon = x;
    double lat = y;
    box.itransform(lon, lat);
    if (!transformation.transform(lon, lat))
      return;

    // Create list of stations to read
    Station station;
    station.longitude = lon;
    station.latitude = lat;

    Positions positions;
    positions.layout = Positions::Layout::Station;
    positions.stations.stations.emplace_back(station);

    // Fetch observations for the single coordinate

    std::vector<std::string> params{paraminfo.parameter, "fmisid"};

    auto pointvalues = ObservationReader::read(theState,
                                               params,
                                               *this,
                                               positions,
                                               maxdistance_km,
                                               crs,
                                               box,
                                               valid_time,
                                               valid_time_period);

    if (pointvalues.size() >= 1)
    {
      const auto& values = pointvalues.front();

      const double xmul = (multiplier ? *multiplier : 1.0);
      const double xoff = (offset ? *offset : 0.0);

      double value = values[0];
      if (value == kFloatMissing)
        value = std::numeric_limits<double>::quiet_NaN();
      value = xmul * value + xoff;
      int fmisid = values[1];

      // Output results
      theInfo["time"] = Fmi::to_iso_string(valid_time);
      theInfo["longitude"] = std::round(lon * 1e5) / 1e5;
      theInfo["latitude"] = std::round(lat * 1e5) / 1e5;
      theInfo["features"][paraminfo.parameter] = value;
      theInfo["features"]["Fmisid"] = fmisid;
      theInfo["features"]["URL"] = "https://hav.fmi.fi/hav/asema/?fmisid=" + std::to_string(fmisid);

      // Add more station information
      Engine::Observation::Settings settings;
      settings.maxdistance = maxdistance_km * 1000;
      settings.taggedFMISIDs.emplace_back("tag", values[1]);

      Spine::Stations stations;
      theState.getObsEngine().getStations(stations, settings);
      if (stations.size() >= 1 && stations.front().fmisid == fmisid)
      {
        const auto& s = stations.front();
        theInfo["features"]["StationName"] = s.formal_name_fi;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the hash for the layer
 */
// ----------------------------------------------------------------------

std::size_t Layer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Properties::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(qid));
    Fmi::hash_combine(hash, Fmi::hash_value(minresolution));
    Fmi::hash_combine(hash, Fmi::hash_value(maxresolution));
    Fmi::hash_combine(hash, Fmi::hash_value(enable));
    Fmi::hash_combine(hash, Fmi::hash_value(disable));
    Fmi::hash_combine(hash, Fmi::hash_value(maxdistance_km));
    Fmi::hash_combine(hash, Fmi::hash_value(unit_conversion));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    Fmi::hash_combine(hash, Dali::hash_css(css, theState));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(layers, theState));
    Fmi::hash_combine(hash, Fmi::hash_value(visible));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

bool Layer::isFlashOrMobileProducer(const std::string& producer)
{
  return (producer == "flash" || producer == "roadcloud" || producer == "netatmo");
}

bool Layer::isAnimationStepVisible(const State& theState) const
{
  try
  {
    if (theState.animation_enabled)
    {
      for (auto it = animation_effects.effects.begin(); it != animation_effects.effects.end(); ++it)
      {
        if (it->effect == "hide" && theState.animation_loopstep >= it->start_step &&
            theState.animation_loopstep <= it->end_step)
          return false;
      }
    }

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
