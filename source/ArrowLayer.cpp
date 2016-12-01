//======================================================================

#include "ArrowLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include <spine/Exception.h>
#include <spine/Json.h>
#include <spine/ParameterFactory.h>
#include <engines/gis/Engine.h>
#include <gis/Types.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
#include <boost/math/constants/constants.hpp>
#include <cppformat/format.h>  // cppformat
#include <iomanip>

// TODO:
#include <boost/timer/timer.hpp>

// ----------------------------------------------------------------------
/*!
 * \brief Calculate direction of north on paper coordinates
 */
// ----------------------------------------------------------------------

double paper_north(const NFmiArea& theArea, const NFmiPoint& theLatLon)
{
  try
  {
    const NFmiPoint origo = theArea.ToXY(theLatLon);

    const double pi = 3.141592658979323;
    const double latstep = 0.001;  // degrees to north

    const double lat = theLatLon.Y() + latstep;

    // Safety against exceeding the north pole
    if (lat > 90)
      return 0;

    const NFmiPoint north = theArea.ToXY(NFmiPoint(theLatLon.X(), lat));
    const float alpha = static_cast<float>(atan2(origo.X() - north.X(), origo.Y() - north.Y()));
    return alpha * 180 / pi;
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

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

void ArrowLayer::init(const Json::Value& theJson,
                      const State& theState,
                      const Config& theConfig,
                      const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw SmartMet::Spine::Exception(BCP, "Arrow-layer JSON is not a JSON object");

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
      positions.init(json, theConfig);

    json = theJson.get("multiplier", nulljson);
    if (!json.isNull())
      multiplier = json.asDouble();

    json = theJson.get("offset", nulljson);
    if (!json.isNull())
      offset = json.asDouble();

    json = theJson.get("arrows", nulljson);
    if (!json.isNull())
      SmartMet::Spine::JSON::extract_array("arrows", arrows, json, theConfig);
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

void ArrowLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (theState.inDefs())
      throw SmartMet::Spine::Exception(BCP, "ArrowLayer cannot be used in the Defs-section");

    if (!validLayer(theState))
      return;

    // Time execution

    std::string report = "ArrowLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // A symbol must be defined either globally or for speed ranges

    if (!symbol && arrows.empty())
      throw SmartMet::Spine::Exception(
          BCP, "Must define arrow with 'symbol' or 'arrows' to define the symbol for arrows");

    // Establish the data
    auto q = getModel(theState);

    // Establish the valid time

    if (!time)
      throw SmartMet::Spine::Exception(BCP, "Time has not been set for arrow-layer");

    auto valid_time = *time;
    if (time_offset)
      valid_time += boost::posix_time::minutes(*time_offset);

    // Do the conversion just ones, not in every call to interpolate...
    NFmiMetTime met_time = valid_time;

    // Establish the level

    if (level)
    {
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

    // Get the data projection

    std::unique_ptr<OGRSpatialReference> qcrs(new OGRSpatialReference);
    OGRErr err = qcrs->SetFromUserInput(q->area().WKT().c_str());
    if (err != OGRERR_NONE)
      throw SmartMet::Spine::Exception(BCP,
                                       "GDAL does not understand this FMI WKT: " + q->area().WKT());

    // Create the coordinate transformation from image world coordinates
    // to querydata world coordinates

    std::unique_ptr<OGRCoordinateTransformation> transformation(
        OGRCreateCoordinateTransformation(crs.get(), qcrs.get()));
    if (!transformation)
      throw SmartMet::Spine::Exception(
          BCP, "Failed to create the needed coordinate transformation when drawing wind arrows");

    // Initialize inside/outside shapes and intersection isobands

    positions.init(q, projection, valid_time, theState);

    // The parameters. TODO: Allow metaparameters, needs better Q API

    boost::optional<SmartMet::Spine::Parameter> dirparam, speedparam, uparam, vparam;

    if (direction)
      dirparam = SmartMet::Spine::ParameterFactory::instance().parse(*direction);
    if (speed)
      speedparam = SmartMet::Spine::ParameterFactory::instance().parse(*speed);
    if (u)
      uparam = SmartMet::Spine::ParameterFactory::instance().parse(*u);
    if (v)
      vparam = SmartMet::Spine::ParameterFactory::instance().parse(*v);

    if ((u && (direction || speed)) || (v && (direction || speed)))
      throw SmartMet::Spine::Exception(
          BCP, "ArrowLayer cannot define direction, speed and u- and v-components simultaneously");

    if ((u && !v) || (v && !u))
      throw SmartMet::Spine::Exception(BCP, "ArrowLayer must define both U- and V-components");

    if (direction && !speed)
      speedparam = SmartMet::Spine::ParameterFactory::instance().parse("WindSpeedMS");

    if (speed && !q->param(speedparam->number()))
      throw SmartMet::Spine::Exception(
          BCP, "Parameter " + speedparam->name() + " not available in the arrow layer querydata");

    if (dirparam && !q->param(dirparam->number()))
      throw SmartMet::Spine::Exception(
          BCP, "Parameter " + dirparam->name() + " not available in the arrow layer querydata");

    if (uparam && !q->param(uparam->number()))
      throw SmartMet::Spine::Exception(
          BCP, "Parameter " + uparam->name() + " not available in the arrow layer querydata");

    if (vparam && !q->param(vparam->number()))
      throw SmartMet::Spine::Exception(
          BCP, "Parameter " + vparam->name() + " not available in the arrow layer querydata");

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Begin a G-group, put arrows into it as tags

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "</g>";

    // Add layer attributes to the group, not to the arrows
    theState.addAttributes(theGlobals, group_cdt, attributes);

    // Generate the coordinates for the arrows

    auto points = positions.getPoints(q, crs, box);

    const double pi = boost::math::constants::pi<double>();

    for (const auto& point : points)
    {
      // Select arrow based on speed or U- and V-components, if available
      bool check_speeds = (!arrows.empty() && (speed || (u && v)));

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

      // Unit transformation
      double xmultiplier = (multiplier ? *multiplier : 1.0);
      double xoffset = (offset ? *offset : 0.0);
      wspd = xmultiplier * wspd + xoffset;

      // Where is north?
      double north = paper_north(q->area(), point.latlon);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, SmartMet::Engine::Querydata::hash_value(getModel(theState)));
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
    boost::hash_combine(hash, Dali::hash_value(arrows, theState));
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
