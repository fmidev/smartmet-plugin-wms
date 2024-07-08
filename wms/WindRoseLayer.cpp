#ifndef WITHOUT_OBSERVATION

#include "WindRoseLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "JsonTools.h"
#include "Layer.h"
#include "Observations.h"
#include "Select.h"
#include "State.h"
#include "Stations.h"
#include "WindRose.h"
#include <boost/math/constants/constants.hpp>
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/Engine.h>
#include <engines/observation/Engine.h>
#include <engines/observation/Settings.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <spine/Json.h>
#include <timeseries/ParameterTools.h>
#include <timeseries/TimeSeriesInclude.h>
#include <algorithm>
#include <ogr_spatialref.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct value_printer : public boost::static_visitor<std::string>
{
  std::string operator()(const std::string& str) const { return str; }
  std::string operator()(double value) const { return Fmi::to_string(value); }
  std::string operator()(const TS::LonLat& lonlat) const
  {
    return Fmi::to_string(lonlat.lon) + ',' + Fmi::to_string(lonlat.lat);
  }

  std::string operator()(const Fmi::LocalDateTime& t) const
  {
    return Fmi::to_iso_string(t.local_time());
  }
};

// ----------------------------------------------------------------------
/*!
 * \brief Rose data is valid if the maximum gap in the time series is 1 hour
 */
// ----------------------------------------------------------------------

