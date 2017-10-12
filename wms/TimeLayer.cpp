//======================================================================

#include "TimeLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Layer.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <ctpp2/CDT.hpp>
#include <gis/Box.h>
#include <spine/Exception.h>
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

void TimeLayer::init(const Json::Value& theJson,
                     const State& theState,
                     const Config& theConfig,
                     const Properties& theProperties)
{
  try
  {
    // Initialize this variable only once!
    now = boost::posix_time::second_clock::universal_time();

    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Time-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("timezone", nulljson);
    if (!json.isNull())
      timezone = json.asString();

    json = theJson.get("timestamp", nulljson);
    if (!json.isNull())
      timestamp = json.asString();

    json = theJson.get("format", nulljson);
    if (!json.isNull())
      format = json.asString();

    auto longitudeJson = theJson.get("longitude", nulljson);
    auto latitudeJson = theJson.get("latitude", nulljson);
    if (!longitudeJson.isNull() && !latitudeJson.isNull())
    {
      double longitude = longitudeJson.asDouble();
      double latitude = latitudeJson.asDouble();
      double xCoord = 0;
      double yCoord = 0;
      LonLatToXYTransformation transformation(projection);
      transformation.transform(longitude, latitude, xCoord, yCoord);
      x = xCoord;
      y = yCoord;
    }
    else
    {
      json = theJson.get("x", nulljson);
      if (!json.isNull())
        x = json.asInt();

      json = theJson.get("y", nulljson);
      if (!json.isNull())
        y = json.asInt();
    }
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

void TimeLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Time execution

    std::string report = "TimeLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // Establish the data
    auto q = getModel(theState);

    // Establish the valid time

    auto valid_time = getValidTime();

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Generate a <text>cdata<text> layer

    CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
    text_cdt["start"] = "<text";
    text_cdt["end"] = "</text>";

    // Add attributes to the text tag
    theState.addAttributes(theGlobals, text_cdt, attributes);

    // Override X and Y coordinates if so requested

    if (x || y)
    {
      projection.update(q);
      const auto& box = projection.getBox();

      if (x)
        text_cdt["attributes"]["x"] = ((*x) >= 0 ? (*x) : box.width() + (*x));
      if (y)
        text_cdt["attributes"]["y"] = ((*y) >= 0 ? (*y) : box.height() + (*y));
    }

    // Establish the text to be added

    auto tz =
        theState.getGeoEngine().getTimeZones().time_zone_from_string(timezone ? *timezone : "UTC");

    boost::local_time::local_date_time ltime(boost::posix_time::not_a_date_time);

    if (!timestamp || *timestamp == "validtime")
    {
      ltime = boost::local_time::local_date_time(valid_time, tz);
    }
    else if (*timestamp == "origintime")
    {
      if (!q)
        throw Spine::Exception(BCP, "Origintime not avaible for TimeLayer");
      ltime = boost::local_time::local_date_time(q->originTime(), tz);
    }
    else if (*timestamp == "wallclock" || *timestamp == "now")
    {
      ltime = boost::local_time::local_date_time(now, tz);
    }
    else
      throw Spine::Exception(BCP, "Unknown time-option in time-layer: '" + *timestamp + "'");

    std::ostringstream msg;
    std::string fmt = (format ? *format : "%Y-%m-%d %H:%M");

    auto* facet = new boost::posix_time::time_facet(fmt.c_str());
    msg.imbue(std::locale(msg.getloc(), facet));
    msg << ltime.local_time();

    text_cdt["cdata"] = msg.str();

    theLayersCdt.PushBack(text_cdt);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t TimeLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));
    boost::hash_combine(hash, Dali::hash_value(timezone));
    boost::hash_combine(hash, Dali::hash_value(timestamp));
    boost::hash_combine(hash, Dali::hash_value(format));
    boost::hash_combine(hash, Dali::hash_value(x));
    boost::hash_combine(hash, Dali::hash_value(y));
    boost::hash_combine(hash, Dali::hash_value(longitude));
    boost::hash_combine(hash, Dali::hash_value(latitude));

    // TODO: The timestamp to be drawn may depend on the wall clock - must include it
    // in the hash. A better solution would be to format the timestamp and
    // then calculate the hash.

    if (timestamp && (*timestamp == "wallclock" || *timestamp == "now"))
      boost::hash_combine(hash, Dali::hash_value(now));

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
