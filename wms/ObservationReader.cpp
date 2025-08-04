#ifndef WITHOUT_OBSERVATIONS

#include "ObservationReader.h"
#include "AggregationUtility.h"
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

struct DxDy
{
  DxDy(int x, int y) : dx(x), dy(y) {}
  int dx = 0;
  int dy = 0;
};

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
                                    const Fmi::TimePeriod& valid_time_period,
                                    const Fmi::CoordinateTransformation& transformation)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.timezone = "UTC";
    settings.numberofstations = 1;

    settings.timestep = 0;

    settings.starttimeGiven = true;

    if (Spine::optional_bool(state.getRequest().getParameter("debug"), false))
      settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;

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
    auto points = positions.getPoints(q, crs, box, forecast_mode, state);

    auto& obsengine = state.getObsEngine();
    Engine::Observation::StationSettings stationSettings;
    stationSettings.bounding_box_settings = layer.getClipBoundingBox(box, crs);
    settings.taggedFMISIDs = obsengine.translateToFMISID(settings, stationSettings);

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
                                  const Fmi::DateTime& valid_time,
                                  const Fmi::TimePeriod& valid_time_period,
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

    settings.starttimeGiven = true;

    settings.wantedtime = valid_time;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    if (Spine::optional_bool(state.getRequest().getParameter("debug"), false))
      settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;

    auto& obsengine = state.getObsEngine();

    // Store parameters and functions in paramFuncs
    std::vector<TS::ParameterAndFunctions> paramFuncs;
    for (const auto& p : parameters)
    {
      bool ignore_unknown_param = true;
      auto paf = TS::ParameterFactory::instance().parseNameAndFunctions(p, ignore_unknown_param);
      paramFuncs.push_back(paf);
      settings.parameters.push_back(paf.parameter);
    }

    auto fmisid_idx = add_help_parameter(settings.parameters, "fmisid");
    auto lon_idx = add_help_parameter(settings.parameters, "stationlon");
    auto lat_idx = add_help_parameter(settings.parameters, "stationlat");

    // Add fmisid, stationlon, stationlat into paramFuncs structure even if they dont have functions
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("fmisid"));
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("stationlon"));
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("stationlat"));

    // Request intersection parameters too - if any
    auto iparams = positions.intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
    {
      // Add extraparams into paramFuncs structure even if they dont have functions
      bool ignore_unknown_param = true;
      auto paf =
          TS::ParameterFactory::instance().parseNameAndFunctions(extraparam, ignore_unknown_param);
      paramFuncs.push_back(paf);
      settings.parameters.push_back(paf.parameter);
    }

    // Coordinates or bounding box
    Engine::Observation::StationSettings stationSettings;
    stationSettings.bounding_box_settings = layer.getClipBoundingBox(box, crs);
    settings.taggedFMISIDs = obsengine.translateToFMISID(settings, stationSettings);

    // Read the observations and aggregate if requested
    auto result = AggregationUtility::get_obsengine_values(
        obsengine, valid_time, paramFuncs, fmisid_idx, settings);

    // Build the pointvalues

    if (!result)
      return {};

    const auto& values = *result;
    if (values.empty())
      return {};

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
                                      const Fmi::DateTime& valid_time,
                                      const Fmi::TimePeriod& valid_time_period,
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

    settings.starttimeGiven = true;

    settings.wantedtime = valid_time;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    if (Spine::optional_bool(state.getRequest().getParameter("debug"), false))
      settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;

    auto& obsengine = state.getObsEngine();

    // Store parameters and functions in paramFuncs
    std::vector<TS::ParameterAndFunctions> paramFuncs;
    for (const auto& p : parameters)
    {
      bool ignore_unknown_param = true;
      auto paf = TS::ParameterFactory::instance().parseNameAndFunctions(p, ignore_unknown_param);
      paramFuncs.push_back(paf);
      settings.parameters.push_back(paf.parameter);
    }

    auto fmisid_idx = add_help_parameter(settings.parameters, "fmisid");
    auto lon_idx = add_help_parameter(settings.parameters, "stationlon");
    auto lat_idx = add_help_parameter(settings.parameters, "stationlat");

    // Add fmisid, stationlon, stationlat into paramFuncs structure even if they dont have functions
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("fmisid"));
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("stationlon"));
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("stationlat"));

    // Request intersection parameters too - if any
    auto iparams = positions.intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
    {
      // Add extraparams into paramFuncs structure even if they dont have functions
      bool ignore_unknown_param = true;
      auto paf =
          TS::ParameterFactory::instance().parseNameAndFunctions(extraparam, ignore_unknown_param);
      paramFuncs.push_back(paf);
      settings.parameters.push_back(paf.parameter);
    }

    // Collect information on station dx,dy values while converting IDs to fmisid numbers

    std::map<int, DxDy> fmisid_shifts;

    // Collection information on alternate lon,lat placements
    std::map<int, NFmiPoint> fmisid_alternate_lonlats;

    // We do not use the same station twice
    std::set<int> used_fmisids;

    for (const auto& station : positions.stations.stations)
    {
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
        stationSettings.geoid_settings.maxdistance = settings.maxdistance;
        stationSettings.geoid_settings.numberofstations = settings.numberofstations;
        stationSettings.geoid_settings.language = settings.language;
      }
      else if (station.longitude && station.latitude)
      {
        stationSettings.nearest_station_settings.emplace_back(*station.longitude,
                                                              *station.latitude,
                                                              settings.maxdistance,
                                                              settings.numberofstations,
                                                              "");
      }
      else
        throw Fmi::Exception(BCP, "Station ID or coordinate missing");

      auto tagged_fmisids = obsengine.translateToFMISID(settings, stationSettings);

      if (tagged_fmisids.empty())
        continue;

      DxDy dxdy{station.dx ? *station.dx : 0, station.dy ? *station.dy : 0};

      for (const auto& tagged_fmisid : tagged_fmisids)
      {
        auto fmisid = tagged_fmisid.fmisid;
        if (used_fmisids.find(fmisid) != used_fmisids.end())
          continue;
        used_fmisids.insert(fmisid);

        settings.taggedFMISIDs.push_back(tagged_fmisid);
        fmisid_shifts.insert({fmisid, dxdy});

        // Alternate placement given via lonlat to a station with an ID?
        if ((station.fmisid || station.wmo || station.lpnn || station.geoid) && station.longitude &&
            station.latitude)
          fmisid_alternate_lonlats[fmisid] = NFmiPoint(*station.longitude, *station.latitude);
      }
    }

    // Read the observations and aggregate if requested
    auto result = AggregationUtility::get_obsengine_values(
        obsengine, valid_time, paramFuncs, fmisid_idx, settings);

    PointValues pointvalues;

    if (!result || result->empty())
      return pointvalues;

    auto& values = *result;

    for (auto row = 0UL; row < values[0].size(); ++row)
    {
      auto fmisid = get_double(values.at(fmisid_idx).at(row));
      auto lon = get_double(values.at(lon_idx).at(row));
      auto lat = get_double(values.at(lat_idx).at(row));

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

      // Convert latlon to world coordinate if needed

      auto x = lon;
      auto y = lat;

      auto alt_lonlat = fmisid_alternate_lonlats.find(fmisid);
      if (alt_lonlat != fmisid_alternate_lonlats.end())
      {
        x = alt_lonlat->second.X();
        y = alt_lonlat->second.Y();
      }

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

      auto deltax = fmisid_shifts.at(fmisid).dx;
      auto deltay = fmisid_shifts.at(fmisid).dy;

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
                                     const Fmi::DateTime& valid_time,
                                     const Fmi::TimePeriod& valid_time_period,
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

    settings.starttimeGiven = true;

    settings.wantedtime = valid_time;
    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    if (Spine::optional_bool(state.getRequest().getParameter("debug"), false))
      settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;

    auto& obsengine = state.getObsEngine();

    // Store parameters and functions in paramFuncs
    std::vector<TS::ParameterAndFunctions> paramFuncs;
    for (const auto& p : parameters)
    {
      bool ignore_unknown_param = true;
      auto paf = TS::ParameterFactory::instance().parseNameAndFunctions(p, ignore_unknown_param);
      paramFuncs.push_back(paf);
      settings.parameters.push_back(paf.parameter);
    }

    auto fmisid_idx = add_help_parameter(settings.parameters, "fmisid");
    auto stationlon_idx = add_help_parameter(settings.parameters, "stationlon");
    auto stationlat_idx = add_help_parameter(settings.parameters, "stationlat");

    // Add fmisid, stationlon, stationlat into paramFuncs structure even if they dont have functions
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("fmisid"));
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("stationlon"));
    paramFuncs.push_back(TS::ParameterFactory::instance().parseNameAndFunctions("stationlat"));

    // Request intersection parameters too - if any
    auto iparams = positions.intersections.parameters();

    int firstextraparam =
        settings.parameters.size();  // which column holds the first extra parameter

    for (const auto& extraparam : iparams)
    {
      bool ignore_unknown_param = true;
      settings.parameters.push_back(TS::makeParameter(extraparam));
      paramFuncs.push_back(
          TS::ParameterFactory::instance().parseNameAndFunctions(extraparam, ignore_unknown_param));
    }

    // Collect information on point dx,dy values while converting IDs to fmisid numbers

    std::map<int, DxDy> fmisid_shifts;

    // We do not use the same station twice
    std::set<int> used_fmisids;

    for (const auto& point : points)
    {
      Engine::Observation::StationSettings stationSettings;
      stationSettings.nearest_station_settings.emplace_back(
          point.latlon.X(), point.latlon.Y(), settings.maxdistance, settings.numberofstations, "");

      auto tagged_fmisids = obsengine.translateToFMISID(settings, stationSettings);

      if (tagged_fmisids.empty())
        continue;

      DxDy dxdy{point.dx, point.dy};

      for (const auto& tagged_fmisid : tagged_fmisids)
      {
        auto fmisid = tagged_fmisid.fmisid;
        if (used_fmisids.find(fmisid) != used_fmisids.end())
          continue;

        double x = point.latlon.X();
        double y = point.latlon.Y();

        if (!crs.isGeographic())
          if (!transformation.transform(x, y))
            continue;

        // To pixel coordinate
        box.transform(x, y);

        // Skip if not inside desired area
        if (!layer.inside(box, x, y))
          continue;

        used_fmisids.insert(fmisid);

        settings.taggedFMISIDs.push_back(tagged_fmisid);
        fmisid_shifts.insert({fmisid, dxdy});
      }
    }

    // Read the observations and aggregate if requested
    auto result = AggregationUtility::get_obsengine_values(
        obsengine, valid_time, paramFuncs, fmisid_idx, settings);
    PointValues pointvalues;

    if (!result || result->empty())
      return pointvalues;

    auto& values = *result;

    for (auto row = 0UL; row < values[0].size(); ++row)
    {
      auto fmisid = get_double(values.at(fmisid_idx).at(row));
      auto lon = get_double(values.at(stationlon_idx).at(row));
      auto lat = get_double(values.at(stationlat_idx).at(row));

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

      auto dx = fmisid_shifts.at(fmisid).dx;
      auto dy = fmisid_shifts.at(fmisid).dy;

      Positions::Point pp{xpos, ypos, NFmiPoint(lon, lat), dx, dy};
      PointData pv{pp};
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
}  // namespace

PointValues read(State& state,
                 const std::vector<std::string>& parameters,
                 const Layer& layer,
                 const Positions& positions,
                 double maxdistance,
                 const Fmi::SpatialReference& crs,
                 const Fmi::Box& box,
                 const Fmi::DateTime& valid_time,
                 const Fmi::TimePeriod& valid_time_period)
{
  try
  {
    // Create the coordinate transformation from WGS84 coordinates to image CRS
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

    auto points = positions.getPoints(q, crs, box, forecast_mode, state);

    if (!points.empty())
    {
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
    }

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
