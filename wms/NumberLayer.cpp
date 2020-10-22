//======================================================================

#include <spine/TimeSeriesOutput.h>

#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "Layer.h"
#include "NumberLayer.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"
#include <engines/gis/Engine.h>
#include <macgyver/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <spine/ParameterTools.h>
#include <spine/ValueFormatter.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/grid/Engine.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <iomanip>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct PointValue
{
  Positions::Point point;
  double value;
};

using PointValues = std::vector<PointValue>;

// ----------------------------------------------------------------------
/*!
 * \brief Forecast reader
 */
// ----------------------------------------------------------------------

PointValues read_forecasts(const NumberLayer& layer,
                           const Engine::Querydata::Q& q,
                           const std::shared_ptr<OGRSpatialReference>& crs,
                           const Fmi::Box& box,
                           const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    // Generate the coordinates for the numbers

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

    // The parameters. This *must* be done after the call to positions generation

    auto param = Spine::ParameterFactory::instance().parse(*layer.parameter);

    boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
    boost::local_time::local_date_time localdatetime(valid_time_period.begin(), utc);

    PointValues pointvalues;

    auto mylocale = std::locale::classic();
    NFmiPoint dummy;

    for (const auto& point : points)
    {
      if (layer.inside(box, point.x, point.y))
      {
        Spine::Location loc(point.latlon.X(), point.latlon.Y());

        // Q API SUCKS!!
        Engine::Querydata::ParameterOptions options(
            param, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);

        auto result = q->value(options, localdatetime);
        if (boost::get<double>(&result) != nullptr)
        {
          double tmp = *boost::get<double>(&result);
          pointvalues.push_back(PointValue{point, tmp});
          // printf("Point %d,%d  => %f,%f  = %f\n",point.x,point.y,point.latlon.X(),
          // point.latlon.Y(),tmp);
        }
        else if (boost::get<int>(&result) != nullptr)
        {
          double tmp = *boost::get<int>(&result);
          pointvalues.push_back(PointValue{point, tmp});
          // printf("Point %d,%d  => %f,%f  = %f\n",point.x,point.y,point.latlon.X(),
          // point.latlon.Y(),tmp);
        }
        else
        {
          PointValue missingvalue{point, kFloatMissing};
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
                               const Engine::Grid::Engine* gridEngine,
                               QueryServer::Query& query,
                               const std::shared_ptr<OGRSpatialReference>& crs,
                               const Fmi::Box& box,
                               const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    // Generate the coordinates for the numbers

    PointValues pointvalues;

    int width = 0;
    int height = 0;
    int originalWidth = 0;
    int originalHeight = 0;

    const char* widthStr = query.mAttributeList.getAttributeValue("grid.width");
    const char* heightStr = query.mAttributeList.getAttributeValue("grid.height");
    const char* originalCrs = query.mAttributeList.getAttributeValue("grid.original.crs");
    const char* originalWidthStr = query.mAttributeList.getAttributeValue("grid.original.width");
    const char* originalHeightStr = query.mAttributeList.getAttributeValue("grid.original.height");

    if (widthStr)
      width = atoi(widthStr);

    if (heightStr)
      height = atoi(heightStr);

    if (originalWidthStr)
      originalWidth = atoi(originalWidthStr);

    if (originalHeightStr)
      originalHeight = atoi(originalHeightStr);

    T::ParamValue_vec* values = nullptr;
    uint originalGeometryId = 0;

    for (auto param = query.mQueryParameterList.begin(); param != query.mQueryParameterList.end();
         ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        if ((*val)->mValueVector.size() > 0)
        {
          originalGeometryId = (*val)->mGeometryId;
          values = &(*val)->mValueVector;
        }
      }
    }

    auto points = layer.positions->getPoints(
        originalCrs, originalWidth, originalHeight, originalGeometryId, crs, box);

    if (values && values->size() > 0)
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
              pointvalues.push_back(PointValue{point, tmp});
            }
            else
            {
              PointValue missingvalue{point, kFloatMissing};
              pointvalues.push_back(missingvalue);
            }
            // printf("Point %d,%d  => %f,%f  = %f\n",point.x,point.y,point.latlon.X(),
            // point.latlon.Y(),tmp);
          }
          else
          {
            PointValue missingvalue{point, kFloatMissing};
            pointvalues.push_back(missingvalue);
          }
        }
        else
        {
          // printf("Not inside %d,%d  => %f,%f  = %ld,%ld\n",point.x,point.y,point.latlon.X(),
          // point.latlon.Y(),box.width(),box.height());
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
 * \brief Observation reader
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
PointValues read_flash_observations(const NumberLayer& layer,
                                    State& state,
                                    const std::shared_ptr<OGRSpatialReference>& crs,
                                    const Fmi::Box& box,
                                    const boost::posix_time::time_period& valid_time_period,
                                    OGRCoordinateTransformation& transformation)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.latest = false;
    settings.timezone = "UTC";
    settings.numberofstations = 1;

    settings.timestep = 0;

    settings.starttimeGiven = true;

    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();
    settings.parameters.push_back(Spine::makeParameter("longitude"));
    settings.parameters.push_back(Spine::makeParameter("latitude"));

    if (layer.parameter)
      settings.parameters.push_back(Spine::makeParameter(*layer.parameter));

    // Request intersection parameters too - if any
    auto iparams = layer.positions->intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(Spine::makeParameter(extraparam));

    // Generate the coordinates for the symbols

    Engine::Querydata::Q q;
    const bool forecast_mode = false;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

    Engine::Observation::StationSettings stationSettings;
    stationSettings.bounding_box_settings = layer.getClipBoundingBox(box, state, crs);
    settings.taggedFMISIDs = obsengine.translateToFMISID(
        settings.starttime, settings.endtime, settings.stationtype, stationSettings);

    auto result = obsengine.values(settings);

    // Build the pointvalues

    if (!result)
      return {};

    const auto& values = *result;
    if (values.empty())
      return {};

    // Accept all flashes for the time period

    // We accept only the newest observation for each interval (except for flashes)
    // obsengine returns the data sorted by fmisid and by time

    PointValues pointvalues;

    for (std::size_t row = 0; row < values[0].size(); ++row)
    {
      double lon = get_double(values.at(0).at(row));
      double lat = get_double(values.at(1).at(row));
      double value = kFloatMissing;

      if (layer.parameter)
        value = get_double(values.at(2).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (crs->IsGeographic() == 0)
        if (transformation.Transform(1, &x, &y) == 0)
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!layer.inside(box, x, y))
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!layer.positions->inside(lon, lat, ivalues))
        continue;

      int xpos = lround(x);
      int ypos = lround(y);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
      PointValue pv{point, value};
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer failed to read observations from the database");
  }
}

