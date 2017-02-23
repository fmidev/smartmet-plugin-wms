//======================================================================

#include <prettyprint.hpp>
#include <spine/TimeSeriesOutput.h>

#include "NumberLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include <spine/Exception.h>
#include <spine/ParameterFactory.h>
#include <spine/Json.h>
#include <spine/ValueFormatter.h>
#include <engines/gis/Engine.h>
#ifndef WITHOUT_OBSERVATION
#include <engines/observation/Engine.h>
#endif
#include <gis/Types.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
#include <iomanip>

#include <fmt/format.h>

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
      throw SmartMet::Spine::Exception(BCP, "Number-layer JSON is not a JSON object");

    // Iterate through all the members

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();

    json = theJson.get("positions", nulljson);
    if (!json.isNull())
      positions.init(json, theConfig);

    json = theJson.get("maxdistance", nulljson);
    if (!json.isNull())
      maxdistance = json.asDouble();

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
      SmartMet::Spine::JSON::extract_array("numbers", numbers, json, theConfig);
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

void NumberLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (theState.inDefs())
      throw SmartMet::Spine::Exception(BCP, "NumberLayer cannot be used in the Defs-section");

    if (!validLayer(theState))
      return;

    // Time execution

    std::string report = "NumberLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // Establish the data

    bool use_observations = isObservation(theState);
    SmartMet::Engine::Querydata::Q q = getModel(theState);

    // Establish the parameter

    if (!parameter)
      throw SmartMet::Spine::Exception(BCP, "Parameter not set for number-layer");

    // Establish the valid time

    if (!time)
      throw SmartMet::Spine::Exception(BCP, "Time has not been set for isoband-layer");

    auto valid_time = *time;
    if (time_offset)
      valid_time += boost::posix_time::minutes(*time_offset);

    // Do this conversion just once for speed:
    NFmiMetTime met_time = valid_time;

    // Establish the level

    if (level)
    {
      if (use_observations)
        throw std::runtime_error("Cannot set level value for observations in NumberLayer");

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

    // Initialize inside/outside shapes and intersection isobands

    positions.init(q, projection, valid_time, theState);

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Data conversion settings

    double xmultiplier = (multiplier ? *multiplier : 1.0);
    double xoffset = (offset ? *offset : 0.0);

    // Generate the coordinates for the numbers

    auto points = positions.getPoints(q, crs, box);

    // The parameters. This *must* be done after the call to positions generation

    auto param = SmartMet::Spine::ParameterFactory::instance().parse(*parameter);

    // Establish the numbers to draw. At this point we know that if
    // use_observations is true, obsengine is not disabled.

    struct PointValue
    {
      Positions::Point point;
      double value;
    };

    using PointValues = std::vector<PointValue>;
    PointValues pointvalues;

    if (!use_observations)
    {
      if (!q->param(param.number()))
        throw SmartMet::Spine::Exception(
            BCP, "Parameter " + param.name() + " not available in the number layer querydata");

      for (const auto& point : points)
      {
        PointValue value{point,
                         q->interpolate(point.latlon, met_time, 180)};  // TODO: magic constant
        pointvalues.push_back(value);
      }
    }
#ifndef WITHOUT_OBSERVATION
    else
    {
      Engine::Observation::Settings settings;
      settings.starttime = valid_time;
      settings.endtime = valid_time;
      settings.starttimeGiven = true;
      settings.stationtype = *producer;
      settings.timezone = "UTC";
      settings.numberofstations = 1;  // get only the nearest station for each coordinate
      settings.maxdistance = maxdistance * 1000;  // obsengine uses meters

      // Get actual station coordinates plus the actual observation
      auto& obsengine = theState.getObsEngine();
      settings.parameters.push_back(obsengine.makeParameter("stationlon"));
      settings.parameters.push_back(obsengine.makeParameter("stationlat"));
      settings.parameters.push_back(obsengine.makeParameter(*parameter));

      // ObsEngine takes a rather strange vector of coordinates as input...

      using Coordinate = std::map<std::string, double>;
      settings.coordinates.reserve(points.size());
      for (const auto& point : points)
      {
        Coordinate coordinate{std::make_pair("lon", point.latlon.X()),
                              std::make_pair("lat", point.latlon.Y())};
        settings.coordinates.push_back(coordinate);
      }

      Spine::ValueFormatterParam valueformatterparam;
      Spine::ValueFormatter valueformatter(valueformatterparam);

      auto result = obsengine.values(settings, valueformatter);

      // Store values for SVG generation

      if (result)
      {
        const auto& values = *result;
        if (!values.empty())
        {
          // Station WGS84 coordinates
          std::unique_ptr<OGRSpatialReference> wgs84(new OGRSpatialReference);
          OGRErr err = wgs84->SetFromUserInput("WGS84");

          if (err != OGRERR_NONE)
            throw SmartMet::Spine::Exception(BCP, "GDAL does not understand WKT 'WGS84'!");

          std::unique_ptr<OGRCoordinateTransformation> transformation(
              OGRCreateCoordinateTransformation(wgs84.get(), crs.get()));
          if (!transformation)
            throw SmartMet::Spine::Exception(
                BCP,
                "Failed to create the needed coordinate transformation for "
                "generating station positions");

          // TODO: Should refactor this using Positions methods
          // TODO: Should avoid getting the same stations twice??

          for (std::size_t row = 0; row < values[0].size(); ++row)
          {
            double lon = boost::get<double>(values.at(0).at(row).value);
            double lat = boost::get<double>(values.at(1).at(row).value);
            double val = boost::get<double>(values.at(2).at(row).value);

            // Convert latlon to world coordinate if needed

            double x = lon;
            double y = lat;

            if (!crs->IsGeographic())
              if (!transformation->Transform(1, &x, &y))
                continue;

            // To pixel coordinate
            box.transform(x, y);

            // Skip if not inside desired area
            if (x >= 0 && x < box.width() && y >= 0 && y < box.height())
            {
              if (positions.inside(lon, lat))
              {
                int xpos = x;
                int ypos = y;
                Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
                PointValue value{point, val};
                pointvalues.push_back(value);
              }
            }
          }
        }
      }
    }
#endif

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
    theLayersCdt.PushBack(group_cdt);

    // Then numbers

    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point;
      float value = pointvalue.value;

      if (value != kFloatMissing)
        value = xmultiplier * value + xoffset;

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
          theState.addAttributes(theGlobals, text_cdt, selection->attributes);

        text_cdt["attributes"]["x"] = Fmi::to_string(point.x + label.dx);
        text_cdt["attributes"]["y"] = Fmi::to_string(point.y + label.dy);
        theLayersCdt.PushBack(text_cdt);
      }
    }

    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Failed to generate NumberLayer", NULL);
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
    if (isObservation(theState))
      return 0;

    auto hash = Layer::hash_value(theState);
    auto q = getModel(theState);
    if (q)
      boost::hash_combine(hash, SmartMet::Engine::Querydata::hash_value(q));
    boost::hash_combine(hash, Dali::hash_value(producer));
    boost::hash_combine(hash, Dali::hash_value(parameter));
    boost::hash_combine(hash, Dali::hash_value(level));
    boost::hash_combine(hash, Dali::hash_value(multiplier));
    boost::hash_combine(hash, Dali::hash_value(offset));
    boost::hash_combine(hash, Dali::hash_value(positions, theState));
    boost::hash_combine(hash, Dali::hash_value(maxdistance));
    boost::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    boost::hash_combine(hash, Dali::hash_value(scale));
    boost::hash_combine(hash, Dali::hash_value(label, theState));
    boost::hash_combine(hash, Dali::hash_value(numbers, theState));
    return hash;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Calculating hash_value for the layer failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
