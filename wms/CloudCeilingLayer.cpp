//======================================================================
#ifndef WITHOUT_OBSERVATION

#include "CloudCeilingLayer.h"
#include "Config.h"
#include "Iri.h"
#include "JsonTools.h"
#include "PointData.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/printf.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/StringConversion.h>
#include <timeseries/ParameterTools.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
using PointValues = std::vector<PointData>;

// ----------------------------------------------------------------------
/*!
 * \brief Add the stations by keyword, fmisids or the bounding box to the settings
 */
// ----------------------------------------------------------------------

void CloudCeilingLayer::addStations(Engine::Observation::Settings& settings,
                                    const State& state,
                                    const Fmi::SpatialReference& crs,
                                    const Fmi::Box& box) const
{
  auto& obsengine = state.getObsEngine();

  // Check keyword
  if (!itsKeyword.empty())
  {
    const auto& geoengine = state.getGeoEngine();
    Locus::QueryOptions options;
    auto locations = geoengine.keywordSearch(options, itsKeyword);

    for (const auto& loc : locations)
    {
      if (loc->fmisid)
      {
        if (loc->longitude >= box.xmin() && loc->longitude <= box.xmax() &&
            loc->latitude >= box.ymin() && loc->latitude <= box.ymax())
          settings.taggedFMISIDs.emplace_back(Fmi::to_string(*loc->fmisid), *loc->fmisid);
      }
    }
  }

  // Check fmisid
  if (settings.taggedFMISIDs.empty() && !itsFMISIDs.empty())
  {
    for (const auto& fmisid : itsFMISIDs)
      settings.taggedFMISIDs.emplace_back(Fmi::to_string(fmisid), fmisid);
  }

  // Get stations using bounding box
  if (settings.taggedFMISIDs.empty())
  {
    // Coordinates or bounding box
    Engine::Observation::StationSettings stationSettings;
    stationSettings.bounding_box_settings = getClipBoundingBox(box, crs);
    settings.taggedFMISIDs = obsengine.translateToFMISID(settings, stationSettings);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief The point of the lowest cloud layer with at least 5/8 coverage, if any
 */
// ----------------------------------------------------------------------

std::optional<PointData> CloudCeilingLayer::ceilingPoint(
    const TS::TimeSeriesVector& values,
    std::size_t row,
    double lon,
    double lat,
    const Fmi::SpatialReference& crs,
    const Fmi::CoordinateTransformation& transformation,
    const Fmi::Box& box) const
{
  std::vector<double> cla_values;
  std::vector<double> clhb_values;
  cla_values.push_back(get_double(values.at(2).at(row)));
  cla_values.push_back(get_double(values.at(3).at(row)));
  cla_values.push_back(get_double(values.at(4).at(row)));
  cla_values.push_back(get_double(values.at(5).at(row)));
  cla_values.push_back(get_double(values.at(6).at(row)));
  clhb_values.push_back(get_double(values.at(7).at(row)));
  clhb_values.push_back(get_double(values.at(8).at(row)));
  clhb_values.push_back(get_double(values.at(9).at(row)));
  clhb_values.push_back(get_double(values.at(10).at(row)));
  clhb_values.push_back(get_double(values.at(11).at(row)));
  for (std::size_t i = 0; i < 5; i++)
  {
    auto cla_val = cla_values.at(i);
    auto clhb_val = clhb_values.at(i);

    if (clhb_val == kFloatMissing)
      continue;
    if (cla_val >= 5 && cla_val <= 9)
    {
      auto x = lon;
      auto y = lat;

      auto value = clhb_val;

      if (!crs.isGeographic())
        if (!transformation.transform(x, y))
          break;

      // To pixel coordinate
      box.transform(x, y);

      // Skip if not inside desired area
      if (!inside(box, x, y))
        break;

      int xpos = lround(x);
      int ypos = lround(y);

      Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
      PointData pv{point, value};
      return pv;
    }
  }
  return std::nullopt;
}

PointValues CloudCeilingLayer::readObservations(const State& state,
                                                const Fmi::SpatialReference& crs,
                                                const Fmi::Box& box,
                                                const Fmi::DateTime& valid_time,
                                                const Fmi::TimePeriod& valid_time_period) const
{
  try
  {
    // Create the coordinate transformation from image world coordinates
    // to WGS84 coordinates
    Fmi::CoordinateTransformation transformation("WGS84", crs);

    Engine::Observation::Settings settings;
    settings.allplaces = false;
    settings.stationtype = *paraminfo.producer;
    settings.wantedtime = valid_time;
    settings.timezone = "UTC";
    settings.numberofstations = 1;
    settings.maxdistance = maxdistance_km * 1000;  // obsengine uses meters

    settings.starttimeGiven = true;

    settings.starttime = valid_time_period.begin();
    settings.endtime = valid_time_period.end();

    auto& obsengine = state.getObsEngine();
    settings.parameters.push_back(TS::makeParameter("stationlon"));
    settings.parameters.push_back(TS::makeParameter("stationlat"));
    // Pilvikerroksen määrä
    settings.parameters.push_back(TS::makeParameter("CLA1_PT1M_ACC"));
    settings.parameters.push_back(TS::makeParameter("CLA2_PT1M_ACC"));
    settings.parameters.push_back(TS::makeParameter("CLA3_PT1M_ACC"));
    settings.parameters.push_back(TS::makeParameter("CLA4_PT1M_ACC"));
    settings.parameters.push_back(TS::makeParameter("CLA5_PT1M_ACC"));
    // Pilvikerroksen korkeus
    settings.parameters.push_back(TS::makeParameter("CLHB1_PT1M_INSTANT"));
    settings.parameters.push_back(TS::makeParameter("CLHB2_PT1M_INSTANT"));
    settings.parameters.push_back(TS::makeParameter("CLHB3_PT1M_INSTANT"));
    settings.parameters.push_back(TS::makeParameter("CLHB4_PT1M_INSTANT"));
    settings.parameters.push_back(TS::makeParameter("CLH5_PT1M_INSTANT"));

    auto debug = state.getRequest().getParameter("debug");
    settings.debug_options = (debug && Fmi::stoi(*debug) > 0);

    // GetFMISIDs
    addStations(settings, state, crs, box);

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
      auto point = ceilingPoint(values, row, lon, lat, crs, transformation, box);
      if (point)
        pointvalues.push_back(*point);
    }

    return pointvalues;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP,
                                "CloudCeilingLayer failed to read observations from the database");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void CloudCeilingLayer::init(Json::Value& theJson,
                             const State& theState,
                             const Config& theConfig,
                             const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "CloudCeilingLayer-layer JSON is not a JSON object");

    // Iterate through all the members

    NumberLayer::init(theJson, theState, theConfig, theProperties);

    // Stations of the layer
    JsonTools::remove_string(itsKeyword, theJson, "keyword");

    // If keyword is missing try 'fmisids'
    if (itsKeyword.empty())
    {
      auto json = JsonTools::remove(theJson, "fmisid");
      if (!json.isNull())
      {
        if (json.isString())
          itsFMISIDs.push_back(Fmi::stoi(json.asString()));
        else if (json.isArray())
        {
          for (const auto& j : json)
            itsFMISIDs.push_back(Fmi::stoi(j.asString()));
        }
        else
          throw Fmi::Exception(BCP, "fmisid value must be an array of strings or a string");
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void CloudCeilingLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    // Time execution

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "CloudCeilingLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Establish the data

    const bool use_observations = true;

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

    // Establish the valid time

    auto valid_time = getValidTime();
    auto valid_time_period = getValidTimePeriod();

    // Get projection details

    const auto& crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Initialize inside/outside shapes and intersection isobands

    positions->init(paraminfo.producer, projection, valid_time, theState);

    // Update the globals

    addCss(theGlobals, theState);

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    PointValues pointvalues;

    pointvalues = readObservations(theState, crs, box, valid_time, valid_time_period);

    pointvalues = prioritize(pointvalues, point_value_options);

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

    // Data conversion settings

    double xmultiplier = (multiplier ? *multiplier : 1.0);
    double xoffset = (offset ? *offset : 0.0);

    // Symbols first

    addSymbols(theGlobals, group_cdt, theState, pointvalues, xmultiplier, xoffset);
    theLayersCdt.PushBack(group_cdt);

    // Then numbers

    int valid_count =
        addNumbers(theGlobals, theLayersCdt, theState, pointvalues, xmultiplier, xoffset);

    if (valid_count < minvalues)
      throw Fmi::Exception(BCP, "Too few valid values in number layer")
          .addParameter("valid values", Fmi::to_string(valid_count))
          .addParameter("minimum count", Fmi::to_string(minvalues));

    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate CloudCeilingLayer!")
        .addParameter("qid", qid)
        .addParameter("Producer", *paraminfo.producer)
        .addParameter("Parameter", paraminfo.parameter);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief GetFeatureInfo
 */
// ----------------------------------------------------------------------

void CloudCeilingLayer::getFeatureInfo(CTPP::CDT& /* theInfo */, const State& /* theState */)
{
  // TODO();
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif  // WITHOUT_OBSERVATION
