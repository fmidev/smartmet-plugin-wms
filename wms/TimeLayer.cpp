//======================================================================

#include "TimeLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/grid/Engine.h>
#include <fmt/chrono.h>
#include <gis/Box.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <macgyver/DateTime.h>
#include <macgyver/Exception.h>
#include <macgyver/LocalDateTime.h>
#include <spine/Json.h>
#include <array>
#include <ogr_spatialref.h>

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

void TimeLayer::init(Json::Value& theJson,
                     const State& theState,
                     const Config& theConfig,
                     const Properties& theProperties)
{
  try
  {
    // Initialize this variable only once!
    now = Fmi::SecondClock::universal_time();

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Time-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(timezone, theJson, "timezone");
    JsonTools::remove_string(prefix, theJson, "prefix");
    JsonTools::remove_string(suffix, theJson, "suffix");

    auto json = JsonTools::remove(theJson, "timestamp");
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

    JsonTools::remove_string(formatter, theJson, "formatter");
    if (formatter != "boost" && formatter != "strftime" && formatter != "fmt")
      throw Fmi::Exception(BCP, "Unknown time formatter '" + formatter + "'");

    json = JsonTools::remove(theJson, "format");
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

    JsonTools::remove_int(x, theJson, "x");
    JsonTools::remove_int(y, theJson, "y");

    // latlon settings override x/y
    JsonTools::remove_double(longitude, theJson, "longitude");
    JsonTools::remove_double(latitude, theJson, "latitude");

    if (longitude && latitude)
    {
      double xCoord = 0;
      double yCoord = 0;
      LonLatToXYTransformation transformation(projection);
      if (!transformation.transform(*longitude, *latitude, xCoord, yCoord))
        throw Fmi::Exception(BCP, "Invalid lonlat for timestamp");
      x = xCoord;
      y = yCoord;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    if (source && *source == "grid")
      generate_gridEngine(theGlobals, theLayersCdt, theState);
    else
      generate_qEngine(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

void TimeLayer::generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    const auto* gridEngine = theState.getGridEngine();
    if (!gridEngine || !gridEngine->isEnabled())
      throw Fmi::Exception(BCP, "The grid-engine is disabled!");

    if (*projection.crs == "data")
      return;

    // if (!validLayer(theState))
    //  return;

    // Time execution

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "TimeLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Establish the data
    // auto q = getModel(theState);

    // Establish the valid time

    if (!projection.projectionParameter)
      throw Fmi::Exception(BCP, "Parameter not set for time-layer");

    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);

    auto valid_time = getValidTime();
    ;

    std::string wkt = *projection.crs;
    // std::cout << wkt << "\n";

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      if (strstr(wkt.c_str(), "+proj") != wkt.c_str())
        wkt = projection.getCRS().WKT();

      // std::cout << wkt << "\n";

      // Adding the bounding box information into the query.

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      auto bbox = fmt::format("{},{},{},{}", bl.X(), bl.Y(), tr.X(), tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox", bbox);

      const auto& box = projection.getBox();
      bbox = fmt::format("{},{},{},{}", box.xmin(), box.ymin(), box.xmax(), box.ymax());
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means
      // that we we do not know the actual projection yet and we have to wait that the grid-engine
      // delivers us the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    std::string paramStr = *projection.projectionParameter;

    if (originalGridQuery->mProducerNameList.empty())
    {
      gridEngine->getProducerNameList(producerName, originalGridQuery->mProducerNameList);
      if (originalGridQuery->mProducerNameList.empty())
        originalGridQuery->mProducerNameList.push_back(producerName);
    }

    attributeList.addAttribute("param", paramStr);

    std::string forecastTime = Fmi::to_iso_string(getValidTime());
    attributeList.addAttribute("startTime", forecastTime);
    attributeList.addAttribute("endTime", forecastTime);
    attributeList.addAttribute("timelist", forecastTime);
    attributeList.addAttribute("timezone", "UTC");

    if (origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery, attributeList);

    // Fullfilling information into the query object.

    for (auto& param : originalGridQuery->mQueryParameterList)
    {
      param.mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      param.mType = QueryServer::QueryParameter::Type::Vector;
      param.mFlags = QueryServer::QueryParameter::Flags::NoReturnValues;
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs", wkt);

    if (projection.size && *projection.size > 0)
    {
      originalGridQuery->mAttributeList.addAttribute("grid.size", Fmi::to_string(*projection.size));
    }
    else
    {
      if (projection.xsize)
        originalGridQuery->mAttributeList.addAttribute("grid.width",
                                                       Fmi::to_string(*projection.xsize));

      if (projection.ysize)
        originalGridQuery->mAttributeList.addAttribute("grid.height",
                                                       Fmi::to_string(*projection.ysize));
    }

    if (wkt == "data" && projection.x1 && projection.y1 && projection.x2 && projection.y2)
    {
      auto bbox = fmt::format(
          "{},{},{},{}", *projection.x1, *projection.y1, *projection.x2, *projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox", bbox);
    }

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    // query.print(std::cout,0,0);

    std::shared_ptr<QueryServer::ParameterValues> p;

    for (auto param = query->mQueryParameterList.begin();
         param != query->mQueryParameterList.end() && p == nullptr;
         ++param)
    {
      if (param->mParam == paramStr && !param->mValueList.empty())
      {
        auto val = param->mValueList.begin();
        p = (*val);
      }
    }

    std::optional<Fmi::DateTime> originTime;
    if (p && p->mAnalysisTime.length() >= 15)
      originTime = Fmi::TimeParser::parse_iso(p->mAnalysisTime);

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
      // projection.update(q);
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
      throw Fmi::Exception(BCP, "TimeLayer timestamp and format arrays should be of the same size");

    // Create the output
    std::ostringstream msg;
    msg << prefix;

    for (auto i = 0UL; i < timestamp.size(); i++)
    {
      auto name = timestamp[i];
      auto fmt = format[i];

      if (name.empty())
        throw Fmi::Exception(BCP, "TimeLayer timestamp setting cannot be an empty string");

      if (fmt.empty())
        throw Fmi::Exception(BCP, "TimeLayer format setting cannot be an empty string");

      std::optional<Fmi::LocalDateTime> loctime;
      std::optional<Fmi::TimeDuration> duration;

      if (name == "validtime")
      {
        loctime = Fmi::LocalDateTime(valid_time, tz);
      }
      else if (name == "origintime")
      {
        if (!originTime)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");

        loctime = Fmi::LocalDateTime(*originTime, tz);
      }
      else if (name == "wallclock" || name == "now")
      {
        loctime = Fmi::LocalDateTime(now, tz);
      }
      else if (name == "starttime")
      {
        auto tmp = Fmi::LocalDateTime(valid_time, tz);
        if (interval_start)
          tmp -= Fmi::Minutes(*interval_start);
        loctime = tmp;
      }
      else if (name == "endtime")
      {
        auto tmp = Fmi::LocalDateTime(valid_time, tz);
        if (interval_end)
          tmp += Fmi::Minutes(*interval_end);
        loctime = tmp;
      }
      else if (name == "leadtime")
      {
        if (!originTime)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");

        duration = valid_time - *originTime;
      }
      else if (name == "leadhour")
      {
        if (!originTime)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");

        Fmi::DateTime ot = *originTime;
        duration = valid_time - ot + ot.time_of_day() - Fmi::Hours(ot.time_of_day().hours());
      }
      else if (name == "time_offset")
      {
        duration = Fmi::Minutes(time_offset ? *time_offset : 0);
      }
      else if (name == "interval_start")
      {
        duration = Fmi::Minutes(interval_start ? *interval_start : 0);
      }
      else if (name == "interval_end")
      {
        duration = Fmi::Minutes(interval_end ? *interval_end : 0);
      }
      else
      {
        loctime = Fmi::LocalDateTime(valid_time, tz) + Fmi::TimeParser::parse_duration(name);
      }

      // durations are always formatted with boost
      if (!loctime || formatter == "boost")
      {
        try
        {
          if (loctime)
            msg << Fmi::format_time(fmt, loctime->local_time());
          else
            msg << Fmi::format_time(fmt, *duration);
        }
        catch (...)
        {
          throw Fmi::Exception::Trace(BCP, "Failed to format time with Boost")
              .addParameter("format", "'" + fmt + "'");
        }
      }
      else if (formatter == "strftime")
      {
        auto timeinfo = to_tm(loctime->local_time());
        std::array<char, 100> buffer;
        if (strftime(buffer.data(), 100, fmt.c_str(), &timeinfo) == 0)
        {
          throw Fmi::Exception(BCP, "Failed to format a non-empty time string with strftime")
              .addParameter("format", "'" + fmt + "'");
        }
        msg << buffer.data();
      }
      else if (formatter == "fmt")
      {
        try
        {
          auto timeinfo = to_tm(loctime->local_time());
          msg << fmt::format(fmt::runtime(fmt), timeinfo);
        }
        catch (...)
        {
          throw Fmi::Exception::Trace(BCP, "Failed to format time with fmt")
              .addParameter("format", "'" + fmt + "'");
        }
      }

      else
        throw Fmi::Exception(BCP, "Unknown TimeLayer time formatter '" + formatter + "'");
    }
    msg << suffix;

    text_cdt["cdata"] = msg.str();
    theLayersCdt.PushBack(text_cdt);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void TimeLayer::generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Time execution

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "TimeLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

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
      throw Fmi::Exception(BCP, "TimeLayer timestamp and format arrays should be of the same size");

    // Create the output
    std::ostringstream msg;
    msg << prefix;

    for (auto i = 0UL; i < timestamp.size(); i++)
    {
      auto name = timestamp[i];
      auto fmt = format[i];

      if (name.empty())
        throw Fmi::Exception(BCP, "TimeLayer timestamp setting cannot be an empty string");
      if (fmt.empty())
        throw Fmi::Exception(BCP, "TimeLayer format setting cannot be an empty string");

      std::optional<Fmi::LocalDateTime> loctime;
      std::optional<Fmi::TimeDuration> duration;

      if (name == "validtime")
      {
        loctime = Fmi::LocalDateTime(valid_time, tz);
      }
      else if (name == "origintime")
      {
        if (!q)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");
        loctime = Fmi::LocalDateTime(q->originTime(), tz);
      }
      else if (name == "wallclock" || name == "now")
      {
        loctime = Fmi::LocalDateTime(now, tz);
      }
      else if (name == "starttime")
      {
        auto tmp = Fmi::LocalDateTime(valid_time, tz);
        if (interval_start)
          tmp -= Fmi::Minutes(*interval_start);
        loctime = tmp;
      }
      else if (name == "endtime")
      {
        auto tmp = Fmi::LocalDateTime(valid_time, tz);
        if (interval_end)
          tmp += Fmi::Minutes(*interval_end);
        loctime = tmp;
      }
      else if (name == "leadtime")
      {
        if (!q)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");
        duration = valid_time - q->originTime();
      }
      else if (name == "leadhour")
      {
        if (!q)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");
        Fmi::DateTime ot = q->originTime();
        duration = valid_time - ot + ot.time_of_day() - Fmi::Hours(ot.time_of_day().hours());
      }
      else if (name == "time_offset")
      {
        duration = Fmi::Minutes(time_offset ? *time_offset : 0);
      }
      else if (name == "interval_start")
      {
        duration = Fmi::Minutes(interval_start ? *interval_start : 0);
      }
      else if (name == "interval_end")
      {
        duration = Fmi::Minutes(interval_end ? *interval_end : 0);
      }
      else
      {
        loctime = Fmi::LocalDateTime(valid_time, tz) + Fmi::TimeParser::parse_duration(name);
      }

      // durations are always formatted with boost
      if (!loctime || formatter == "boost")
      {
        try
        {
          if (loctime)
            msg << Fmi::format_time(fmt, loctime->local_time());
          else
            msg << Fmi::format_time(fmt, *duration);
        }
        catch (...)
        {
          throw Fmi::Exception::Trace(BCP, "Failed to format time with Boost")
              .addParameter("format", "'" + fmt + "'");
        }
      }
      else if (formatter == "strftime")
      {
        auto timeinfo = to_tm(loctime->local_time());
        std::array<char, 101> buffer;
        if (strftime(buffer.data(), 100, fmt.c_str(), &timeinfo) == 0)
        {
          throw Fmi::Exception(BCP, "Failed to format a non-empty time string with strftime")
              .addParameter("format", "'" + fmt + "'");
        }
        msg << buffer.data();
      }
      else if (formatter == "fmt")
      {
        try
        {
          auto timeinfo = to_tm(loctime->local_time());
          msg << fmt::format(fmt::runtime(fmt), timeinfo);
        }
        catch (...)
        {
          throw Fmi::Exception::Trace(BCP, "Failed to format time with fmt")
              .addParameter("format", "'" + fmt + "'");
        }
      }

      else
        throw Fmi::Exception(BCP, "Unknown TimeLayer time formatter '" + formatter + "'");
    }
    msg << suffix;

    text_cdt["cdata"] = msg.str();
    theLayersCdt.PushBack(text_cdt);
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

std::size_t TimeLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    if (!(source && *source == "grid"))
      Fmi::hash_combine(hash, Engine::Querydata::hash_value(getModel(theState)));

    Fmi::hash_combine(hash, Fmi::hash_value(timezone));
    Fmi::hash_combine(hash, Fmi::hash_value(prefix));
    Fmi::hash_combine(hash, Fmi::hash_value(suffix));
    Fmi::hash_combine(hash, Fmi::hash_value(timestamp));
    Fmi::hash_combine(hash, Fmi::hash_value(format));
    Fmi::hash_combine(hash, Fmi::hash_value(formatter));
    Fmi::hash_combine(hash, Fmi::hash_value(x));
    Fmi::hash_combine(hash, Fmi::hash_value(y));
    Fmi::hash_combine(hash, Fmi::hash_value(longitude));
    Fmi::hash_combine(hash, Fmi::hash_value(latitude));

    // TODO(mheiskan): The timestamp to be drawn may depend on the wall clock - must include it
    // in the hash. A better solution would be to format the timestamp and
    // then calculate the hash.

    for (const auto& name : timestamp)
    {
      if (name == "wallclock" || name == "now")
        Fmi::hash_combine(hash, Fmi::hash_value(now));
    }

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
