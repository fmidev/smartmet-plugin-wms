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
#include <fmt/chrono.h>
#include <gis/Box.h>
#include <grid-content/queryServer/definition/QueryConfigurator.h>
#include <grid-files/common/GeneralFunctions.h>
#include <engines/grid/Engine.h>
#include <macgyver/Exception.h>
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
      throw Fmi::Exception(BCP, "Time-layer JSON is not a JSON object");

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
        throw Fmi::Exception(BCP, "Unknown time formatter '" + formatter + "'");
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
/*
    json = theJson.get("parameter", nulljson);
    if (!json.isNull())
      parameter = json.asString();

    json = theJson.get("geometryId", nulljson);
    if (!json.isNull())
      geometryId = json.asInt();

    json = theJson.get("levelId", nulljson);
    if (!json.isNull())
      levelId = json.asInt();

    json = theJson.get("level", nulljson);
    if (!json.isNull())
      level = json.asDouble();

    json = theJson.get("forecastType", nulljson);
    if (!json.isNull())
      forecastType = json.asInt();

    json = theJson.get("forecastNumber", nulljson);
    if (!json.isNull())
      forecastNumber = json.asInt();

    auto request = theState.getRequest();

    boost::optional<std::string> v = request.getParameter("geometryId");
    if (v)
      geometryId = toInt32(*v);

    v = request.getParameter("levelId");
    if (v)
      levelId = toInt32(*v);

    v = request.getParameter("level");
    if (v)
      level = toInt32(*v);

    v = request.getParameter("forecastType");
    if (v)
      forecastType = toInt32(*v);

    v = request.getParameter("forecastNumber");
    if (v)
      forecastNumber = toInt32(*v);
*/
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
      generate_gridEngine(theGlobals,theLayersCdt,theState);
    else
      generate_qEngine(theGlobals,theLayersCdt,theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}