PointValues read_all_observations(const NumberLayer& layer,
                                  State& state,
                                  const std::shared_ptr<OGRSpatialReference>& crs,
                                  const Fmi::Box& box,
                                  const boost::posix_time::time_period& valid_time_period,
                                  OGRCoordinateTransformation& transformation)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.latest = true;
    settings.timezone = "UTC";
    settings.numberofstations = 1;
    settings.maxdistance = layer.maxdistance * 1000;  // obsengine uses meters

    // settings.timestep = ?;

    settings.starttimeGiven = true;

    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();
    settings.parameters.push_back(Spine::makeParameter("stationlon"));
    settings.parameters.push_back(Spine::makeParameter("stationlat"));

    if (layer.parameter)
      settings.parameters.push_back(Spine::makeParameter(*layer.parameter));

    // Request intersection parameters too - if any
    auto iparams = layer.positions->intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(Spine::makeParameter(extraparam));

    // Coordinates or bounding box
    Engine::Observation::StationSettings stationSettings;
    stationSettings.bounding_box_settings = layer.getClipBoundingBox(box, state, crs);
    settings.taggedFMISIDs = obsengine.translateToFMISID(
        settings.starttime, settings.endtime, settings.stationtype, stationSettings);

    auto result = obsengine.values(settings);

    // Build the pointvalues

    if (!result)
      return {};

    const auto& values = *result;
    if (values.empty())
      return {};

    // We accept only the newest observation for each interval
    // obsengine returns the data sorted by fmisid and by time

    PointValues pointvalues;

    for (std::size_t row = 0; row < values[0].size(); ++row)
    {
      double lon = get_double(values.at(0).at(row));
      double lat = get_double(values.at(1).at(row));
      double value = kFloatMissing;

      if (layer.parameter)
        value = get_double(values.at(2).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (crs->IsGeographic() == 0)
        if (transformation.Transform(1, &x, &y) == 0)
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!layer.inside(box, x, y))
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!layer.positions->inside(lon, lat, ivalues))
        continue;

      int xpos = lround(x);
      int ypos = lround(y);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
      PointValue pv{point, value};
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer failed to read observations from the database");
  }
}