bool is_rose_data_valid(const TS::TimeSeries& directions,
                        const TS::TimeSeries& speeds,
                        const TS::TimeSeries& temperatures)
{
  try
  {
    // Trivial case:
    if (directions.empty() || speeds.empty() || temperatures.empty())
      return false;

    // Safety check, the sizes should really be always equal

    if (directions.size() != speeds.size() || speeds.size() != temperatures.size())
      throw Fmi::Exception(BCP, "Internal failure in wind rose observations, data size mismatch");

    bool first = true;
    Fmi::DateTime previous_valid_time;

    Fmi::TimeDuration timelimit(0, 60, 0, 0);

    for (std::size_t i = 0; i < directions.size(); i++)
    {
      const auto t = directions[i].time.utc_time();
      const double* dir = boost::get<double>(&directions[i].value);
      const double* spd = boost::get<double>(&speeds[i].value);
      const double* t2m = boost::get<double>(&temperatures[i].value);

      if (dir != nullptr && spd != nullptr && t2m != nullptr &&
          boost::get<double>(directions[i].value) != kFloatMissing &&
          boost::get<double>(speeds[i].value) != kFloatMissing &&
          boost::get<double>(temperatures[i].value) != kFloatMissing)
      {
        if (first)
          first = false;
        else if (t - previous_valid_time > timelimit)
          return false;
        previous_valid_time = t;
      }
      else if (!first)
      {
        if (t - previous_valid_time > timelimit)
          return false;
      }
    }

    // We must return false if there was no valid data at all
    return !first;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate mean value of time series
 */
// ----------------------------------------------------------------------

double mean(const TS::TimeSeries& tseries)
{
  try
  {
    double sum = 0;
    int count = 0;
    for (const auto& tv : tseries)
    {
      const double* value = boost::get<double>(&tv.value);
      if (value != nullptr && *value != kFloatMissing)
      {
        sum += *value;
        ++count;
      }
    }
    if (count == 0)
      return kFloatMissing;
    return sum / count;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate max value of time series
 */
// ----------------------------------------------------------------------

double max(const TS::TimeSeries& tseries)
{
  try
  {
    double res = 0;
    bool valid = false;
    for (const auto& tv : tseries)
    {
      const double* value = boost::get<double>(&tv.value);
      if (value != nullptr && *value != kFloatMissing)
      {
        if (!valid)
          res = *value;
        else
          res = std::max(res, *value);
        valid = true;
      }
    }
    if (!valid)
      return kFloatMissing;
    return res;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate rose sector
 *
 * With 8 sectors we have
 *
 *    step = 360/8 = 45 and thus -22.5...22.5 is sector 0 and so on
 */
// ----------------------------------------------------------------------

int rose_sector(int sectors, double direction)
{
  try
  {
    return lround(sectors * direction / 360.0) % sectors;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate sector start angle
 */
// ----------------------------------------------------------------------

double sector_start_angle(int sector, int sectors)
{
  try
  {
    return fmod((sector - 0.5) * 360.0 / sectors, 360);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate sector start angle
 */
// ----------------------------------------------------------------------

double sector_end_angle(int sector, int sectors)
{
  try
  {
    return fmod((sector + 0.5) * 360.0 / sectors, 360);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate wind rose distribution
 */
// ----------------------------------------------------------------------

std::vector<double> calculate_rose_distribution(const TS::TimeSeries& directions, int sectors)
{
  try
  {
    std::vector<int> counts(sectors, 0);
    int total_count = 0;

    std::vector<double> result(sectors, 0);

    for (const auto& tv : directions)
    {
      const double* value = boost::get<double>(&tv.value);

      if (value != nullptr && *value != kFloatMissing)
      {
        ++counts[rose_sector(sectors, *value)];
        ++total_count;
      }
    }

    if (total_count > 0)
    {
      for (int i = 0; i < sectors; i++)
        result[i] = 100.0 * counts[i] / total_count;
    }

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Calculate wind rose maxima
 */
// ----------------------------------------------------------------------

std::vector<double> calculate_rose_maxima(const TS::TimeSeries& directions,
                                          const TS::TimeSeries& speeds,
                                          int sectors)
{
  try
  {
    std::vector<double> result(sectors, 0);

    for (std::size_t i = 0; i < directions.size(); i++)
    {
      const double* dir = boost::get<double>(&directions[i].value);
      const double* spd = boost::get<double>(&speeds[i].value);
      if (dir != nullptr && spd != nullptr && *dir != kFloatMissing && *spd != kFloatMissing)
      {
        int sector = rose_sector(sectors, *dir);
        result[sector] = std::max(result[sector], *spd);
      }
    }

    return result;
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

void WindRoseLayer::init(Json::Value& theJson,
                         const State& theState,
                         const Config& theConfig,
                         const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "WindRose-layer JSON is not a JSON hash");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(timezone, theJson, "timezone");
    JsonTools::remove_int(starttimeoffset, theJson, "starttimeoffset");
    JsonTools::remove_int(endtimeoffset, theJson, "endtimeoffset");

    auto json = JsonTools::remove(theJson, "windrose");
    if (!json.isNull())
      windrose.init(json, theConfig);

    json = JsonTools::remove(theJson, "observations");
    if (!json.isNull())
      observations.init(json, theConfig);

    json = JsonTools::remove(theJson, "stations");
    if (!json.isNull())
      stations.init(json, theConfig);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 *
 * The generated SVG is something like this:
 *
 * <g class="WindRoses">
 *  <use x="" y="" class="StationSymbol" xlink:href="#StationSymbol"/>
 *  <use x="" y="" class="WindRoseSymbol" xlink:href="#RoseSymbol"/>
 *  <path class="LightWind" d="M0 0 Lx1 y1 A rx ry 1 1 x y Z"/>
 *  <path class="ModerateWind" d="M0 0 Lx1 y1 A rx ry 1 1 x y Z"/>
 *  ...
 *  <line x1="" y1="" x2="" y2="" class="StationRoseLine"/>
 *  <text x="" y="" class="RoseTemperature">15 &deg;C</text>
 *
 *  ...repeat above for each station...
 * </g>
 */
// ----------------------------------------------------------------------

void WindRoseLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  if (theState.getConfig().obsEngineDisabled())
    throw std::runtime_error("Cannot use WindRoseLayer when the observation engine is disabled");

  try
  {
    if (theState.inDefs())
      throw Fmi::Exception(BCP, "WindRoseLayer cannot be used in the defs section");

    if (!validLayer(theState))
      return;

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "WindRoseLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Establish the projection

    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Create the coordinate transformation from latlon to world coordinates

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    // Establish the time range

    auto valid_time = getValidTime();

    // TODO(mheiskan): use interval_start and interval_end instead
    auto starttime = valid_time + Fmi::Hours(starttimeoffset);
    auto endtime = valid_time + Fmi::Hours(endtimeoffset);

    auto rosedata = getObservations(theState, starttime, endtime);

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, box, theState);

    // Generate wind roses inside a <g>...</g>

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Add attributes to the group, not the wind roses
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Collect the text layers separately
    std::list<CTPP::CDT> text_layers;

    for (const auto& station : stations.stations)
    {
      // Currently we require the station to have a fmisid
      if (!station.fmisid)
        throw Fmi::Exception(BCP, "WindRose station fmisid missing");

      // Find the observations for the station
      auto it = rosedata.find(*station.fmisid);
      if (it == rosedata.end())
        continue;

      const WindRoseData& wdata = it->second;

      // Where to center the rose
      double lon = (station.longitude ? *station.longitude : wdata.longitude);
      double lat = (station.latitude ? *station.latitude : wdata.latitude);

      // The respective image coordinate

      double x = lon;
      double y = lat;
      transformation.transform(x, y);
      box.transform(x, y);
      int xrose = lround(x);
      int yrose = lround(y);

      // Where the station is located

      x = wdata.longitude;
      y = wdata.latitude;
      transformation.transform(x, y);
      box.transform(x, y);
      int xstation = lround(x);
      int ystation = lround(y);

      // Generate marker for station if so desired

      if (station.symbol && !station.symbol->empty())
      {
        CTPP::CDT station_cdt(CTPP::CDT::HASH_VAL);
        station_cdt["start"] = "<use";
        station_cdt["end"] = "/>";

        const std::string& iri = *station.symbol;
        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        theState.addAttributes(theGlobals, station_cdt, station.attributes);

        station_cdt["attributes"]["xlink:href"] = "#" + IRI;
        station_cdt["attributes"]["x"] = Fmi::to_string(xstation);
        station_cdt["attributes"]["y"] = Fmi::to_string(ystation);

        group_cdt["tags"].PushBack(station_cdt);
      }

      // Generate the symbol for the wind rose

      if (windrose.symbol && !windrose.symbol->empty())
      {
        CTPP::CDT rose_cdt(CTPP::CDT::HASH_VAL);
        rose_cdt["start"] = "<use";
        rose_cdt["end"] = "/>";

        const std::string& iri = *windrose.symbol;
        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        theState.addAttributes(theGlobals, rose_cdt, windrose.attributes);

        rose_cdt["attributes"]["xlink:href"] = "#" + IRI;
        rose_cdt["attributes"]["x"] = Fmi::to_string(xrose);
        rose_cdt["attributes"]["y"] = Fmi::to_string(yrose);

        group_cdt["tags"].PushBack(rose_cdt);
      }

      // Draw the wind rose sectors

      if (wdata.valid)
      {
        const double pi = boost::math::constants::pi<double>();
        for (int i = 0; i < windrose.sectors; i++)
        {
          double max_percentage =
              *std::max_element(wdata.percentages.begin(), wdata.percentages.end());

          if (wdata.percentages[i] >= windrose.minpercentage)
          {
            // Note: The largest sector always reaches the circle edge, hence we must
            // use a percentage relative to the largest sector
            double relative_percentage = wdata.percentages[i] / max_percentage;
            // Note: The areas scale as a square of the radius
            double r = windrose.radius * sqrt(relative_percentage);

            double angle1 = sector_start_angle(i, windrose.sectors) * pi / 180;
            double angle2 = sector_end_angle(i, windrose.sectors) * pi / 180;

            // Note that for winds angle zero is north wind, but in SVG coordinates
            // angle 0 is to the right.
            double x1 = xrose + r * cos(angle1 - pi / 2);
            double y1 = yrose + r * sin(angle1 - pi / 2);
            double x2 = xrose + r * cos(angle2 - pi / 2);
            double y2 = yrose + r * sin(angle2 - pi / 2);

            CTPP::CDT sector_cdt(CTPP::CDT::HASH_VAL);
            sector_cdt["start"] = "<path";
            sector_cdt["end"] = "/>";

            auto selection = Select::attribute(windrose.limits, wdata.max_winds[i]);
            if (selection)
              theState.addAttributes(theGlobals, sector_cdt, selection->attributes);

            std::string path =
                std::string("M") + Fmi::to_string(xrose) + ' ' + Fmi::to_string(yrose) + ' ' +
                Fmi::to_string(x1) + ',' + Fmi::to_string(y1) + " A " + Fmi::to_string(r) + ' ' +
                Fmi::to_string(r) + " 0,0,1 " + Fmi::to_string(x2) + ',' + Fmi::to_string(y2) + 'Z';

            sector_cdt["attributes"]["d"] = path;

            group_cdt["tags"].PushBack(sector_cdt);
          }
        }
      }

      // Connect the station to the wind rose

      if (windrose.connector)
      {
        int x1 = xstation;
        int y1 = ystation;
        int x2 = xrose;
        int y2 = yrose;
        bool ok = true;

        // Clip from the station end if so requested
        if (windrose.connector->startoffset > 0)
        {
          double length = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
          double alpha = windrose.connector->startoffset / length;
          if (alpha >= 1)
            ok = false;
          else
          {
            x1 += lround(alpha * (x2 - x1));
            y1 += lround(alpha * (y2 - y1));
          }
        }

        // Clip from the rose end if so requested
        if (ok && windrose.connector->endoffset > 0)
        {
          double length = sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
          double alpha = windrose.connector->endoffset / length;
          if (alpha >= 1)
            ok = false;
          else
          {
            x2 += lround(alpha * (x1 - x2));
            y2 += lround(alpha * (y1 - y2));
          }
        }

        // And render if there is something left to draw
        if (ok)
        {
          CTPP::CDT line_cdt(CTPP::CDT::HASH_VAL);
          line_cdt["start"] = "<line";
          line_cdt["end"] = "/>";

          theState.addAttributes(theGlobals, line_cdt, windrose.connector->attributes);

          line_cdt["attributes"]["x1"] = x1;
          line_cdt["attributes"]["y1"] = y1;
          line_cdt["attributes"]["x2"] = x2;
          line_cdt["attributes"]["y2"] = y2;

          group_cdt["tags"].PushBack(line_cdt);
        }
      }

      // Draw the title

      if (station.title)
      {
        CTPP::CDT title_cdt(CTPP::CDT::HASH_VAL);
        title_cdt["start"] = "<text";
        title_cdt["end"] = "</text>";
        title_cdt["cdata"] = station.title->translate(language);

        theState.addAttributes(theGlobals, title_cdt, station.title->attributes);
        title_cdt["attributes"]["x"] = Fmi::to_string(xrose + station.title->dx);
        title_cdt["attributes"]["y"] = Fmi::to_string(yrose + station.title->dy);

        text_layers.push_back(title_cdt);
      }

      // Draw the observations

      if (wdata.valid)
      {
        for (const auto& observation : observations.observations)
        {
          double value = 0;
          if (observation.parameter == "mean_t(T)")
            value = wdata.mean_temperature;
          else if (observation.parameter == "mean_t(ws_10min)")
            value = wdata.mean_wind;
          else if (observation.parameter == "max_t(ws_10min)")
            value = wdata.max_wind;
          else
            throw Fmi::Exception(BCP,
                                 "Unknown WindRoseLayer parameter '" + observation.parameter + "'");

          CTPP::CDT obs_cdt(CTPP::CDT::HASH_VAL);
          obs_cdt["start"] = "<text";
          obs_cdt["end"] = "</text>";
          obs_cdt["cdata"] = observation.label.print(value);

          theState.addAttributes(theGlobals, obs_cdt, observation.attributes);
          obs_cdt["attributes"]["x"] = Fmi::to_string(xrose + observation.label.dx);
          obs_cdt["attributes"]["y"] = Fmi::to_string(yrose + observation.label.dy);

          text_layers.push_back(obs_cdt);
        }
      }
    }

    theLayersCdt.PushBack(group_cdt);
    for (const auto& cdt : text_layers)
      theLayersCdt.PushBack(cdt);

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
 * \brief Read the observations
 */
// ----------------------------------------------------------------------

std::map<int, WindRoseData> WindRoseLayer::getObservations(
    State& theState,
    const Fmi::DateTime& theStartTime,
    const Fmi::DateTime& theEndTime) const
{
  try
  {
    // Oracle settings
    Engine::Observation::Settings settings;

    settings.starttime = theStartTime;
    settings.endtime = theEndTime;
    settings.starttimeGiven = true;
    settings.stationtype = "observations_fmi";
    settings.timezone = timezone;
    settings.useCommonQueryMethod = true;

    auto& observation = theState.getObsEngine();
    settings.parameters.push_back(TS::makeParameter("WindDirection"));
    settings.parameters.push_back(TS::makeParameter("WindSpeedMS"));
    settings.parameters.push_back(TS::makeParameter("t2m"));
    settings.parameters.push_back(TS::makeParameter("stationlongitude"));
    settings.parameters.push_back(TS::makeParameter("stationlatitude"));

    // settings.debug_options = Engine::Observation::Settings::DUMP_SETTINGS;

    std::map<int, WindRoseData> result;

    for (const auto& station : stations.stations)
    {
      settings.taggedFMISIDs.clear();
      if (!station.fmisid)
        throw Fmi::Exception(BCP, "Station fmisid is required for wind roses");

      settings.taggedFMISIDs.emplace_back(Fmi::to_string(*station.fmisid), *station.fmisid);

      auto res = observation.values(settings);

      // We skip stations with missing data

      if (!res || res->size() != 5)
        continue;

      const auto& values = *res;

      const auto& directions = values[0];
      const auto& speeds = values[1];
      const auto& temperatures = values[2];
      const auto& longitudes = values[3];
      const auto& latitudes = values[4];

      if (latitudes.empty() || longitudes.empty() || temperatures.empty() || speeds.empty() ||
          directions.empty())
      {
        // No data for this station, continue to the next
        std::cerr << "No data for " << *station.fmisid << std::endl;

        continue;
      }

      WindRoseData rosedata;
      rosedata.mean_wind = mean(speeds);
      rosedata.max_wind = max(speeds);
      rosedata.mean_temperature = mean((*res)[2]);

      rosedata.longitude = boost::get<double>(longitudes[0].value);
      rosedata.latitude = boost::get<double>(latitudes[0].value);

      rosedata.percentages = calculate_rose_distribution(directions, windrose.sectors);
      rosedata.max_winds = calculate_rose_maxima(directions, speeds, windrose.sectors);
      rosedata.valid = is_rose_data_valid(directions, speeds, temperatures);

      result[*station.fmisid] = rosedata;
    }

    return result;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t WindRoseLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(timezone));
    Fmi::hash_combine(hash, Fmi::hash_value(starttimeoffset));
    Fmi::hash_combine(hash, Fmi::hash_value(endtimeoffset));
    Fmi::hash_combine(hash, Dali::hash_value(windrose, theState));
    Fmi::hash_combine(hash, Dali::hash_value(observations, theState));
    Fmi::hash_combine(hash, Dali::hash_value(stations, theState));
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

#endif  // WITHOUT_OBSERVATION
