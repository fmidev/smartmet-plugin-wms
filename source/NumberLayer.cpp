//======================================================================

#include "NumberLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Layer.h"
#include "Select.h"
#include "State.h"
#include <spine/Exception.h>
#include <spine/ParameterFactory.h>
#include <spine/Json.h>
#include <engines/gis/Engine.h>
#include <gis/Types.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <newbase/NFmiArea.h>
#include <newbase/NFmiPoint.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
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
      throw SmartMet::Spine::Exception(BCP, "Arrow-layer JSON is not a JSON object");

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

    json = theJson.get("label", nulljson);
    if (!json.isNull())
      label.init(json, theConfig);

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
    auto q = getModel(theState);

    // Establish the parameter

    if (!parameter)
      throw SmartMet::Spine::Exception(BCP, "Parameter not set for number-layer");

    auto param = SmartMet::Spine::ParameterFactory::instance().parse(*parameter);

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

    // Generate numbers as text layers inside <g>..</g>
    // Tags do not work, they do not have cdata enabled in the
    // template

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";

    // Add attributes to the group, not the text tags
    theState.addAttributes(theGlobals, group_cdt, attributes);

    theLayersCdt.PushBack(group_cdt);

    // Data conversion settings

    double xmultiplier = (multiplier ? *multiplier : 1.0);
    double xoffset = (offset ? *offset : 0.0);

    // Generate the coordinates for the numbers

    auto points = positions.getPoints(q, crs, box);

    // The parameters. This *must* be done after the call to positions generation

    if (!q->param(param.number()))
      throw SmartMet::Spine::Exception(
          BCP, "Parameter " + param.name() + " not available in the number layer querydata");

    for (const auto& point : points)
    {
      // Number direction and speed

      double value = q->interpolate(point.latlon, met_time, 180);  // TODO: magic constant

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, SmartMet::Engine::Querydata::hash_value(getModel(theState)));
    boost::hash_combine(hash, Dali::hash_value(parameter));
    boost::hash_combine(hash, Dali::hash_value(level));
    boost::hash_combine(hash, Dali::hash_value(multiplier));
    boost::hash_combine(hash, Dali::hash_value(offset));
    boost::hash_combine(hash, Dali::hash_value(positions, theState));
    boost::hash_combine(hash, Dali::hash_value(label, theState));
    boost::hash_combine(hash, Dali::hash_value(numbers, theState));
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