PointValues read_station_observations(const NumberLayer& layer,
                                      State& state,
                                      const std::shared_ptr<OGRSpatialReference>& crs,
                                      const Fmi::Box& box,
                                      const boost::posix_time::time_period& valid_time_period,
                                      OGRCoordinateTransformation& transformation)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.latest = true;
    settings.timezone = "UTC";
    settings.numberofstations = 1;
    settings.maxdistance = layer.maxdistance * 1000;  // obsengine uses meters

    // settings.timestep = ?;

    settings.starttimeGiven = true;

    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();
    settings.parameters.push_back(Spine::makeParameter("stationlon"));
    settings.parameters.push_back(Spine::makeParameter("stationlat"));

    if (layer.parameter)
      settings.parameters.push_back(Spine::makeParameter(*layer.parameter));

    // Request intersection parameters too - if any
    auto iparams = layer.positions->intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(Spine::makeParameter(extraparam));

    if (!layer.positions)
      throw Fmi::Exception(BCP, "Positions not defined for station-layout of numbers");

    // We must read the stations one at a time to preserve dx,dy values
    PointValues pointvalues;

    for (const auto& station : layer.positions->stations.stations)
    {
      // Copy Oracle settings
      auto opts = settings;

      Engine::Observation::StationSettings stationSettings;

      // Use an unique ID first if specified, ignoring the coordinates even if set
      if (station.fmisid)
        stationSettings.fmisids.push_back(*station.fmisid);
      else if (station.wmo)
        stationSettings.wmos.push_back(*station.wmo);
      else if (station.lpnn)
        stationSettings.lpnns.push_back(*station.lpnn);
      else if (station.geoid)
      {
        stationSettings.geoid_settings.geoids.push_back(*station.geoid);
        stationSettings.geoid_settings.maxdistance = opts.maxdistance;
        stationSettings.geoid_settings.numberofstations = opts.numberofstations;
        stationSettings.geoid_settings.language = opts.language;
      }
      else if (station.longitude && station.latitude)
      {
        stationSettings.nearest_station_settings.emplace_back(
            *station.longitude, *station.latitude, opts.maxdistance, opts.numberofstations, "");
      }
      else
        throw Fmi::Exception(BCP, "Station ID or coordinate missing");

      opts.taggedFMISIDs = obsengine.translateToFMISID(
          settings.starttime, settings.endtime, settings.stationtype, stationSettings);

      auto result = obsengine.values(opts);

      if (!result || result->empty() || (*result)[0].empty())
        continue;

      const auto& values = *result;

      // We accept only the newest observation for each interval
      // obsengine returns the data sorted by fmisid and by time,

      const int row = 0;
      double lon = get_double(values.at(0).at(row));
      double lat = get_double(values.at(1).at(row));
      double value = kFloatMissing;

      if (layer.parameter)
        value = get_double(values.at(2).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (crs->IsGeographic() == 0)
        if (transformation.Transform(1, &x, &y) == 0)
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!layer.inside(box, x, y))
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!layer.positions->inside(lon, lat, ivalues))
        continue;

      int xpos = lround(x);
      int ypos = lround(y);

      // Keep only the latest value for each coordinate

      int deltax = (station.dx ? *station.dx : 0);
      int deltay = (station.dy ? *station.dy : 0);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat), deltax, deltay};
      PointValue pv{point, value};
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer failed to read observations from the database");
  }
}

PointValues read_latlon_observations(const NumberLayer& layer,
                                     State& state,
                                     const std::shared_ptr<OGRSpatialReference>& crs,
                                     const Fmi::Box& box,
                                     const boost::posix_time::time_period& valid_time_period,
                                     OGRCoordinateTransformation& transformation,
                                     const Positions::Points& points)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.timezone = "UTC";
    settings.numberofstations = 1;                    // we need only the nearest station
    settings.latest = true;                           // we need only the newest observation
    settings.maxdistance = layer.maxdistance * 1000;  // obsengine uses meters

    settings.starttimeGiven = true;

    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();
    settings.parameters.push_back(Spine::makeParameter("stationlon"));
    settings.parameters.push_back(Spine::makeParameter("stationlat"));
    settings.parameters.push_back(Spine::makeParameter("fmisid"));

    if (layer.parameter)
      settings.parameters.push_back(Spine::makeParameter(*layer.parameter));

    // Request intersection parameters too - if any
    auto iparams = layer.positions->intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(Spine::makeParameter(extraparam));

    // Process the points one at a time so that we can assign dx,dy values to them

    PointValues pointvalues;

    // We do not use the same station twice
    std::set<int> used_fmisids;

    for (const auto& point : points)
    {
      // Copy common Oracle settings
      auto opts = settings;

      Engine::Observation::StationSettings stationSettings;
      stationSettings.nearest_station_settings.emplace_back(
          point.latlon.X(), point.latlon.Y(), opts.maxdistance, opts.numberofstations, "");

      opts.taggedFMISIDs = obsengine.translateToFMISID(
          settings.starttime, settings.endtime, settings.stationtype, stationSettings);

      auto result = obsengine.values(opts);

      if (!result || result->empty() || (*result)[0].empty())
        continue;

      const auto& values = *result;

      // We accept only the newest observation for each interval
      // obsengine returns the data sorted by fmisid and by time

      const int row = 0;

      double lon = get_double(values.at(0).at(row));
      double lat = get_double(values.at(1).at(row));
      int fmisid = get_fmisid(values.at(2).at(row));
      double value = kFloatMissing;

      if (used_fmisids.find(fmisid) != used_fmisids.end())
        continue;
      used_fmisids.insert(fmisid);

      if (layer.parameter)
        value = get_double(values.at(3).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (crs->IsGeographic() == 0)
        if (transformation.Transform(1, &x, &y) == 0)
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!layer.inside(box, x, y))
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!layer.positions->inside(lon, lat, ivalues))
        continue;

      int xpos = lround(x);
      int ypos = lround(y);

      Positions::Point pp{xpos, ypos, NFmiPoint(lon, lat), point.dx, point.dy};
      PointValue pv{pp, value};
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer failed to read observations from the database");
  }
}

PointValues read_observations(const NumberLayer& layer,
                              State& state,
                              const std::shared_ptr<OGRSpatialReference>& crs,
                              const Fmi::Box& box,
                              const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    // Create the coordinate transformation from image world coordinates
    // to WGS84 coordinates

    auto obscrs = state.getGisEngine().getSpatialReference("WGS84");

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(obscrs.get(), crs.get()));
    if (transformation == nullptr)
      throw Fmi::Exception(
          BCP, "Failed to create the needed coordinate transformation when drawing symbols");

    if (layer.isFlashOrMobileProducer(*layer.producer))
      return read_flash_observations(layer, state, crs, box, valid_time_period, *transformation);

    if (layer.positions->layout == Positions::Layout::Station)
      return read_station_observations(layer, state, crs, box, valid_time_period, *transformation);

    Engine::Querydata::Q q;
    const bool forecast_mode = false;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

    if (!points.empty())
      return read_latlon_observations(
          layer, state, crs, box, valid_time_period, *transformation, points);

    return read_all_observations(layer, state, crs, box, valid_time_period, *transformation);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "NumberLayer failed to read observations from the database");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void NumberLayer::init(const Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Number-layer JSON is not a JSON object");

    // Iterate through all the members

    Layer::init(theJson, theState, theConfig, theProperties);

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

    json = theJson.get("unit_conversion", nulljson);
    if (!json.isNull())
      unit_conversion = json.asString();

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();

    json = theJson.get("positions", nulljson);
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }

    json = theJson.get("maxdistance", nulljson);
    if (!json.isNull())
      maxdistance = json.asDouble();

    json = theJson.get("minvalues", nulljson);
    if (!json.isNull())
      minvalues = json.asInt();

    json = theJson.get("label", nulljson);
    if (!json.isNull())
      label.init(json, theConfig);

    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      symbol = json.asString();

    json = theJson.get("scale", nulljson);
    if (!json.isNull())
      scale = json.asDouble();

    json = theJson.get("numbers", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("numbers", numbers, json, theConfig);
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

    if (source && *source == "grid")
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    Fmi::Exception exception(BCP, "Operation failed!", nullptr);
    exception.addParameter("Producer", *producer);
    exception.addParameter("Parameter", *parameter);
    throw exception;
  }
}

void NumberLayer::generate_gridEngine(CTPP::CDT& theGlobals,
                                      CTPP::CDT& theLayersCdt,
                                      State& theState)
{
  try
  {
    // Time execution

    std::string report = "NumberLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // Make sure position generation is initialized

    if (!positions)
      positions = Positions{};

    // Add layer margins to position generation
    positions->addMargins(xmargin, ymargin);

    // Establish the parameter

    if (!parameter)
      throw Fmi::Exception(BCP, "Parameter not set for number-layer");

    auto gridEngine = theState.getGridEngine();
    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;
    auto valid_time_period = getValidTimePeriod();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time_period.begin();

    std::string wkt = *projection.crs;
    // std::cout << wkt << "\n";

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      auto crs = projection.getCRS();
      char* out = nullptr;
      crs->exportToWkt(&out);
      wkt = out;
      OGRFree(out);

      // std::cout << wkt << "\n";

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

    std::string param = gridEngine->getParameterString(*producer, pName);
    attributeList.addAttribute("param", param);

    if (!projection.projectionParameter)
      projection.projectionParameter = param;

    if (param == *parameter && originalGridQuery->mProducerNameList.size() == 0)
    {
      gridEngine->getProducerNameList(*producer,originalGridQuery->mProducerNameList);
      if (originalGridQuery->mProducerNameList.size() == 0)
        originalGridQuery->mProducerNameList.push_back(*producer);
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
      it->mType = QueryServer::QueryParameter::Type::Vector;
      it->mFlags = QueryServer::QueryParameter::Flags::ReturnCoordinates;

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

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    // query.print(std::cout,0,0);

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

    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    if (wkt == "data")
      return;

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time_period.begin(), theState);

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

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    PointValues pointvalues;
    pointvalues = read_gridForecasts(*this, gridEngine, *originalGridQuery, crs, box, valid_time_period);

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

    // Symbols first

    for (const auto& pointvalue : pointvalues)
    {
      // Start generating the hash

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      const auto& point = pointvalue.point;
      float value = pointvalue.value;

      if (value != kFloatMissing)
        value = xmultiplier * value + xoffset;

      std::string iri;
      if (symbol)
        iri = *symbol;

      auto selection = Select::attribute(numbers, value);
      if (selection && selection->symbol)
        iri = *selection->symbol;

      // librsvg cannot handle scale + transform, must move former into latter
      boost::optional<double> rescale;
      if (selection)
      {
        auto scaleattr = selection->attributes.remove("scale");
        if (scaleattr)
          rescale = Fmi::stod(*scaleattr);
      }

      if (!iri.empty())
      {
        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        // Lack of CSS3 transform support forces us to use a direct transformation
        // which may override user settings
        tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

        std::string tmp = fmt::sprintf("translate(%d,%d)", point.x, point.y);

        double newscale = (scale ? *scale : 1.0) * (rescale ? *rescale : 1.0);
        if (newscale != 1.0)
          tmp += fmt::sprintf(" scale(%g)", newscale);

        tag_cdt["attributes"]["transform"] = tmp;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }
    theLayersCdt.PushBack(group_cdt);

    // Then numbers

    int valid_count = 0;
    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point;
      float value = pointvalue.value;

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
        text_cdt["cdata"] = txt;

        auto selection = Select::attribute(numbers, value);

        if (selection)
          selection->attributes.remove("scale");

        if (selection)
          theState.addAttributes(theGlobals, text_cdt, selection->attributes);

        text_cdt["attributes"]["x"] = Fmi::to_string(point.x + point.dx + label.dx);
        text_cdt["attributes"]["y"] = Fmi::to_string(point.y + point.dy + label.dy);
        theLayersCdt.PushBack(text_cdt);
      }
    }

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in number layer")
          .addParameter("valid values", std::to_string(valid_count))
          .addParameter("minimum count", std::to_string(minvalues));

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

    std::string report = "NumberLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // Establish the data

    bool use_observations = theState.isObservation(producer);
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

    // Establish the parameter

    if (!parameter)
      throw Fmi::Exception(BCP, "Parameter not set for number-layer");

    // Establish the valid time

    auto valid_time_period = getValidTimePeriod();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time_period.begin();

    // Establish the level

    if (q && !q->firstLevel())
      throw Fmi::Exception(BCP, "Unable to set first level in querydata.");

    if (level)
    {
      if (use_observations)
        throw std::runtime_error("Cannot set level value for observations in NumberLayer");

      if (!q->selectLevel(*level))
        throw Fmi::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available!");
    }

    // Get projection details

    projection.update(q);
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time_period.begin(), theState);

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

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    PointValues pointvalues;

    if (!use_observations)
      pointvalues = read_forecasts(*this, q, crs, box, valid_time_period);
