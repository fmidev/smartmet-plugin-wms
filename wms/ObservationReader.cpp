#ifndef WITHOUT_OBSERVATIONS

#include "ObservationReader.h"
#include "Layer.h"
#include "PointData.h"
#include "Positions.h"
#include "State.h"
#include "ValueTools.h"
#include <engines/observation/Engine.h>
#include <fmt/format.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/SpatialReference.h>
#include <timeseries/ParameterTools.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace ObservationReader
{
namespace
{
// Add extra parameter unless it is already included
std::size_t add_help_parameter(std::vector<Spine::Parameter>& parameters, const std::string& param)
{
  for (auto i = 0UL; i < parameters.size(); i++)
  {
    if (parameters[i].name() == param)
      return i;
  }
  auto n = parameters.size();
  parameters.push_back(TS::makeParameter(param));
  return n;
}

// ----------------------------------------------------------------------
/*!
 * \brief Flash data reader
 */
// ----------------------------------------------------------------------

PointValues read_flash_observations(State& state,
                                    const std::vector<std::string>& parameters,
                                    const Layer& layer,
                                    const Positions& positions,
                                    const Fmi::SpatialReference& crs,
                                    const Fmi::Box& box,
                                    const boost::posix_time::time_period& valid_time_period,
                                    const Fmi::CoordinateTransformation& transformation)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.timezone = "UTC";
    settings.numberofstations = 1;
    settings.localTimePool = state.getLocalTimePool();

    settings.timestep = 0;

    settings.starttimeGiven = true;

    // Note: wantedtime is not set, we want all the flashes from the time period
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    for (const auto& p : parameters)
      settings.parameters.push_back(TS::makeParameter(p));

    auto lon_idx = add_help_parameter(settings.parameters, "longitude");
    auto lat_idx = add_help_parameter(settings.parameters, "latitude");

    // Request intersection parameters too - if any
    auto iparams = positions.intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(TS::makeParameter(extraparam));

    // Generate the coordinates for the symbols

    Engine::Querydata::Q q;
    const bool forecast_mode = false;
    auto points = positions.getPoints(q, crs, box, forecast_mode);

    auto& obsengine = state.getObsEngine();
    Engine::Observation::StationSettings stationSettings;
    stationSettings.bounding_box_settings = layer.getClipBoundingBox(box, crs);
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

    PointValues pointvalues;

    for (std::size_t row = 0; row < values[0].size(); ++row)
    {
      double lon = get_double(values.at(lon_idx).at(row));
      double lat = get_double(values.at(lat_idx).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (!crs.isGeographic())
        if (!transformation.transform(x, y))
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!layer.inside(box, x, y))
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!positions.inside(lon, lat, ivalues))
        continue;

      int xpos = lround(x);
      int ypos = lround(y);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
      PointData pv{point};
      for (auto i = 0UL; i < parameters.size(); i++)
        pv.add(get_double(values.at(i).at(row)));

      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Layer failed to read observations from the database");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read all available stations
 */
// ----------------------------------------------------------------------

PointValues read_all_observations(State& state,
                                  const std::vector<std::string>& parameters,
                                  const Layer& layer,
                                  const Positions& positions,
                                  double maxdistance,
                                  const Fmi::SpatialReference& crs,
                                  const Fmi::Box& box,
                                  const boost::posix_time::ptime& valid_time,
                                  const boost::posix_time::time_period& valid_time_period,
                                  const Fmi::CoordinateTransformation& transformation)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.timezone = "UTC";
    settings.numberofstations = 1;
    settings.maxdistance = maxdistance * 1000;  // obsengine uses meters
    settings.localTimePool = state.getLocalTimePool();

    // settings.timestep = ?;

    settings.starttimeGiven = true;

    settings.wantedtime = valid_time;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();

    for (const auto& p : parameters)
      settings.parameters.push_back(TS::makeParameter(p));

    auto lon_idx = add_help_parameter(settings.parameters, "stationlon");
    auto lat_idx = add_help_parameter(settings.parameters, "stationlat");

    // Request intersection parameters too - if any
    auto iparams = positions.intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(TS::makeParameter(extraparam));

    // Coordinates or bounding box
    Engine::Observation::StationSettings stationSettings;
    stationSettings.bounding_box_settings = layer.getClipBoundingBox(box, crs);
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
      double lon = get_double(values.at(lon_idx).at(row));
      double lat = get_double(values.at(lat_idx).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (!crs.isGeographic())
        if (!transformation.transform(x, y))
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!layer.inside(box, x, y))
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!positions.inside(lon, lat, ivalues))
        continue;

      int xpos = lround(x);
      int ypos = lround(y);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
      PointData pv{point};
      for (auto i = 0UL; i < parameters.size(); i++)
        pv.add(get_double(values.at(i).at(row)));

      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Layer failed to read observations from the database");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read specific stations only
 */
// ----------------------------------------------------------------------

PointValues read_station_observations(State& state,
                                      const std::vector<std::string>& parameters,
                                      const Layer& layer,
                                      const Positions& positions,
                                      double maxdistance,
                                      const Fmi::SpatialReference& crs,
                                      const Fmi::Box& box,
                                      const boost::posix_time::ptime& valid_time,
                                      const boost::posix_time::time_period& valid_time_period,
                                      const Fmi::CoordinateTransformation& transformation)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.timezone = "UTC";
    settings.numberofstations = 1;
    settings.maxdistance = maxdistance * 1000;  // obsengine uses meters
    settings.localTimePool = state.getLocalTimePool();

    // settings.timestep = ?;

    settings.starttimeGiven = true;

    settings.wantedtime = valid_time;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();

    for (const auto& p : parameters)
      settings.parameters.push_back(TS::makeParameter(p));

    auto lon_idx = add_help_parameter(settings.parameters, "stationlon");
    auto lat_idx = add_help_parameter(settings.parameters, "stationlat");

    // Request intersection parameters too - if any
    auto iparams = positions.intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(TS::makeParameter(extraparam));

    // We must read the stations one at a time to preserve dx,dy values
    PointValues pointvalues;

    for (const auto& station : positions.stations.stations)
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

      if (opts.taggedFMISIDs.empty())
        continue;

      auto result = obsengine.values(opts);

      if (!result || result->empty() || (*result)[0].empty())
        continue;

      const auto& values = *result;

      // We accept only the newest observation for each interval
      // obsengine returns the data sorted by fmisid and by time,

      const int row = 0;
      double lon = get_double(values.at(lon_idx).at(row));
      double lat = get_double(values.at(lat_idx).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (!crs.isGeographic())
        if (!transformation.transform(x, y))
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!layer.inside(box, x, y))
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!positions.inside(lon, lat, ivalues))
        continue;

      int xpos = lround(x);
      int ypos = lround(y);

      // Keep only the latest value for each coordinate

      int deltax = (station.dx ? *station.dx : 0);
      int deltay = (station.dy ? *station.dy : 0);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat), deltax, deltay};
      PointData pv{point};
      for (auto i = 0UL; i < parameters.size(); i++)
        pv.add(get_double(values.at(i).at(row)));
      pointvalues.push_back(pv);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Layer failed to read observations from the database");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Read specific coordinates only
 */
