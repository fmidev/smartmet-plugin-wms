#include "WMSGetLegendGraphic.h"
#include "WMS.h"
#include "WMSException.h"

#include "TemplateFactory.h"

#include <macgyver/StringConversion.h>

#include <boost/algorithm/string/erase.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

#include <spine/Convenience.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
struct CaseInsensitiveComparator : std::binary_function<std::string, std::string, bool>
{
  char asciilower(char ch) const
  {
    char ret = ch;
    if (ch >= 'A' && ch <= 'Z')
      ret = static_cast<char>(ch + ('a' - 'A'));
    return ret;
  }

  bool operator()(const std::string& first, const std::string& second) const
  {
    std::size_t n = std::min(first.size(), second.size());
    for (std::size_t i = 0; i < n; i++)
    {
      char ch1 = asciilower(first[i]);
      char ch2 = asciilower(second[i]);
      if (ch1 != ch2)
        return false;
    }

    return (first.size() == second.size());
  }
};

std::string demimetype(const std::string& theMimeType)
{
  if (theMimeType == "image/png")
    return "png";
  if (theMimeType == "application/pdf")
    return "pdf";
  if (theMimeType == "application/postscript")
    return "ps";
  if (theMimeType == "image/svg+xml")
    return "svg";

  throw Spine::Exception(BCP, "Unknown mime type requested: '" + theMimeType + "'");
}

void check_getlegendgraphic_request_options(const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    // check that all mandatory options has been defined
    std::string missing_options;

    if (!theHTTPRequest.getParameter("VERSION"))
    {
      throw Spine::Exception(BCP, "Schema version not defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    if (!theHTTPRequest.getParameter("SLD_VERSION"))
    {
      throw Spine::Exception(BCP, "SLD specification version not defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    if (!theHTTPRequest.getParameter("LAYER"))
    {
      throw Spine::Exception(BCP, "Layer must be defined in GetLegendGraphic request")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
    }

    if (!theHTTPRequest.getParameter("FORMAT"))
    {
      throw Spine::Exception(BCP, "FORMAT-option has not been defined")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void validate_options(const get_legend_graphic_request_options& options,
                      const WMSConfig& itsConfig,
                      const Engine::Querydata::Engine& querydata)
{
  try
  {
    // check the requested version number
    if (!itsConfig.isValidVersion(options.version))
    {
      throw Spine::Exception(BCP, "The requested version is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED)
          .addParameter("Requested version", options.version)
          .addParameter("Supported versions",
                        boost::algorithm::join(itsConfig.supportedWMSVersions(), "','"));
    }

    // check that layer is valid (as defined in GetCapabilities response)
    if (!itsConfig.isValidLayer(options.layer, true))
    {
      throw Spine::Exception(BCP, "The requested layer is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
          .addParameter("Requested layer", options.layer);
    }

    // check format
    if (!itsConfig.isValidMapFormat(options.format))
    {
      throw Spine::Exception(BCP, "The requested map format is not supported!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_FORMAT)
          .addParameter("Requested map format", options.format)
          .addParameter("Supported map formats",
                        boost::algorithm::join(itsConfig.supportedMapFormats(), "','"));
    }

    if (options.sld_version != "1.1.0")
    {
      throw Spine::Exception(BCP, "Invalid SLD-specification version!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED)
          .addParameter("Requested SLD-specification version", options.format)
          .addParameter("Supported SLD-specification version",
                        boost::algorithm::join(itsConfig.supportedMapFormats(), "','"));
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      Spine::Exception exception(BCP, "Invalid time interval!");
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
      Spine::Exception exception(BCP, "Invalid time interval!");
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}  // anonymous namespace

WMSGetLegendGraphic::WMSGetLegendGraphic(const WMSConfig& theConfig) : itsConfig(theConfig)
{
}

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

    itsParameters.layer = layerName;
    itsParameters.style = styleName;
    itsParameters.width = 1000;  // default values
    itsParameters.height = 1000;
    itsParameters.format = *(theRequest.getParameter("FORMAT"));
    itsParameters.version = *(theRequest.getParameter("VERSION"));
    itsParameters.sld_version = *(theRequest.getParameter("SLD_VERSION"));

    // validate the given options
    validate_options(itsParameters, itsConfig, theQEngine);

    // Convert format to image type
    theRequest.removeParameter("format");
    theRequest.addParameter("type", demimetype(itsParameters.format));

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
    theRequest.addParameter("type", demimetype(itsParameters.format));
    theRequest.addParameter("projection.crs", "EPSG:4326");
    theRequest.addParameter("projection.bbox", "0,0,1,1");

    // resolve current time (most recent) for the layer
    CaseInsensitiveComparator cicomp;
    std::string time_str = Spine::optional_string(theRequest.getParameter("TIME"), "current");
    if (true == cicomp(time_str, "current"))
    {
      if (!itsConfig.isTemporal(layerName))
      {
        theRequest.removeParameter("time");
      }
      else
      {
        if (false == itsConfig.currentValue(layerName))
        {
          Spine::Exception exception(BCP, "Invalid TIME option value for the current layer!");
          exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
          exception.addParameter("Time value", time_str);
          exception.addParameter("Layer", layerName);
          throw exception;
        }

        boost::posix_time::ptime mostCurrentTime(itsConfig.mostCurrentTime(layerName));
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSGetLegendGraphic::jsonText() const
{
  try
  {
    return itsConfig.jsonText(itsParameters.layer);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}
}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet