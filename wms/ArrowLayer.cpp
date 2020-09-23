//======================================================================
#include "ArrowLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"
#include <engines/gis/Engine.h>
#include <engines/querydata/Q.h>
#include <macgyver/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <spine/ParameterTools.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <boost/math/constants/constants.hpp>
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
struct PointValue
{
  Positions::Point point;
  double direction;
  double speed;
};

using PointValues = std::vector<PointValue>;
}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Forecast reader
 */
// ----------------------------------------------------------------------

PointValues read_forecasts(const ArrowLayer& layer,
                           const Engine::Querydata::Q& q,
                           const std::shared_ptr<OGRSpatialReference>& crs,
                           const Fmi::Box& box,
                           const boost::posix_time::time_period& time_period)
{
  try
  {
    NFmiMetTime met_time = time_period.begin();

    boost::optional<Spine::Parameter> dirparam, speedparam, uparam, vparam;

    if (layer.direction)
      dirparam = Spine::ParameterFactory::instance().parse(*layer.direction);
    if (layer.speed)
      speedparam = Spine::ParameterFactory::instance().parse(*layer.speed);
    if (layer.u)
      uparam = Spine::ParameterFactory::instance().parse(*layer.u);
    if (layer.v)
      vparam = Spine::ParameterFactory::instance().parse(*layer.v);

    if (speedparam && !q->param(speedparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + speedparam->name() + " not available in the arrow layer querydata");

    if (dirparam && !q->param(dirparam->number()))
      throw Fmi::Exception(
          BCP, "Parameter " + dirparam->name() + " not available in the arrow layer querydata");

    // WindUMS and WindVMS are metaparameters, cannot check their existence here

    // We may need to convert relative U/V components to true north
    boost::movelib::unique_ptr<OGRCoordinateTransformation> uvtransformation;
    boost::movelib::unique_ptr<OGRSpatialReference> wgs84;
    boost::movelib::unique_ptr<OGRSpatialReference> qsrs;
    if (uparam && vparam && q->isRelativeUV())
    {
      wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
      OGRErr err = wgs84->SetFromUserInput("WGS84");
      if (err != OGRERR_NONE)
        throw Fmi::Exception(BCP, "GDAL does not understand WGS84");

      qsrs = boost::movelib::make_unique<OGRSpatialReference>();
      err = qsrs->SetFromUserInput(q->area().WKT().c_str());
      if (err != OGRERR_NONE)
        throw Fmi::Exception(BCP, "Failed to establish querydata spatial reference");
      uvtransformation.reset(OGRCreateCoordinateTransformation(wgs84.get(), qsrs.get()));
    }

    // Generate the coordinates for the arrows

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

    PointValues pointvalues;

    // Q API SUCKS
    boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
    boost::local_time::local_date_time localdatetime(met_time, utc);
    std::string tmp;
    auto mylocale = std::locale::classic();
    NFmiPoint dummy;

    for (const auto& point : points)
    {
      if (!layer.inside(box, point.x, point.y))
        continue;

      // printf("POS %d,%d  %f %f\n",point.x, point.y,point.latlon.X(), point.latlon.Y());

      // Arrow direction and speed
      double wdir = kFloatMissing;
      double wspd = 0;

      if (uparam && vparam)
      {
        Spine::Location loc(point.latlon.X(), point.latlon.Y());

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
        auto uresult = q->value(up, localdatetime);

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
        auto vresult = q->value(vp, localdatetime);

        if (boost::get<double>(&uresult) != nullptr && boost::get<double>(&vresult) != nullptr)
        {
          auto uspd = *boost::get<double>(&uresult);
          auto vspd = *boost::get<double>(&vresult);

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
        q->param(dirparam->number());
        wdir = q->interpolate(point.latlon, met_time, 180);
        if (speedparam)
        {
          q->param(speedparam->number());
          wspd = q->interpolate(point.latlon, met_time, 180);
        }
      }

      // Skip points with invalid values
      if (wdir == kFloatMissing || wspd == kFloatMissing)
        continue;

      PointValue value{point, wdir, wspd};
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
                               boost::optional<std::string> dirParam,
                               boost::optional<std::string> speedParam,
                               boost::optional<std::string> uParam,
                               boost::optional<std::string> vParam,
                               const std::shared_ptr<OGRSpatialReference>& crs,
                               const Fmi::Box& box,
                               const boost::posix_time::time_period& time_period)
{
  try
  {
    // Generate the coordinates for the symbols

    // const bool forecast_mode = true;
    // auto points = layer.positions->getPoints(nullptr, crs, box, forecast_mode);

    PointValues pointvalues;

    int width = 0;
    int height = 0;
    int originalWidth = 0;
    int originalHeight = 0;
    int relativeUV = 0;

    const char* widthStr = query.mAttributeList.getAttributeValue("grid.width");
    const char* heightStr = query.mAttributeList.getAttributeValue("grid.height");
    const char* originalCrs = query.mAttributeList.getAttributeValue("grid.original.crs");
    const char* originalRelativeUVStr =
        query.mAttributeList.getAttributeValue("grid.original.relativeUV");
    const char* originalWidthStr = query.mAttributeList.getAttributeValue("grid.original.width");
    const char* originalHeightStr = query.mAttributeList.getAttributeValue("grid.original.height");

    if (widthStr)
      width = atoi(widthStr);

    if (heightStr)
      height = atoi(heightStr);

    if (originalRelativeUVStr)
      relativeUV = atoi(originalRelativeUVStr);

    if (originalWidthStr)
      originalWidth = atoi(originalWidthStr);

    if (originalHeightStr)
      originalHeight = atoi(originalHeightStr);

    T::ParamValue_vec* dirValues = nullptr;
    T::ParamValue_vec* speedValues = nullptr;
    T::ParamValue_vec* vValues = nullptr;
    T::ParamValue_vec* uValues = nullptr;
    uint originalGeometryId = 0;

    for (auto param = query.mQueryParameterList.begin(); param != query.mQueryParameterList.end();
         ++param)
    {
      for (auto val = param->mValueList.begin(); val != param->mValueList.end(); ++val)
      {
        if (val->mValueVector.size() > 0)
        {
          originalGeometryId = val->mGeometryId;

          if (dirParam && param->mParameterKey == *dirParam)
            dirValues = &val->mValueVector;
          else if (speedParam && param->mParameterKey == *speedParam)
            speedValues = &val->mValueVector;
          else if (vParam && param->mParameterKey == *vParam)
            vValues = &val->mValueVector;
          else if (uParam && param->mParameterKey == *uParam)
            uValues = &val->mValueVector;
        }
      }
    }

    auto points = layer.positions->getPoints(
        originalCrs, originalWidth, originalHeight, originalGeometryId, crs, box);

    if (dirValues && dirValues->size() > 0)
    {
      for (const auto& point : points)
      {
        double wdir = ParamValueMissing;
        double wspeed = 0;
        if (layer.inside(box, point.x, point.y))
        {
          size_t pos = (height - point.y - 1) * width + point.x;

          if (pos < dirValues->size())
          {
            wdir = (*dirValues)[pos];
            if (speedValues)
              wspeed = (*speedValues)[pos];
            else
              wspeed = 10;

            if (wdir != ParamValueMissing && wspeed != ParamValueMissing)
            {
              PointValue value{point, wdir, wspeed};
              pointvalues.push_back(value);
              // printf("POS %d,%d  %f %f\n",point.x, point.y,point.latlon.X(), point.latlon.Y());
            }
          }
        }
      }
      return pointvalues;
    }

    if (uValues && vValues && uValues->size() == vValues->size())
    {
      // We may need to convert relative U/V components to true north
      boost::movelib::unique_ptr<OGRCoordinateTransformation> uvtransformation;

      if (relativeUV && originalCrs)
      {
        boost::movelib::unique_ptr<OGRSpatialReference> wgs84;
        boost::movelib::unique_ptr<OGRSpatialReference> qsrs;
        wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
        OGRErr err = wgs84->SetFromUserInput("WGS84");
        if (err != OGRERR_NONE)
          throw Fmi::Exception(BCP, "GDAL does not understand WGS84");

        qsrs = boost::movelib::make_unique<OGRSpatialReference>();
        err = qsrs->SetFromUserInput(originalCrs);
        if (err != OGRERR_NONE)
          throw Fmi::Exception(BCP, "Failed to establish querydata spatial reference");

        uvtransformation.reset(OGRCreateCoordinateTransformation(wgs84.get(), qsrs.get()));
      }

      for (const auto& point : points)
      {
        double wdir = ParamValueMissing;
        double wspeed = 0;
        if (layer.inside(box, point.x, point.y))
        {
          size_t pos = (height - point.y - 1) * width + point.x;

          if (pos < vValues->size())
          {
            double v = (*vValues)[pos];
            double u = (*uValues)[pos];
            if (u != ParamValueMissing && v != ParamValueMissing)
            {
              wspeed = sqrt(u * u + v * v);

              if (uvtransformation == nullptr)
                wdir = fmod(180 + 180 / pi * atan2(u, v), 360);
              else
              {
                auto rot =
                    Fmi::OGR::gridNorth(*uvtransformation, point.latlon.X(), point.latlon.Y());
                if (!rot)
                  continue;
                wdir = fmod(180 - *rot + 180 / pi * atan2(u, v), 360);
              }
            }

            if (wdir != ParamValueMissing && wspeed != ParamValueMissing)
            {
              PointValue value{point, wdir, wspeed};
              pointvalues.push_back(value);
            }
          }
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
 * \brief Observation reader
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION

PointValues read_all_observations(const ArrowLayer& layer,
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

    settings.timestep = 0;

    settings.starttimeGiven = true;

    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();
    settings.parameters.push_back(Spine::makeParameter("stationlon"));
    settings.parameters.push_back(Spine::makeParameter("stationlat"));

    bool use_uv_components = (layer.u && layer.v);

    if (use_uv_components)
    {
      settings.parameters.push_back(Spine::makeParameter(*layer.u));
      settings.parameters.push_back(Spine::makeParameter(*layer.v));
    }
    else
    {
      settings.parameters.push_back(Spine::makeParameter(*layer.direction));
      if (layer.speed)
        settings.parameters.push_back(Spine::makeParameter(*layer.speed));
      else
        settings.parameters.push_back(Spine::makeParameter("WindSpeedMS"));
    }

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

      double wdir = kFloatMissing;
      double wspd = kFloatMissing;
      if (!use_uv_components)
      {
        wdir = get_double(values.at(2).at(row));
        wspd = get_double(values.at(3).at(row));
      }
      else
      {
        double uspd = get_double(values.at(2).at(row));
        double vspd = get_double(values.at(3).at(row));

        if (uspd != kFloatMissing && vspd != kFloatMissing)
        {
          wspd = sqrt(uspd * uspd + vspd * vspd);
          if (uspd != 0 || vspd != 0)
            wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
        }
      }

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
      PointValue pv{point, wdir, wspd};
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read observations from the database");
  }
}

PointValues read_station_observations(const ArrowLayer& layer,
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

    bool use_uv_components = (layer.u && layer.v);

    if (use_uv_components)
    {
      settings.parameters.push_back(Spine::makeParameter(*layer.u));
      settings.parameters.push_back(Spine::makeParameter(*layer.v));
    }
    else
    {
      settings.parameters.push_back(Spine::makeParameter(*layer.direction));
      if (layer.speed)
        settings.parameters.push_back(Spine::makeParameter(*layer.speed));
      else
        settings.parameters.push_back(Spine::makeParameter("WindSpeedMS"));
    }

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
      // obsengine returns the data sorted by fmisid and by time

      const int row = 0;

      double lon = get_double(values.at(0).at(row));
      double lat = get_double(values.at(1).at(row));

      double wdir = kFloatMissing;
      double wspd = kFloatMissing;
      if (!use_uv_components)
      {
        wdir = get_double(values.at(2).at(row));
        wspd = get_double(values.at(3).at(row));
      }
      else
      {
        double uspd = get_double(values.at(2).at(row));
        double vspd = get_double(values.at(3).at(row));

        if (uspd != kFloatMissing && vspd != kFloatMissing)
        {
          wspd = sqrt(uspd * uspd + vspd * vspd);
          if (uspd != 0 || vspd != 0)
            wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
        }
      }

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

      int deltax = (station.dx ? *station.dx : 0);
      int deltay = (station.dy ? *station.dy : 0);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat), deltax, deltay};
      PointValue pv{point, wdir, wspd};
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read observations from the database");
  }
}

PointValues read_latlon_observations(const ArrowLayer& layer,
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

    bool use_uv_components = (layer.u && layer.v);

    if (use_uv_components)
    {
      settings.parameters.push_back(Spine::makeParameter(*layer.u));
      settings.parameters.push_back(Spine::makeParameter(*layer.v));
    }
    else
    {
      settings.parameters.push_back(Spine::makeParameter(*layer.direction));
      if (layer.speed)
        settings.parameters.push_back(Spine::makeParameter(*layer.speed));
      else
        settings.parameters.push_back(Spine::makeParameter("WindSpeedMS"));
    }

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

      if (used_fmisids.find(fmisid) != used_fmisids.end())
        continue;
      used_fmisids.insert(fmisid);

      double wdir = kFloatMissing;
      double wspd = kFloatMissing;
      if (!use_uv_components)
      {
        wdir = get_double(values.at(3).at(row));
        wspd = get_double(values.at(4).at(row));
      }
      else
      {
        double uspd = get_double(values.at(3).at(row));
        double vspd = get_double(values.at(4).at(row));

        if (uspd != kFloatMissing && vspd != kFloatMissing)
        {
          wspd = sqrt(uspd * uspd + vspd * vspd);
          if (uspd != 0 || vspd != 0)
            wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
        }
      }

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
      PointValue pv{pp, wdir, wspd};
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read observations from the database");
  }
}

PointValues read_observations(const ArrowLayer& layer,
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
          BCP, "Failed to create the needed coordinate transformation when drawing arrows");

    if (layer.isFlashOrMobileProducer(*layer.producer))
      throw Fmi::Exception(BCP, "Cannot use flsh or mobile producer in ArrowLayer");

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
    throw Fmi::Exception::Trace(BCP, "ArrowLayer failed to read observations from the database");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void ArrowLayer::init(const Json::Value& theJson,
                      const State& theState,
                      const Config& theConfig,
                      const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Arrow-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("direction", nulljson);
    if (!json.isNull())
      direction = json.asString();

    json = theJson.get("speed", nulljson);
    if (!json.isNull())
      speed = json.asString();

    json = theJson.get("u", nulljson);
    if (!json.isNull())
      u = json.asString();

    json = theJson.get("v", nulljson);
    if (!json.isNull())
      v = json.asString();

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

    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      symbol = json.asString();

    json = theJson.get("scale", nulljson);
    if (!json.isNull())
      scale = json.asDouble();

    json = theJson.get("southflop", nulljson);
    if (!json.isNull())
      southflop = json.asBool();

    json = theJson.get("northflop", nulljson);
    if (!json.isNull())
      northflop = json.asBool();

    json = theJson.get("flip", nulljson);
    if (!json.isNull())
      flip = json.asBool();

    json = theJson.get("positions", nulljson);
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }

    json = theJson.get("dx", nulljson);
    if (!json.isNull())
      dx = json.asInt();

    json = theJson.get("dy", nulljson);
    if (!json.isNull())
      dy = json.asInt();

    json = theJson.get("minvalues", nulljson);
    if (!json.isNull())
      minvalues = json.asInt();

    json = theJson.get("maxdistance", nulljson);
    if (!json.isNull())
      maxdistance = json.asDouble();

    json = theJson.get("unit_conversion", nulljson);
    if (!json.isNull())
      unit_conversion = json.asString();

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();

    json = theJson.get("minrotationspeed", nulljson);
    if (!json.isNull())
      minrotationspeed = json.asDouble();

    json = theJson.get("arrows", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("arrows", arrows, json, theConfig);
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
    exception.addParameter("Producer", *producer);
    exception.addParameter("Direction", *direction);
    exception.addParameter("Speed", *speed);
    exception.addParameter("V", *v);
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
    // Time execution

    std::string report = "ArrowLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

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

    boost::optional<std::string> dirparam, speedparam, uparam, vparam;

    if (direction)
    {
      paramList.push_back(*direction);
    }
    if (speed)
    {
      paramList.push_back(*speed);
    }
    if (u)
    {
      paramList.push_back(*u);
    }
    if (v)
    {
      paramList.push_back(*v);
    }

    auto gridEngine = theState.getGridEngine();
    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;
    auto valid_time_period = getValidTimePeriod();

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time_period.begin();

    std::string wkt = *projection.crs;
    if (wkt != "data")
    {
      // Getting the bounding box and the WKT of the requested projection.

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

    std::string aProducer = *producer;
    char paramBuf[1000];
    char* p = paramBuf;

    for (auto parameter = paramList.begin(); parameter != paramList.end(); ++parameter)
    {
      std::string pName = *parameter;
      auto pos = pName.find(".raw");
      if (pos != std::string::npos)
      {
        attributeList.addAttribute("areaInterpolationMethod",
                                   std::to_string(T::AreaInterpolationMethod::Linear));
        pName.erase(pos, 4);
      }

      std::string param = gridEngine->getParameterString(*producer, pName);

      if (!projection.projectionParameter)
        projection.projectionParameter = param;

      if (param == *parameter && originalGridQuery->mProducerNameList.size() == 0)
      {
        gridEngine->getProducerNameList(*producer, originalGridQuery->mProducerNameList);
        if (originalGridQuery->mProducerNameList.size() == 0)
          originalGridQuery->mProducerNameList.push_back(*producer);
      }

      if (p != paramBuf)
        p += sprintf(p, ",");

      p += sprintf(p, "%s", param.c_str());
    }

    attributeList.addAttribute("param", paramBuf);

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

    // Get projection details

    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    if (wkt == "data")
      return;

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time_period.begin(), theState);

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
        *this, gridEngine, *query, direction, speed, u, v, crs, box, valid_time_period);

    // Coordinate transformation from WGS84 to output SRS so that we can rotate
    // winds according to map north

    auto wgs84 = boost::movelib::make_unique<OGRSpatialReference>();
    OGRErr err = wgs84->SetFromUserInput("WGS84");
    if (err != OGRERR_NONE)
      throw Fmi::Exception(BCP, "GDAL does not understand WGS84");

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), crs.get()));
    if (transformation == nullptr)
      throw Fmi::Exception(
          BCP,
          "Failed to create the needed coordinate transformation when reading wind directions");

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
      const auto& point = pointvalue.point;

      // Select arrow based on speed or U- and V-components, if available
      bool check_speeds = (!arrows.empty() && (speed || (u && v)));

      double wdir = pointvalue.direction;
      double wspd = pointvalue.speed;

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
      auto fix = Fmi::OGR::gridNorth(*transformation, point.latlon.X(), point.latlon.Y());
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

      boost::optional<double> rescale;

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
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

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

    // Establish the valid time

    auto valid_time_period = getValidTimePeriod();

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
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time_period.begin(), theState);

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
      pointvalues = read_forecasts(*this, q, crs, box, valid_time_period);
