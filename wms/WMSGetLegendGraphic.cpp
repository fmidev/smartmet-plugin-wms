#include "WMSGetLegendGraphic.h"
#include "CaseInsensitiveComparator.h"
#include "Mime.h"
#include "WMSException.h"
#include <boost/algorithm/string/erase.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Convenience.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
void check_getlegendgraphic_request_options(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    // check that all mandatory options has been defined

    if (!theHTTPRequest.getParameter("VERSION"))
    {
      throw Fmi::Exception(BCP, "Schema version not defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    if (!theHTTPRequest.getParameter("SLD_VERSION"))
    {
      throw Fmi::Exception(BCP, "SLD specification version not defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    if (!theHTTPRequest.getParameter("LAYER"))
    {
      throw Fmi::Exception(BCP, "Layer must be defined in GetLegendGraphic request")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
    }

    if (!theHTTPRequest.getParameter("FORMAT"))
    {
      throw Fmi::Exception(BCP, "FORMAT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking GetLegendGraphic options failed!");
  }
}

void validate_options(const get_legend_graphic_request_options& options,
                      const WMSConfig& itsConfig,
                      const Engine::Querydata::Engine& /* querydata */)
{
  try
  {
    // check the requested version number
    if (!itsConfig.isValidVersion(options.version))
    {
      throw Fmi::Exception(BCP, "The requested version is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED)
          .addParameter("Requested version", options.version)
          .addParameter("Supported versions",
                        boost::algorithm::join(itsConfig.supportedWMSVersions(), "','"));
    }

    // check that layer is valid (as defined in GetCapabilities response)
    if (!itsConfig.isValidLayer(options.layer, true))
    {
      throw Fmi::Exception(BCP, "The requested layer is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
          .addParameter("Requested layer", options.layer);
    }

    // check that style is valid (as defined in GetCapabilities response)
    if (!itsConfig.isValidStyle(options.layer, options.style))
    {
      throw Fmi::Exception(BCP,
                           "The style is not supported by the requested layer!" + options.layer +
                               ", " + options.style)
          .addParameter(WMS_EXCEPTION_CODE, WMS_STYLE_NOT_DEFINED)
          .addParameter("Requested style", options.style)
          .addParameter("Requested layer", options.layer);
    }

    // check format
    if (!itsConfig.isValidMapFormat(options.format))
    {
      throw Fmi::Exception(BCP, "The requested map format is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_FORMAT)
          .addParameter("Requested map format", options.format)
          .addParameter("Supported map formats",
                        boost::algorithm::join(itsConfig.supportedMapFormats(), "','"));
    }

    if (options.sld_version != "1.1.0")
    {
      throw Fmi::Exception(BCP, "Invalid SLD-specification version!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED)
          .addParameter("Requested SLD-specification version", options.format)
          .addParameter("Supported SLD-specification version",
                        boost::algorithm::join(itsConfig.supportedMapFormats(), "','"));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetLegendGraphic request did not validate!");
  }
}

#if 0
// Not currently needed

// if iso-string contains fractions of seconds remove them
// so that Fmi::TimeParser knows how to parse
boost::posix_time::ptime parse_time(const std::string& time)
{
  try
  {
    std::string t(time);
    // remove second fractions
    if (t.find("Z") != std::string::npos && t.find(".") != std::string::npos &&
        t.find("Z") > t.find("."))  // iso string with fractions of second
    {
      size_t len(t.find("Z") - t.find("."));
      t.erase(t.find("."), len);
    }

    return Fmi::TimeParser::parse(t);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Parsing GetLegendGraphic time failed!");
  }
}

void parse_interval_with_resolution(const std::string time_str,
                                    std::vector<boost::posix_time::ptime>& timesteps)
{
  try
  {
    // interval defined by its lower bound, upper bound and resolution: min/max/res
    std::vector<std::string> parts;
    boost::algorithm::split(parts, time_str, boost::algorithm::is_any_of("/"));

    if (parts.size() != 3)
    {
      Fmi::Exception exception(BCP, "Invalid time interval!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      exception.addParameter("Time interval", time_str);
      throw exception;
    }

    CaseInsensitiveComparator cicomp;
    boost::posix_time::ptime startTime =
        (cicomp(parts[0], "current") ? boost::posix_time::second_clock::universal_time()
                                     : parse_time(parts[0]));
    boost::posix_time::ptime endTime =
        (cicomp(parts[1], "current") ? boost::posix_time::second_clock::universal_time()
                                     : parse_time(parts[1]));

    if (endTime < startTime)
    {
      Fmi::Exception exception(BCP, "Invalid time interval!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      exception.addParameter("Time interval", time_str);
      throw exception;
    }

    std::string resolution(parts[2]);

    unsigned int total_minutes = resolution_in_minutes(resolution);

    if (total_minutes > 0)
    {
      while (startTime <= endTime)
      {
        timesteps.push_back(startTime);
        startTime += boost::posix_time::minutes(total_minutes);
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Parsing time interval failed!");
  }
}
#endif

}  // anonymous namespace

WMSGetLegendGraphic::WMSGetLegendGraphic(const WMSConfig& theConfig) : itsConfig(theConfig) {}

void WMSGetLegendGraphic::parseHTTPRequest(const Engine::Querydata::Engine& theQEngine,
                                           Spine::HTTP::Request& theRequest)
{
  try
  {
    // check all mandatory options
    check_getlegendgraphic_request_options(theRequest);

    // parse the request
    std::string layerName = *(theRequest.getParameter("LAYER"));
    std::string styleName;
    if (theRequest.getParameter("STYLE"))
      styleName = *(theRequest.getParameter("STYLE"));
    if (styleName.empty())
      styleName = "default";

    itsParameters.layer = layerName;
    itsParameters.style = styleName;
    itsParameters.width = 500;
    itsParameters.height = 500;
    itsParameters.format = *(theRequest.getParameter("FORMAT"));
    itsParameters.version = *(theRequest.getParameter("VERSION"));
    itsParameters.sld_version = *(theRequest.getParameter("SLD_VERSION"));

    // validate the given options
    validate_options(itsParameters, itsConfig, theQEngine);

    // Convert format to image type
    theRequest.removeParameter("format");
    theRequest.addParameter("type", Dali::demimetype(itsParameters.format));

    // Convert WMS width & height to projection xsize & ysize
    std::string xsize = Fmi::to_string(itsParameters.width);
    std::string ysize = Fmi::to_string(itsParameters.height);
    if (theRequest.getParameter("WIDTH"))
      xsize = *(theRequest.getParameter("WIDTH"));
    if (theRequest.getParameter("HEIGHT"))
      ysize = *(theRequest.getParameter("HEIGHT"));
    theRequest.removeParameter("width");
    theRequest.removeParameter("height");
    theRequest.addParameter("projection.xsize", xsize);
    theRequest.addParameter("projection.ysize", ysize);

    theRequest.removeParameter("format");
    theRequest.addParameter("type", Dali::demimetype(itsParameters.format));
    theRequest.addParameter("projection.crs", "EPSG:4326");
    theRequest.addParameter("projection.bbox", "0,0,1,1");

    // resolve current time (most recent) for the layer
    CaseInsensitiveComparator cicomp;
    std::string time_str = Spine::optional_string(theRequest.getParameter("TIME"), "current");
    if (cicomp(time_str, "current"))
    {
      if (!itsConfig.isTemporal(layerName))
      {
        theRequest.removeParameter("time");
      }
      else
      {
        if (!itsConfig.currentValue(layerName))
        {
          Fmi::Exception exception(BCP, "Invalid TIME option value for the current layer!");
          exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
          exception.addParameter("Time value", time_str);
          exception.addParameter("Layer", layerName);
          throw exception;
        }

        const boost::optional<boost::posix_time::ptime> reference_time;
        boost::posix_time::ptime mostCurrentTime(
            itsConfig.mostCurrentTime(layerName, reference_time));
        if (mostCurrentTime.is_not_a_date_time())
          theRequest.removeParameter("time");
        else
          theRequest.addParameter("time", Fmi::to_iso_string(mostCurrentTime));
      }
    }
    else
    {
      theRequest.addParameter("time", time_str);
    }

    theRequest.addParameter("customer", itsConfig.layerCustomer(layerName));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Parsing GetLegendGraphic request failed!");
  }
}

Json::Value WMSGetLegendGraphic::json() const
{
  try
  {
    return itsConfig.json(itsParameters.layer);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "GetLegendGraphic JSON generation failed!");
  }
}
}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
