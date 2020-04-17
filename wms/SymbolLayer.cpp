//======================================================================
#include "SymbolLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Intersections.h"
#include "Iri.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"
#include <engines/gis/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/querydata/Engine.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <gis/Box.h>
#include <gis/CoordinateTransformation.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <spine/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <spine/ParameterTools.h>
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
struct PointValue
{
  Positions::Point point;
  double value;
};

using PointValues = std::vector<PointValue>;
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
                           const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    // Generate the coordinates for the symbols

    const bool forecast_mode = true;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

    // querydata API for value() sucks

    boost::optional<Spine::Parameter> param;
    if (layer.parameter)
      param = Spine::ParameterFactory::instance().parse(*layer.parameter);

    boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
    boost::local_time::local_date_time localdatetime(valid_time_period.begin(), utc);

    PointValues pointvalues;
    auto mylocale = std::locale::classic();
    NFmiPoint dummy;

    for (const auto& point : points)
    {
      if (!layer.inside(box, point.x, point.y))
        continue;

      if (layer.symbols.empty())
      {
        PointValue missingvalue{point, kFloatMissing};
        pointvalues.push_back(missingvalue);
      }
      else
      {
        Spine::Location loc(point.latlon.X(), point.latlon.Y());

        // Q API SUCKS!!
        Engine::Querydata::ParameterOptions options(
            *param, "", loc, "", "", *timeformatter, "", "", mylocale, "", false, dummy, dummy);

        auto result = q->value(options, localdatetime);
        if (boost::get<double>(&result) != nullptr)
        {
          double tmp = *boost::get<double>(&result);
          pointvalues.push_back(PointValue{point, tmp});
        }
        else if (boost::get<int>(&result) != nullptr)
        {
          double tmp = *boost::get<int>(&result);
          pointvalues.push_back(PointValue{point, tmp});
        }
        else
        {
          PointValue missingvalue{point, kFloatMissing};
          pointvalues.push_back(missingvalue);
        }
      }
    }
    return pointvalues;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Observation reader
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
PointValues read_flash_observations(const SymbolLayer& layer,
                                    State& state,
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

    settings.boundingBox = layer.getClipBoundingBox(box, crs);

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

      if (!crs.isGeographic())
        if (!transformation.transform(x, y) == 0)
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
    throw Spine::Exception::Trace(BCP, "SymbolLayer failed to read observations from the database");
  }
}

PointValues read_all_observations(const SymbolLayer& layer,
                                  State& state,
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

    settings.boundingBox = layer.getClipBoundingBox(box, crs);

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

      if (!crs.isGeographic())
        if (!transformation.transform(x, y))
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
    throw Spine::Exception::Trace(BCP, "SymbolLayer failed to read observations from the database");
  }
}

