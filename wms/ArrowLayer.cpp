//======================================================================
#include "ArrowLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include "ValueTools.h"

#include <spine/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <engines/gis/Engine.h>
#include <engines/querydata/Q.h>
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
#include <boost/math/constants/constants.hpp>
#include <fmt/format.h>
#include <iomanip>

// TODO:
#include <boost/timer/timer.hpp>

const double pi = boost::math::constants::pi<double>();

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Calculate direction of north on paper coordinates
 */
// ----------------------------------------------------------------------

double paper_north(const Engine::Querydata::Q& theQ, const NFmiPoint& theLatLon)
{
  if (!theQ)
    return 0;

  try
  {
    const NFmiPoint origo = theQ->area().ToXY(theLatLon);

    const double latstep = 0.001;  // degrees to north

    const double lat = theLatLon.Y() + latstep;

    // Safety against exceeding the north pole
    if (lat > 90)
      return 0;

    const NFmiPoint north = theQ->area().ToXY(NFmiPoint(theLatLon.X(), lat));
    const float alpha = static_cast<float>(atan2(origo.X() - north.X(), origo.Y() - north.Y()));
    return alpha * 180 / pi;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "ArrowLayer failed to calculate paper north", NULL);
  }
}

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
}

// ----------------------------------------------------------------------
/*!
 * \brief Forecast reader
 */
// ----------------------------------------------------------------------