// ----------------------------------------------------------------------

PointValues read_latlon_observations(State& state,
                                     const std::vector<std::string>& parameters,
                                     const Layer& layer,
                                     const Positions& positions,
                                     double maxdistance,
                                     const Fmi::SpatialReference& crs,
                                     const Fmi::Box& box,
                                     const boost::posix_time::ptime& valid_time,
                                     const boost::posix_time::time_period& valid_time_period,
                                     const Fmi::CoordinateTransformation& transformation,
                                     const Positions::Points& points)
{
  Engine::Observation::Settings settings;
  try
  {
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.timezone = "UTC";
    settings.numberofstations = 1;              // we need only the nearest station
    settings.maxdistance = maxdistance * 1000;  // obsengine uses meters
    settings.localTimePool = state.getLocalTimePool();

    settings.starttimeGiven = true;

    settings.wantedtime = valid_time;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();

    for (const auto& p : parameters)
      settings.parameters.push_back(TS::makeParameter(p));

    auto stationlon_idx = add_help_parameter(settings.parameters, "stationlon");
    auto stationlat_idx = add_help_parameter(settings.parameters, "stationlat");
    auto fmisid_idx = add_help_parameter(settings.parameters, "fmisid");

    // Request intersection parameters too - if any
    auto iparams = positions.intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
      settings.parameters.push_back(TS::makeParameter(extraparam));

    // settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;
    // std::cout << "Settings:\n" << settings << "\n";

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

      if (opts.taggedFMISIDs.empty())
        continue;

      try
      {
        auto result = obsengine.values(opts);
        if (!result || result->empty() || (*result)[0].empty())
          continue;

        const auto& values = *result;

        // We accept only the newest observation for each interval
        // obsengine returns the data sorted by fmisid and by time

        const int row = 0;

        int fmisid = get_fmisid(values.at(fmisid_idx).at(row));
        double lon = get_double(values.at(stationlon_idx).at(row));
        double lat = get_double(values.at(stationlat_idx).at(row));

        if (used_fmisids.find(fmisid) != used_fmisids.end())
          continue;
        used_fmisids.insert(fmisid);

        // Collect extra values used for filtering the input

        Intersections::IntersectValues ivalues;

        for (std::size_t i = 0; i < iparams.size(); i++)
          ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

        // Convert latlon to world coordinate if needed

        double x = lon;
        double y = lat;

        if (!crs.isGeographic())
          if (!transformation.transform(x, y))
            continue;

        // To pixel coordinate
        box.transform(x, y);

        // Skip if not inside desired area
        if (!layer.inside(box, x, y))
          continue;

        // Skip if not inside/outside desired shapes or limits of other parameters
        if (!positions.inside(lon, lat, ivalues))
          continue;

        int xpos = lround(x);
        int ypos = lround(y);

        Positions::Point pp{xpos, ypos, NFmiPoint(lon, lat), point.dx, point.dy};
        PointData pv{pp};
        for (auto i = 0UL; i < parameters.size(); i++)
          pv.add(get_double(values.at(i).at(row)));
        pointvalues.push_back(pv);
      }
      catch (...)
      {
        throw;
      }
    }
    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Layer failed to read observations from the database");
  }
}
}  // namespace