#ifndef WITHOUT_OBSERVATION
    else
      pointvalues = read_observations(*this, theState, crs, box, valid_time_period);
#endif

    // Coordinate transformation from WGS84 to output SRS so that we can rotate
    // winds according to map north

    auto wgs84 = theState.getGisEngine().getSpatialReference("WGS84");

    boost::movelib::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(wgs84.get(), crs.get()));
    if (transformation == nullptr)
      throw Fmi::Exception(
          BCP,
          "Failed to create the needed coordinate transformation when reading wind directions");

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
      const auto& point = pointvalue.point;

      // Select arrow based on speed or U- and V-components, if available
      bool check_speeds = (!arrows.empty() && (speed || (u && v)));

      double wdir = pointvalue.direction;
      double wspd = pointvalue.speed;

      if (wdir == kFloatMissing)
        continue;

      if (check_speeds && wspd == kFloatMissing)
        continue;

      ++valid_count;

      // Unit transformation
      double xmultiplier = (multiplier ? *multiplier : 1.0);
      double xoffset = (offset ? *offset : 0.0);
      if (wspd != kFloatMissing)
        wspd = xmultiplier * wspd + xoffset;

      // Apply final rotation to output coordinate system
      auto fix = Fmi::OGR::gridNorth(*transformation, point.latlon.X(), point.latlon.Y());
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

      boost::optional<double> rescale;

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
      return invalid_hash;

    auto hash = Layer::hash_value(theState);

    if (!(source && *source == "grid"))
    {
      auto q = getModel(theState);
      if (q)
        Dali::hash_combine(hash, Engine::Querydata::hash_value(q));
    }

    Dali::hash_combine(hash, Dali::hash_value(source));
    Dali::hash_combine(hash, Dali::hash_value(direction));
    Dali::hash_combine(hash, Dali::hash_value(speed));
    Dali::hash_combine(hash, Dali::hash_value(u));
    Dali::hash_combine(hash, Dali::hash_value(v));
    Dali::hash_combine(hash, Dali::hash_value(geometryId));
    Dali::hash_combine(hash, Dali::hash_value(levelId));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(forecastType));
    Dali::hash_combine(hash, Dali::hash_value(forecastNumber));
    Dali::hash_combine(hash, Dali::hash_value(unit_conversion));
    Dali::hash_combine(hash, Dali::hash_value(multiplier));
    Dali::hash_combine(hash, Dali::hash_value(offset));
    Dali::hash_combine(hash, Dali::hash_value(minrotationspeed));
    Dali::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Dali::hash_combine(hash, Dali::hash_value(scale));
    Dali::hash_combine(hash, Dali::hash_value(southflop));
    Dali::hash_combine(hash, Dali::hash_value(northflop));
    Dali::hash_combine(hash, Dali::hash_value(flip));
    Dali::hash_combine(hash, Dali::hash_value(positions, theState));
    Dali::hash_combine(hash, Dali::hash_value(dx));
    Dali::hash_combine(hash, Dali::hash_value(dy));
    Dali::hash_combine(hash, Dali::hash_value(minvalues));
    Dali::hash_combine(hash, Dali::hash_value(maxdistance));
    Dali::hash_combine(hash, Dali::hash_value(arrows, theState));
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