PointValues read_forecasts(const ArrowLayer& layer,
                           Engine::Querydata::Q q,
                           const boost::shared_ptr<OGRSpatialReference>& crs,
                           const Fmi::Box& box,
                           const boost::posix_time::time_period& time_period)
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

  if (layer.direction && !layer.speed)
    speedparam = Spine::ParameterFactory::instance().parse("WindSpeedMS");

  if (speedparam && !q->param(speedparam->number()))
    throw Spine::Exception(
        BCP, "Parameter " + speedparam->name() + " not available in the arrow layer querydata");

  if (dirparam && !q->param(dirparam->number()))
    throw Spine::Exception(
        BCP, "Parameter " + dirparam->name() + " not available in the arrow layer querydata");

  if (uparam && !q->param(uparam->number()))
    throw Spine::Exception(
        BCP, "Parameter " + uparam->name() + " not available in the arrow layer querydata");

  if (vparam && !q->param(vparam->number()))
    throw Spine::Exception(
        BCP, "Parameter " + vparam->name() + " not available in the arrow layer querydata");

  // Generate the coordinates for the arrows

  const bool forecast_mode = true;
  auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

  PointValues pointvalues;

  for (const auto& point : points)
  {
    if (!layer.inside(box, point.x, point.y))
      continue;

    // Arrow direction and speed
    double wdir = kFloatMissing;
    double wspd = 0;

    if (uparam && vparam)
    {
      q->param(uparam->number());
      double uspd = q->interpolate(point.latlon, met_time, 180);  // TODO: magic constant
      q->param(vparam->number());
      double vspd = q->interpolate(point.latlon, met_time, 180);  // TODO: magic constant
      if (uspd != kFloatMissing && vspd != kFloatMissing)
      {
        wspd = sqrt(uspd * uspd + vspd * vspd);
        if (uspd != 0 || vspd != 0)
          wdir = fmod(180 + 180 / pi * atan2(uspd, vspd), 360);
      }
    }
    else
    {
      q->param(dirparam->number());
      wdir = q->interpolate(point.latlon, met_time, 180);  // TODO: magic constant
      if (speedparam)
      {
        q->param(speedparam->number());
        wspd = q->interpolate(point.latlon, met_time, 180);  // TODO: magic constant
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

// ----------------------------------------------------------------------
/*!
 * \brief Observation reader
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
PointValues read_observations(const ArrowLayer& layer,
                              State& state,
                              const boost::shared_ptr<OGRSpatialReference>& crs,
                              const Fmi::Box& box,
                              const boost::posix_time::time_period& valid_time_period)
{
  Engine::Observation::Settings settings;
  settings.starttime = valid_time_period.begin();
  settings.endtime = valid_time_period.end();
  settings.starttimeGiven = true;
  settings.stationtype = *layer.producer;
  settings.timezone = "UTC";
  settings.numberofstations = 1;
  settings.maxdistance = layer.maxdistance * 1000;  // obsengine uses meters

  // Get actual station coordinates plus the actual observation
  auto& obsengine = state.getObsEngine();
  settings.parameters.push_back(obsengine.makeParameter("stationlon"));
  settings.parameters.push_back(obsengine.makeParameter("stationlat"));
  settings.parameters.push_back(obsengine.makeParameter("fmisid"));

  bool use_uv_components = (layer.u && layer.v);

  if (use_uv_components)
  {
    settings.parameters.push_back(obsengine.makeParameter(*layer.u));
    settings.parameters.push_back(obsengine.makeParameter(*layer.v));
  }
  else
  {
    settings.parameters.push_back(obsengine.makeParameter(*layer.direction));
    if (layer.speed)
      settings.parameters.push_back(obsengine.makeParameter(*layer.speed));
    else
      settings.parameters.push_back(obsengine.makeParameter("WindSpeedMS"));
  }

  // Request intersection parameters too - if any
  const int firstextraparam = settings.parameters.size();
  auto iparams = layer.positions->intersections.parameters();

  for (const auto& extraparam : iparams)
    settings.parameters.push_back(obsengine.makeParameter(extraparam));

  // ObsEngine takes a rather strange vector of coordinates as input...

  if (layer.positions->layout == Positions::Layout::Data || *layer.producer == "flash")
  {
    settings.boundingBox = layer.getClipBoundingBox(box, crs);

    // TODO. Calculate these
    // settings.boundingBox["minx"] = 10;
    // settings.boundingBox["miny"] = 50;
    // settings.boundingBox["maxx"] = 50;
    // settings.boundingBox["maxy"] = 80;
  }
  else
  {
    Engine::Querydata::Q q;
    const bool forecast_mode = false;
    auto points = layer.positions->getPoints(q, crs, box, forecast_mode);

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

  // Build the expected output container for building the SVG

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
    throw Spine::Exception(BCP, "GDAL does not understand WGS84");

  std::unique_ptr<OGRCoordinateTransformation> transformation(
      OGRCreateCoordinateTransformation(crs.get(), obscrs.get()));
  if (!transformation)
    throw Spine::Exception(
        BCP, "Failed to create the needed coordinate transformation when drawing wind arrows");

  std::string previous_fmisid;
  boost::posix_time::ptime previous_time;

  PointValues pointvalues;
  for (std::size_t row = 0; row < values[0].size(); ++row)
  {
    const auto& t = values.at(0).at(row).time;
    double lon = get_double(values.at(0).at(row));
    double lat = get_double(values.at(1).at(row));
    std::string fmisid = boost::get<std::string>(values.at(2).at(2).value);

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

#if 0
    std::cout << fmisid << "\t" << t << "\t" << lon << "\t" << lat << "\t" << wspd << "," << wdir;
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
    if (layer.inside(box, x, y) && layer.positions->inside(lon, lat, ivalues))
    {
      // Keep only the latest value for each coordinate

      bool replace_previous = ((*layer.producer != "flash") && !pointvalues.empty() &&
                               (fmisid == previous_fmisid) && (t.utc_time() > previous_time));

      if (replace_previous)
      {
        pointvalues.back().direction = wdir;
        pointvalues.back().speed = wspd;
      }
      else
      {
        int xpos = x;
        int ypos = y;

        Positions::Point point{xpos, ypos, NFmiPoint(lon, lat)};
        PointValue pv{point, wdir, wspd};
        pointvalues.push_back(pv);
      }
    }

    previous_time = t.utc_time();
    previous_fmisid = fmisid;
  }
  return pointvalues;
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
      throw Spine::Exception(BCP, "Arrow-layer JSON is not a JSON object");

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

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      symbol = json.asString();

    json = theJson.get("scale", nulljson);
    if (!json.isNull())
      scale = json.asDouble();

    json = theJson.get("positions", nulljson);
    if (!json.isNull())
    {
      positions = Positions{};
      positions->init(json, theConfig);
    }

    json = theJson.get("maxdistance", nulljson);
    if (!json.isNull())
      maxdistance = json.asDouble();

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();

    json = theJson.get("arrows", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("arrows", arrows, json, theConfig);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    if (theState.inDefs())
      throw Spine::Exception(BCP, "ArrowLayer cannot be used in the Defs-section");

    if (!validLayer(theState))
      return;

    // Time execution

    std::string report = "ArrowLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // A symbol must be defined either globally or for speed ranges

    if (!symbol && arrows.empty())
      throw Spine::Exception(
          BCP, "Must define arrow with 'symbol' or 'arrows' to define the symbol for arrows");

    // Establish the data

    bool use_observations = isObservation(theState);
    Engine::Querydata::Q q = getModel(theState);

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
        throw std::runtime_error("Cannot set level value for observations in NumberLayer");

      bool match = false;
      for (q->resetLevel(); !match && q->nextLevel();)
        match = (q->levelValue() == *level);
      if (!match)
        throw Spine::Exception(BCP, "Level value " + Fmi::to_string(*level) + " is not available");
    }

    // Get projection details

    // bool has_data_proj = (projection.crs && *projection.crs == "data");
    projection.update(q);
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Initialize inside/outside shapes and intersection isobands

    positions->init(q, projection, valid_time_period.begin(), theState);

    // The parameters. TODO: Allow metaparameters, needs better Q API

    if ((u && (direction || speed)) || (v && (direction || speed)))
      throw Spine::Exception(
          BCP, "ArrowLayer cannot define direction, speed and u- and v-components simultaneously");

    if ((u && !v) || (v && !u))
      throw Spine::Exception(BCP, "ArrowLayer must define both U- and V-components");

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

    // Render the collected values

    for (const auto& pointvalue : pointvalues)
    {
      const auto& point = pointvalue.point;

      // Select arrow based on speed or U- and V-components, if available
      bool check_speeds = (!arrows.empty() && (speed || (u && v)));

      double wdir = pointvalue.direction;
      double wspd = pointvalue.speed;

      // Unit transformation
      double xmultiplier = (multiplier ? *multiplier : 1.0);
      double xoffset = (offset ? *offset : 0.0);
      wspd = xmultiplier * wspd + xoffset;

      // Where is north?
      double north = paper_north(q, point.latlon);
      double rotate = fmod(wdir + 180 - north, 360);

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<use";
      tag_cdt["end"] = "/>";

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
          theState.addAttributes(theGlobals, tag_cdt, selection->attributes);
        }
      }

      if (!iri.empty())
      {
        std::string IRI = Iri::normalize(iri);
        bool flop = southflop && (point.latlon.Y() < 0);

        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        tag_cdt["attributes"]["xlink:href"] = "#" + IRI;

        // Using x and y attributes messes up rotation, hence we
        // must use the translate transformation instead.
        std::string transform;

        double yscale = (scale ? *scale : 1.0);
        double xscale = (flop ? -yscale : yscale);

        if (xscale == 1 && yscale == 1)
          transform = fmt::sprintf("translate(%d %d) rotate(%d)",
                                   point.x,
                                   point.y,
                                   static_cast<int>(std::round(rotate)));
        else if (xscale == yscale)
          transform = fmt::sprintf("translate(%d %d) rotate(%d) scale(%g)",
                                   point.x,
                                   point.y,
                                   static_cast<int>(std::round(rotate)),
                                   xscale);
        else
          transform = fmt::sprintf("translate(%d %d) rotate(%d) scale(%g %g)",
                                   point.x,
                                   point.y,
                                   static_cast<int>(std::round(rotate)),
                                   xscale,
                                   yscale);

        tag_cdt["attributes"]["transform"] = transform;

        group_cdt["tags"].PushBack(tag_cdt);
      }
    }

    // We created only this one layer
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    if (isObservation(theState))
      return 0;

    auto hash = Layer::hash_value(theState);
    auto q = getModel(theState);
    if (q)
      boost::hash_combine(hash, Engine::Querydata::hash_value(q));
    boost::hash_combine(hash, Dali::hash_value(direction));
    boost::hash_combine(hash, Dali::hash_value(speed));
    boost::hash_combine(hash, Dali::hash_value(u));
    boost::hash_combine(hash, Dali::hash_value(v));
    boost::hash_combine(hash, Dali::hash_value(level));
    boost::hash_combine(hash, Dali::hash_value(multiplier));
    boost::hash_combine(hash, Dali::hash_value(offset));
    boost::hash_combine(hash, Dali::hash_symbol(symbol, theState));
    boost::hash_combine(hash, Dali::hash_value(scale));
    boost::hash_combine(hash, Dali::hash_value(southflop));
    boost::hash_combine(hash, Dali::hash_value(positions, theState));
    boost::hash_combine(hash, Dali::hash_value(maxdistance));
    boost::hash_combine(hash, Dali::hash_value(arrows, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
