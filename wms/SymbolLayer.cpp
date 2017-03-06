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
#include <engines/querydata/Engine.h>

#include <gis/Box.h>
#include <gis/OGR.h>
#include <gis/Types.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <spine/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>

#include <boost/foreach.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>

#include <iomanip>

// TODO:
#include <boost/timer/timer.hpp>

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

PointValues read_forecasts(const SymbolLayer& layer,
                           Engine::Querydata::Q q,
                           const boost::shared_ptr<OGRSpatialReference>& crs,
                           const Fmi::Box& box,
                           const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    // Generate the coordinates for the symbols

    auto points = layer.positions->getPoints(q, crs, box);

    // querydata API for value() sucks

    boost::optional<Spine::Parameter> param;
    if (layer.parameter)
      param = Spine::ParameterFactory::instance().parse(*layer.parameter);

    boost::shared_ptr<Fmi::TimeFormatter> timeformatter(Fmi::TimeFormatter::create("iso"));
    boost::local_time::time_zone_ptr utc(new boost::local_time::posix_time_zone("UTC"));
    boost::local_time::local_date_time localdatetime(valid_time_period.begin(), utc);

    PointValues pointvalues;
    for (const auto& point : points)
    {
      if (layer.symbols.empty())
      {
        PointValue value{point, kFloatMissing};
        pointvalues.push_back(value);
      }
      else
      {
        Spine::Location loc(point.latlon.X(), point.latlon.Y());
        NFmiPoint dummy;

        // Q API SUCKS!!
        Engine::Querydata::ParameterOptions options(*param,
                                                    "",
                                                    loc,
                                                    "",
                                                    "",
                                                    *timeformatter,
                                                    "",
                                                    "",
                                                    std::locale::classic(),
                                                    "",
                                                    false,
                                                    NFmiPoint(),
                                                    dummy);

        auto result = q->value(options, localdatetime);
        if (boost::get<double>(&result))
        {
          PointValue value{point, *boost::get<double>(&result)};
          pointvalues.push_back(value);
        }
      }
    }
    return pointvalues;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Observation reader
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
PointValues read_observations(const SymbolLayer& layer,
                              State& state,
                              const boost::shared_ptr<OGRSpatialReference>& crs,
                              const Fmi::Box& box,
                              const boost::posix_time::time_period& valid_time_period)
{
  try
  {
    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *layer.producer;
    settings.latest = false;
    settings.timezone = "UTC";
    settings.numberofstations = 1;

    // settings.timestep = ?;

    settings.starttimeGiven = true;

    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    /*
     * Note:
     *
     * producer=flash
     *   - read all observations
     *   - read also latitude and longitude
     * producer=other
     *   - read latest observation from interval (not needed if starttime==endtime)
     *   - read also stationlon and stationlat
     *
     * Note: obsengine does not support fetcing the latest
     *       observation from a fixed interval.
     */

    auto& obsengine = state.getObsEngine();
    if (*layer.producer == "flash")
    {
      settings.parameters.push_back(obsengine.makeParameter("longitude"));
      settings.parameters.push_back(obsengine.makeParameter("latitude"));
    }
    else
    {
      settings.parameters.push_back(obsengine.makeParameter("stationlon"));
      settings.parameters.push_back(obsengine.makeParameter("stationlat"));
      settings.parameters.push_back(obsengine.makeParameter("fmisid"));
    }

    if (layer.parameter)
      settings.parameters.push_back(obsengine.makeParameter(*layer.parameter));

    // Request intersection parameters too - if any
    auto iparams = layer.positions->intersections.parameters();
    for (const auto& extraparam : iparams)
      settings.parameters.push_back(obsengine.makeParameter(extraparam));

    // Generate the coordinates for the symbols

    Engine::Querydata::Q q;
    auto points = layer.positions->getPoints(q, crs, box);

    // Coordinates or bounding box

    if (*layer.producer == "flash")
    {
      // TODO. Calculate these
      settings.boundingBox["minx"] = 10;
      settings.boundingBox["miny"] = 50;
      settings.boundingBox["maxx"] = 50;
      settings.boundingBox["maxy"] = 80;
    }
    else
    {
      // ObsEngine takes a rather strange vector of coordinates as input...

      using Coordinate = std::map<std::string, double>;
      settings.coordinates.reserve(points.size());
      for (const auto& point : points)
      {
        Coordinate coordinate{std::make_pair("lon", point.latlon.X()),
                              std::make_pair("lat", point.latlon.Y())};
        settings.coordinates.push_back(coordinate);
      }
    }

    Spine::ValueFormatterParam valueformatterparam;
    Spine::ValueFormatter valueformatter(valueformatterparam);

    auto result = obsengine.values(settings, valueformatter);

    // Build the pointvalues

    if (!result)
      return {};

    const auto& values = *result;
    if (values.empty())
      return {};

    // Create the coordinate transformation from image world coordinates
    // to WGS84 coordinates

    std::unique_ptr<OGRSpatialReference> obscrs(new OGRSpatialReference);
    OGRErr err = obscrs->SetFromUserInput("WGS84");
    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP, "GDAL does not understand WGS84");

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(crs.get(), obscrs.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(
          BCP, "Failed to create the needed coordinate transformation when drawing symbols");

    // We accept only the newest observation for each interval (except for flashes)
    // obsengine returns the data sorted by fmisid and by time

    std::string previous_fmisid;
    boost::posix_time::ptime previous_time;

    PointValues pointvalues;
    for (std::size_t row = 0; row < values[0].size(); ++row)
    {
      const auto& t = values.at(0).at(row).time;
      double lon = get_double(values.at(0).at(row));
      double lat = get_double(values.at(1).at(row));
      double value = kFloatMissing;
      std::string fmisid;
      int firstextraparam;  // which column holds the first extra parameter

      if (*layer.producer == "flash")
      {
        if (layer.parameter)
          value = get_double(values.at(2).at(row));
        firstextraparam = 3;
      }
      else
      {
        fmisid = boost::get<std::string>(values.at(2).at(row).value);
        if (layer.parameter)
          value = get_double(values.at(3).at(row));
        firstextraparam = 4;
      }

      // Collect extra values used for filtering the input

      Intersections::IntersectValues ivalues;

      for (std::size_t i = 0; i < iparams.size(); i++)
        ivalues[iparams.at(i)] = get_double(values.at(firstextraparam + i).at(row));

#if 0
      std::cout << fmisid << "\t" << t << "\t" << lon << "\t" << lat << "\t" << value;
      for (std::size_t i = 0; i < iparams.size(); i++)
        std::cout << "\t" << iparams.at(i) << "=" << ivalues.at(iparams.at(i));
      std::cout << std::endl;
#endif

      // Convert latlon to world coordinate if needed

      double x = lon;
      double y = lat;

      if (!crs->IsGeographic())
        if (!transformation->Transform(1, &x, &y))
          continue;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (x < 0 || x >= box.width() || y < 0 || y >= box.height())
        continue;

      // Skip if not inside/outside desired shapes or limits of other parameters
      if (!layer.positions->inside(lon, lat, ivalues))
        continue;

      int xpos = x;
      int ypos = y;

      // Keep only the latest value for each coordinate

      bool replace_previous = ((*layer.producer != "flash") && !pointvalues.empty() &&
                               (fmisid == previous_fmisid) && (t.utc_time() > previous_time));

      if (replace_previous)
        pointvalues.back().value = value;
      else
      {
        Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
        PointValue pv{point, value};
        pointvalues.push_back(pv);
      }

      previous_time = t.utc_time();
      previous_fmisid = fmisid;
    }

    return pointvalues;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(
        BCP, "SymbolLayer failed to read observations from the database", NULL);
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

    json = theJson.get("maxdistance", nulljson);
    if (!json.isNull())
      maxdistance = json.asDouble();

    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      symbol = json.asString();

    json = theJson.get("scale", nulljson);
    if (!json.isNull())
      scale = json.asDouble();

    json = theJson.get("symbols", nulljson);
    if (!json.isNull())
      SmartMet::Spine::JSON::extract_array("symbols", symbols, json, theConfig);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    if (theState.inDefs())
      throw SmartMet::Spine::Exception(BCP, "SymbolLayer cannot be used in the Defs-section");

    if (!validLayer(theState))
      return;

    // Time execution

    std::string report = "SymbolLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // A symbol must be defined either globally or for values

    if (!symbol && symbols.empty())
      throw SmartMet::Spine::Exception(
          BCP,
          "Must define default symbol with 'symbol' or several 'symbols' for specific values in a "
          "symbol-layer");

    // Establish the data

    bool use_observations = isObservation(theState);
    auto q = getModel(theState);

    // Make sure position generation is initialized

    if (!positions)
    {
      positions = Positions{};
      if (use_observations)
        positions->layout = Positions::Layout::Data;
    }

    // Establish the valid time

    auto valid_time_period = getValidTimePeriod();

    // Establish the level

    if (level)
    {
      if (use_observations)
        throw std::runtime_error("Cannot set level value for observations in SymbolLayer");

      bool match = false;
      for (q->resetLevel(); !match && q->nextLevel();)
        match = (q->levelValue() == *level);
      if (!match)
        throw SmartMet::Spine::Exception(
            BCP, "Level value " + Fmi::to_string(*level) + " is not available");
    }

    // Get projection details

    // bool has_data_proj = (projection.crs && *projection.crs == "data");
    projection.update(q);
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Set the proper symbol on if it is needed
    if (!symbols.empty())
    {
      if (!parameter)
        throw SmartMet::Spine::Exception(
            BCP, "Parameter not set for SymbolLayer even though multiple symbols are in use");
    }

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Initialize inside/outside shapes and intersection isobands

    positions->init(q, projection, valid_time_period.begin(), theState);

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    PointValues pointvalues;

    if (!use_observations)
      pointvalues = read_forecasts(*this, q, crs, box, valid_time_period);
#ifndef WITHOUT_OBSERVATION
    else
      pointvalues = read_observations(*this, theState, crs, box, valid_time_period);
#endif

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the symbols

    theState.addAttributes(theGlobals, group_cdt, attributes);

    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point;

      // Start generating the hash

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

      std::string iri;

      if (symbol)
        iri = *symbol;

      if (!symbols.empty())
      {
        auto selection = Select::attribute(symbols, pointvalue.value);
        if (selection)
        {
          if (selection->symbol)
            iri = *selection->symbol;
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

        std::string tmp = fmt::sprintf("translate(%d,%d)", point.x, point.y);
        if (scale)
          tmp += fmt::sprintf(" scale(%g)", *scale);

        tag_cdt["attributes"]["transform"] = tmp;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    if (isObservation(theState))
      return 0;

    auto hash = Layer::hash_value(theState);
    auto q = getModel(theState);
    if (q)
      boost::hash_combine(hash, SmartMet::Engine::Querydata::hash_value(q));

    boost::hash_combine(hash, Dali::hash_value(parameter));
    boost::hash_combine(hash, Dali::hash_value(level));
    boost::hash_combine(hash, Dali::hash_value(positions, theState));
    boost::hash_combine(hash, Dali::hash_value(maxdistance));
    boost::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    boost::hash_combine(hash, Dali::hash_value(scale));
    boost::hash_combine(hash, Dali::hash_value(symbols, theState));
    return hash;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