void TimeLayer::generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (*projection.crs == "data")
      return;

    //if (!validLayer(theState))
    //  return;

    // Time execution

    std::string report = "TimeLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // Establish the data
    //auto q = getModel(theState);

    // Establish the valid time

    if (!projection.projectionParameter)
      throw Fmi::Exception(BCP, "Parameter not set for time-layer");

    auto gridEngine = theState.getGridEngine();
    std::shared_ptr<QueryServer::Query> originalGridQuery(new QueryServer::Query());
    QueryServer::QueryConfigurator queryConfigurator;
    T::AttributeList attributeList;

    std::string producerName = gridEngine->getProducerName(*producer);

    auto valid_time = getValidTime();;

    std::string wkt = *projection.crs;
    //std::cout << wkt << "\n";

    if (wkt != "data")
    {
      // Getting WKT and the bounding box of the requested projection.

      auto crs = projection.getCRS();
      char *out = nullptr;
      crs->exportToWkt(&out);
      wkt = out;
      OGRFree(out);

      //std::cout << wkt << "\n";

      // Adding the bounding box information into the query.

      char bbox[100];

      auto bl = projection.bottomLeftLatLon();
      auto tr = projection.topRightLatLon();
      sprintf(bbox,"%f,%f,%f,%f",bl.X(),bl.Y(),tr.X(),tr.Y());
      originalGridQuery->mAttributeList.addAttribute("grid.llbox",bbox);

      const auto& box = projection.getBox();
      sprintf(bbox,"%f,%f,%f,%f",box.xmin(),box.ymin(),box.xmax(),box.ymax());
      originalGridQuery->mAttributeList.addAttribute("grid.bbox",bbox);
    }
    else
    {
      // The requested projection is the same as the projection of the requested data. This means that we
      // we do not know the actual projection yet and we have to wait that the grid-engine delivers us
      // the requested data and the projection information of the current data.
    }

    // Adding parameter information into the query.

    std::string paramStr = *projection.projectionParameter;

    if (originalGridQuery->mProducerNameList.size() == 0)
    {
      gridEngine->getProducerNameList(producerName,originalGridQuery->mProducerNameList);
      if (originalGridQuery->mProducerNameList.size() == 0)
        originalGridQuery->mProducerNameList.push_back(producerName);
    }

    attributeList.addAttribute("param",paramStr);

    std::string forecastTime = Fmi::to_iso_string(*time);
    attributeList.addAttribute("startTime",forecastTime);
    attributeList.addAttribute("endTime",forecastTime);
    attributeList.addAttribute("timelist",forecastTime);
    attributeList.addAttribute("timezone","UTC");

    if (origintime)
      attributeList.addAttribute("analysisTime", Fmi::to_iso_string(*origintime));

    // Tranforming information from the attribute list into the query object.
    queryConfigurator.configure(*originalGridQuery,attributeList);

    // Fullfilling information into the query object.

    for (auto it = originalGridQuery->mQueryParameterList.begin(); it != originalGridQuery->mQueryParameterList.end(); ++it)
    {
      it->mLocationType = QueryServer::QueryParameter::LocationType::Geometry;
      it->mType = QueryServer::QueryParameter::Type::Vector;
      it->mFlags = QueryServer::QueryParameter::Flags::NoReturnValues;
    }

    originalGridQuery->mSearchType = QueryServer::Query::SearchType::TimeSteps;
    originalGridQuery->mAttributeList.addAttribute("grid.crs",wkt);

    if (projection.size  &&  *projection.size > 0)
    {
      originalGridQuery->mAttributeList.addAttribute("grid.size",std::to_string(*projection.size));
    }
    else
    {
      if (projection.xsize)
        originalGridQuery->mAttributeList.addAttribute("grid.width",std::to_string(*projection.xsize));

      if (projection.ysize)
        originalGridQuery->mAttributeList.addAttribute("grid.height",std::to_string(*projection.ysize));
    }

    if (wkt == "data"  &&  projection.x1 && projection.y1 && projection.x2 && projection.y2)
    {
      char bbox[100];
      sprintf(bbox,"%f,%f,%f,%f",*projection.x1,*projection.y1,*projection.x2,*projection.y2);
      originalGridQuery->mAttributeList.addAttribute("grid.bbox",bbox);
    }

    // The Query object before the query execution.
    // query.print(std::cout,0,0);

    // Executing the query.
    std::shared_ptr<QueryServer::Query> query = gridEngine->executeQuery(originalGridQuery);

    // The Query object after the query execution.
    // query.print(std::cout,0,0);


    std::shared_ptr<QueryServer::ParameterValues> p;

    for (auto param = query->mQueryParameterList.begin(); param != query->mQueryParameterList.end()  &&  p == nullptr; ++param)
    {
      if (param->mParam == paramStr  &&  param->mValueList.size() > 0)
      {
        auto val = param->mValueList.begin();
        p = (*val);
      }
    }

    boost::optional<boost::posix_time::ptime> originTime;
    if (p  && p->mAnalysisTime.length() >= 15)
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
      //projection.update(q);
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
      throw Fmi::Exception(BCP,"TimeLayer timestamp and format arrays should be of the same size");

    // Create the output
    std::ostringstream msg;
    msg << prefix;

    for (auto i = 0ul; i < timestamp.size(); i++)
    {
      auto name = timestamp[i];
      auto fmt = format[i];

      if (name.empty())
        throw Fmi::Exception(BCP, "TimeLayer timestamp setting cannot be an empty string");

      if (fmt.empty())
        throw Fmi::Exception(BCP, "TimeLayer format setting cannot be an empty string");

      boost::optional<boost::local_time::local_date_time> loctime;
      boost::optional<boost::posix_time::time_duration> duration;

      if (name == "validtime")
      {
        loctime = boost::local_time::local_date_time(valid_time, tz);
      }
      else if (name == "origintime")
      {
        if (!originTime)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");

        loctime = boost::local_time::local_date_time(*originTime, tz);
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
        if (!originTime)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");

        duration = valid_time - *originTime;
      }
      else if (name == "leadhour")
      {
        if (!originTime)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");

        boost::posix_time::ptime ot = *originTime;
        duration = valid_time - ot + ot.time_of_day() - boost::posix_time::hours(ot.time_of_day().hours());
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
          throw Fmi::Exception::Trace(BCP, "Failed to format time with Boost")
              .addParameter("format", "'" + fmt + "'");
        }
      }
      else if (formatter == "strftime")
      {
        auto timeinfo = to_tm(loctime->local_time());
        char buffer[100];
        if (strftime(static_cast<char*>(buffer), 100, fmt.c_str(), &timeinfo) == 0)
        {
          throw Fmi::Exception(BCP, "Failed to format a non-empty time string with strftime")
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
      throw Fmi::Exception(BCP,
                             "TimeLayer timestamp and format arrays should be of the same size");

    // Create the output
    std::ostringstream msg;
    msg << prefix;

    for (auto i = 0ul; i < timestamp.size(); i++)
    {
      auto name = timestamp[i];
      auto fmt = format[i];

      if (name.empty())
        throw Fmi::Exception(BCP, "TimeLayer timestamp setting cannot be an empty string");
      if (fmt.empty())
        throw Fmi::Exception(BCP, "TimeLayer format setting cannot be an empty string");

      boost::optional<boost::local_time::local_date_time> loctime;
      boost::optional<boost::posix_time::time_duration> duration;

      if (name == "validtime")
      {
        loctime = boost::local_time::local_date_time(valid_time, tz);
      }
      else if (name == "origintime")
      {
        if (!q)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");
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
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");
        duration = valid_time - q->originTime();
      }
      else if (name == "leadhour")
      {
        if (!q)
          throw Fmi::Exception(BCP, "Origintime not avaible for TimeLayer");
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
          throw Fmi::Exception::Trace(BCP, "Failed to format time with Boost")
              .addParameter("format", "'" + fmt + "'");
        }
      }
      else if (formatter == "strftime")
      {
        auto timeinfo = to_tm(loctime->local_time());
        char buffer[100];
        if (strftime(static_cast<char*>(buffer), 100, fmt.c_str(), &timeinfo) == 0)
        {
          throw Fmi::Exception(BCP, "Failed to format a non-empty time string with strftime")
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