PointValues read_station_observations(const SymbolLayer& layer,
                                      State& state,
                                      const Fmi::SpatialReference& crs,
                                      const Fmi::Box& box,
                                      const boost::posix_time::time_period& valid_time_period,
                                      const Fmi::CoordinateTransformation& transformation)
{
  try
  {
    using Coordinate = std::map<std::string, double>;

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
      throw Spine::Exception(BCP, "Positions not defined for station-layout of numbers");

    // We must read the stations one at a time to preserve dx,dy values
    PointValues pointvalues;

    for (const auto& station : layer.positions->stations.stations)
    {
      // Copy Oracle settings
      auto opts = settings;

      // Use an unique ID first if specified, ignoring the coordinates even if set
      if (station.fmisid)
        opts.fmisids.push_back(*station.fmisid);
      else if (station.wmo)
        opts.wmos.push_back(*station.wmo);
      else if (station.lpnn)
        opts.lpnns.push_back(*station.lpnn);
      else if (station.geoid)
        opts.geoids.push_back(*station.geoid);
      else if (station.longitude && station.latitude)
      {
        Coordinate coordinate{std::make_pair("lon", *station.longitude),
                              std::make_pair("lat", *station.latitude)};
        opts.coordinates.push_back(coordinate);
      }
      else
        throw Spine::Exception(BCP, "Station ID or coordinate missing");

      auto result = obsengine.values(opts);

      if (!result || result->empty() || (*result)[0].empty())
        continue;

      const auto& values = *result;

      // We accept only the newest observation for each interval
      // obsengine returns the data sorted by fmisid and by time

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

      if (!crs.isGeographic())
        if (!transformation.transform(x, y))
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
    throw Spine::Exception::Trace(BCP, "SymbolLayer failed to read observations from the database");
  }
}

PointValues read_latlon_observations(const SymbolLayer& layer,
                                     State& state,
                                     const Fmi::SpatialReference& crs,
                                     const Fmi::Box& box,
                                     const boost::posix_time::time_period& valid_time_period,
                                     const Fmi::CoordinateTransformation& transformation,
                                     const Positions::Points& points)
{
  try
  {
    using Coordinate = std::map<std::string, double>;

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

      Coordinate coordinate{std::make_pair("lon", point.latlon.X()),
                            std::make_pair("lat", point.latlon.Y())};

      opts.coordinates.push_back(coordinate);
      auto result = obsengine.values(opts);

      if (!result || result->empty() || (*result)[0].empty())
        continue;

      const auto& values = *result;

      // We accept only the newest observation for each interval
      // obsengine returns the data sorted by fmisid and by time.
      // We've used latest=true, so we should get one row only

      auto row = values[0].size() - 1;
      if (row != 0)
        throw Spine::Exception(BCP, "Should have gotten row=0")
            .addParameter("row", Fmi::to_string(row));

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

      if (!crs.isGeographic())
        if (!transformation.transform(x, y))
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
    throw Spine::Exception::Trace(BCP, "SymbolLayer failed to read observations from the database");
  }
}

PointValues read_observations(const SymbolLayer& layer,
                              State& state,
                              const Fmi::SpatialReference& crs,
                              const Fmi::Box& box,
                              const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    // Create the coordinate transformation from image world coordinates
    // to WGS84 coordinates

    Fmi::CoordinateTransformation transformation("WGS84", crs);

    if (layer.isFlashOrMobileProducer(*layer.producer))
      return read_flash_observations(layer, state, crs, box, valid_time_period, transformation);

    if (layer.positions->layout == Positions::Layout::Station)
      return read_station_observations(layer, state, crs, box, valid_time_period, transformation);

    Engine::Querydata::Q q;
    const bool forecast_mode = false;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

    if (!points.empty())
      return read_latlon_observations(
          layer, state, crs, box, valid_time_period, transformation, points);

    return read_all_observations(layer, state, crs, box, valid_time_period, transformation);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "SymbolLayer failed to read observations from the database");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void SymbolLayer::init(const Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Symbol-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("positions", nulljson);
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }

    json = theJson.get("minvalues", nulljson);
    if (!json.isNull())
      minvalues = json.asInt();

    json = theJson.get("maxdistance", nulljson);
    if (!json.isNull())
      maxdistance = json.asDouble();

    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      symbol = json.asString();

    json = theJson.get("scale", nulljson);
    if (!json.isNull())
      scale = json.asDouble();

    json = theJson.get("dx", nulljson);
    if (!json.isNull())
      dx = json.asInt();

    json = theJson.get("dy", nulljson);
    if (!json.isNull())
      dy = json.asInt();

    json = theJson.get("symbols", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("symbols", symbols, json, theConfig);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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

    // Time execution
    std::string report = "SymbolLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // A symbol must be defined either globally or for values

    if (!symbol && symbols.empty())
      throw Spine::Exception(
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

    auto valid_time_period = (!is_legend ? getValidTimePeriod()
                                         : boost::posix_time::time_period(
                                               boost::posix_time::second_clock::local_time(),
                                               boost::posix_time::second_clock::local_time()));

    // Establish the level

    if (q && !q->firstLevel())
      throw Spine::Exception(BCP, "Unable to set first level in querydata.");

    if (level)
    {
      if (!q)
        throw Spine::Exception(BCP, "Cannot generate isobands without gridded level data");

      if (!q->selectLevel(*level))
        throw Spine::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available!");
    }

    // Get projection details

    projection.update(q);
    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Set the proper symbol on if it is needed
    if (!symbols.empty())
    {
      if (!parameter)
        throw Spine::Exception(
            BCP, "Parameter not set for SymbolLayer even though multiple symbols are in use");
    }

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Initialize inside/outside shapes and intersection isobands

    positions->init(producer, projection, valid_time_period.begin(), theState);

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

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the symbols

    theState.addAttributes(theGlobals, group_cdt, attributes);

    int valid_count = 0;

    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point;

      if (pointvalue.value != kFloatMissing)
        ++valid_count;

      // Start generating the hash

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      std::string iri;

      if (symbol)
        iri = *symbol;

      // librsvg cannot handle scale + transform, must move former into latter
      boost::optional<double> rescale;

      if (!symbols.empty())
      {
        auto selection = Select::attribute(symbols, pointvalue.value);
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
      throw Spine::Exception(BCP, "Too few valid values in symbol layer")
          .addParameter("valid values", std::to_string(valid_count))
          .addParameter("minimum count", std::to_string(minvalues));

    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    // Disable caching of observation layers
    if (theState.isObservation(producer))
      return invalid_hash;

    auto hash = Layer::hash_value(theState);
    auto q = getModel(theState);
    if (q)
      Dali::hash_combine(hash, Engine::Querydata::hash_value(q));

    Dali::hash_combine(hash, Dali::hash_value(parameter));
    Dali::hash_combine(hash, Dali::hash_value(level));
    Dali::hash_combine(hash, Dali::hash_value(positions, theState));
    Dali::hash_combine(hash, Dali::hash_value(minvalues));
    Dali::hash_combine(hash, Dali::hash_value(maxdistance));
    Dali::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    Dali::hash_combine(hash, Dali::hash_value(scale));
    Dali::hash_combine(hash, Dali::hash_value(dx));
    Dali::hash_combine(hash, Dali::hash_value(dy));
    Dali::hash_combine(hash, Dali::hash_value(symbols, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
