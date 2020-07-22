//======================================================================

#include "TimeLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Layer.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/time.h>
#include <gis/Box.h>
#include <spine/Exception.h>
#include <spine/Json.h>

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

    json = theJson.get("prefix", nulljson);
    if (!json.isNull())
      prefix = json.asString();

    json = theJson.get("suffix", nulljson);
    if (!json.isNull())
      suffix = json.asString();

    json = theJson.get("timestamp", nulljson);
    if (!json.isNull())
    {
      if (json.isArray())
      {
        for (const auto& str : json)
          timestamp.push_back(str.asString());
      }
      else
        timestamp.push_back(json.asString());
    }

    json = theJson.get("formatter", nulljson);
    if (!json.isNull())
    {
      formatter = json.asString();
      if (formatter != "boost" && formatter != "strftime" && formatter != "fmt")
        throw Spine::Exception(BCP, "Unknown time formatter '" + formatter + "'");
    }

    json = theJson.get("format", nulljson);
    if (!json.isNull())
    {
      if (json.isArray())
      {
        for (const auto& str : json)
          format.push_back(str.asString());
      }
      else
        format.push_back(json.asString());
    }

    auto longitudeJson = theJson.get("longitude", nulljson);
    auto latitudeJson = theJson.get("latitude", nulljson);
    if (!longitudeJson.isNull() && !latitudeJson.isNull())
    {
      double longitude = longitudeJson.asDouble();
      double latitude = latitudeJson.asDouble();
      double xCoord = 0;
      double yCoord = 0;
      LonLatToXYTransformation transformation(projection, theState);
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
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

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

    // Set defaults and validate input.

    if (timestamp.empty())
      timestamp.emplace_back("validtime");

    if (format.empty())
      format.emplace_back("%Y-%m-%d %H:%M");

    if (timestamp.size() != format.size())
      throw Spine::Exception(BCP,
                             "TimeLayer timestamp and format arrays should be of the same size");

    // Create the output
    std::ostringstream msg;
    msg << prefix;

    for (auto i = 0ul; i < timestamp.size(); i++)
    {
      auto name = timestamp[i];
      auto fmt = format[i];

      if (name.empty())
        throw Spine::Exception(BCP, "TimeLayer timestamp setting cannot be an empty string");
      if (fmt.empty())
        throw Spine::Exception(BCP, "TimeLayer format setting cannot be an empty string");

      boost::optional<boost::local_time::local_date_time> loctime;
      boost::optional<boost::posix_time::time_duration> duration;

      if (name == "validtime")
      {
        loctime = boost::local_time::local_date_time(valid_time, tz);
      }
      else if (name == "origintime")
      {
        if (!q)
          throw Spine::Exception(BCP, "Origintime not avaible for TimeLayer");
        loctime = boost::local_time::local_date_time(q->originTime(), tz);
      }
      else if (name == "wallclock" || name == "now")
      {
        loctime = boost::local_time::local_date_time(now, tz);
      }
      else if (name == "starttime")
      {
        auto tmp = boost::local_time::local_date_time(valid_time, tz);
        if (interval_start)
          tmp -= boost::posix_time::minutes(*interval_start);
        loctime = tmp;
      }
      else if (name == "endtime")
      {
        auto tmp = boost::local_time::local_date_time(valid_time, tz);
        if (interval_end)
          tmp += boost::posix_time::minutes(*interval_end);
        loctime = tmp;
      }
      else if (name == "leadtime")
      {
        if (!q)
          throw Spine::Exception(BCP, "Origintime not avaible for TimeLayer");
        duration = valid_time - q->originTime();
      }
      else if (name == "leadhour")
      {
        if (!q)
          throw Spine::Exception(BCP, "Origintime not avaible for TimeLayer");
        boost::posix_time::ptime ot = q->originTime();
        duration =
            valid_time - ot + ot.time_of_day() - boost::posix_time::hours(ot.time_of_day().hours());
      }
      else if (name == "time_offset")
      {
        duration = boost::posix_time::minutes(time_offset ? *time_offset : 0);
      }
      else if (name == "interval_start")
      {
        duration = boost::posix_time::minutes(interval_start ? *interval_start : 0);
      }
      else if (name == "interval_end")
      {
        duration = boost::posix_time::minutes(interval_end ? *interval_end : 0);
      }
      else
      {
        loctime = boost::local_time::local_date_time(valid_time, tz) +
                  Fmi::TimeParser::parse_duration(name);
      }

      // durations are always formatted with boost
      if (!loctime || formatter == "boost")
      {
        try
        {
          auto* facet = new boost::posix_time::time_facet(fmt.c_str());
          msg.imbue(std::locale(msg.getloc(), facet));

          if (loctime)
            msg << loctime->local_time();
          else
            msg << *duration;
        }
        catch (...)
        {
          throw Spine::Exception::Trace(BCP, "Failed to format time with Boost")
              .addParameter("format", "'" + fmt + "'");
        }
      }
      else if (formatter == "strftime")
      {
        auto timeinfo = to_tm(loctime->local_time());
        char buffer[100];
        if (strftime(static_cast<char*>(buffer), 100, fmt.c_str(), &timeinfo) == 0)
        {
          throw Spine::Exception(BCP, "Failed to format a non-empty time string with strftime")
              .addParameter("format", "'" + fmt + "'");
        }
        msg << static_cast<char*>(buffer);
      }
      else if (formatter == "fmt")
      {
        try
        {
          auto timeinfo = to_tm(loctime->local_time());
          msg << fmt::format(fmt, timeinfo);
        }
        catch (...)
        {
          throw Spine::Exception::Trace(BCP, "Failed to format time with fmt")
              .addParameter("format", "'" + fmt + "'");
        }
      }

      else
        throw Spine::Exception(BCP, "Unknown TimeLayer time formatter '" + formatter + "'");
    }
    msg << suffix;

    text_cdt["cdata"] = msg.str();
    theLayersCdt.PushBack(text_cdt);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
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
    Dali::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));
    Dali::hash_combine(hash, Dali::hash_value(timezone));
    Dali::hash_combine(hash, Dali::hash_value(prefix));
    Dali::hash_combine(hash, Dali::hash_value(suffix));
    Dali::hash_combine(hash, Dali::hash_value(timestamp));
    Dali::hash_combine(hash, Dali::hash_value(format));
    Dali::hash_combine(hash, Dali::hash_value(formatter));
    Dali::hash_combine(hash, Dali::hash_value(x));
    Dali::hash_combine(hash, Dali::hash_value(y));
    Dali::hash_combine(hash, Dali::hash_value(longitude));
    Dali::hash_combine(hash, Dali::hash_value(latitude));

    // TODO(mheiskan): The timestamp to be drawn may depend on the wall clock - must include it
    // in the hash. A better solution would be to format the timestamp and
    // then calculate the hash.

    for (const auto& name : timestamp)
    {
      if (name == "wallclock" || name == "now")
        Dali::hash_combine(hash, Dali::hash_value(now));
    }

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