#ifndef WITHOUT_OBSERVATION
    else
      pointvalues = read_observations(*this, theState, crs, box, valid_time_period);
#endif

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

    // Symbols first

    for (const auto& pointvalue : pointvalues)
    {
      // Start generating the hash

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      const auto& point = pointvalue.point;
      float value = pointvalue.value;

      if (value != kFloatMissing)
        value = xmultiplier * value + xoffset;

      std::string iri;
      if (symbol)
        iri = *symbol;

      auto selection = Select::attribute(numbers, value);
      if (selection && selection->symbol)
        iri = *selection->symbol;

      // librsvg cannot handle scale + transform, must move former into latter
      boost::optional<double> rescale;
      if (selection)
      {
        auto scaleattr = selection->attributes.remove("scale");
        if (scaleattr)
          rescale = Fmi::stod(*scaleattr);
      }

      if (!iri.empty())
      {
        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        // Lack of CSS3 transform support forces us to use a direct transformation
        // which may override user settings
        tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

        std::string tmp = fmt::sprintf("translate(%d,%d)", point.x, point.y);

        double newscale = (scale ? *scale : 1.0) * (rescale ? *rescale : 1.0);
        if (newscale != 1.0)
          tmp += fmt::sprintf(" scale(%g)", newscale);

        tag_cdt["attributes"]["transform"] = tmp;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }
    theLayersCdt.PushBack(group_cdt);

    // Then numbers

    int valid_count = 0;
    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point;
      float value = pointvalue.value;

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
        text_cdt["cdata"] = txt;

        auto selection = Select::attribute(numbers, value);

        if (selection)
          selection->attributes.remove("scale");

        if (selection)
          theState.addAttributes(theGlobals, text_cdt, selection->attributes);

        text_cdt["attributes"]["x"] = Fmi::to_string(point.x + point.dx + label.dx);
        text_cdt["attributes"]["y"] = Fmi::to_string(point.y + point.dy + label.dy);
        theLayersCdt.PushBack(text_cdt);
      }
    }

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in number layer")
          .addParameter("valid values", std::to_string(valid_count))
          .addParameter("minimum count", std::to_string(minvalues));

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

std::size_t NumberLayer::hash_value(const State& theState) const
{
  try
  {
    // Disable caching of observation layers
    if (theState.isObservation(producer))
      return invalid_hash;

    auto hash = Layer::hash_value(theState);

    if (!(source && *source == "grid"))
    {
      auto q = getModel(theState);
      if (q)
        Dali::hash_combine(hash, Engine::Querydata::hash_value(q));
    }

    Dali::hash_combine(hash, Dali::hash_value(parameter));
    Dali::hash_combine(hash, Dali::hash_value(geometryId));
    Dali::hash_combine(hash, Dali::hash_value(levelId));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(forecastType));
    Dali::hash_combine(hash, Dali::hash_value(forecastNumber));
    Dali::hash_combine(hash, Dali::hash_value(unit_conversion));
    Dali::hash_combine(hash, Dali::hash_value(multiplier));
    Dali::hash_combine(hash, Dali::hash_value(offset));
    Dali::hash_combine(hash, Dali::hash_value(positions, theState));
    Dali::hash_combine(hash, Dali::hash_value(minvalues));
    Dali::hash_combine(hash, Dali::hash_value(maxdistance));
    Dali::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Dali::hash_combine(hash, Dali::hash_value(scale));
    Dali::hash_combine(hash, Dali::hash_value(label, theState));
    Dali::hash_combine(hash, Dali::hash_value(numbers, theState));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Calculating hash_value for the layer failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