PointValues read(State& state,
                 const std::vector<std::string>& parameters,
                 const Layer& layer,
                 const Positions& positions,
                 double maxdistance,
                 const Fmi::SpatialReference& crs,
                 const Fmi::Box& box,
                 const boost::posix_time::ptime& valid_time,
                 const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    // Create the coordinate transformation from image world coordinates
    // to WGS84 coordinates
    Fmi::CoordinateTransformation transformation("WGS84", crs);

    // Listed stations
    if (positions.layout == Positions::Layout::Station)
    {
      return read_station_observations(state,
                                       parameters,
                                       layer,
                                       positions,
                                       maxdistance,
                                       crs,
                                       box,
                                       valid_time,
                                       valid_time_period,
                                       transformation);
    }

    // We assume that for example ArrowLayer has already checked that the selected producer is valid
    // and does not provide just flash observations
    if (Layer::isFlashOrMobileProducer(*layer.producer))
    {
      return read_flash_observations(
          state, parameters, layer, positions, crs, box, valid_time_period, transformation);
    }

    Engine::Querydata::Q q;  // dummy for
    const bool forecast_mode = false;

    auto points = positions.getPoints(q, crs, box, forecast_mode);

    if (!points.empty())
      return read_latlon_observations(state,
                                      parameters,
                                      layer,
                                      positions,
                                      maxdistance,
                                      crs,
                                      box,
                                      valid_time,
                                      valid_time_period,
                                      transformation,
                                      points);

    return read_all_observations(state,
                                 parameters,
                                 layer,
                                 positions,
                                 maxdistance,
                                 crs,
                                 box,
                                 valid_time,
                                 valid_time_period,
                                 transformation);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "ObservationReader failed to read data from the database");
  }
}

}  // namespace ObservationReader
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif
