#include "WMSGetCapabilities.h"
#include "WMSException.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <ctpp2/CTPP2Logger.hpp>  // logging level defines
#include <fmt/printf.h>
#include <macgyver/Exception.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <algorithm>
#include <memory>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{

#if 0
std::string resolveGetMapURI(const Spine::HTTP::Request& theRequest)
{
  try
  {
    // See what we need to present in the GetMap query
    auto host_header = theRequest.getHeader("Host");
    if (!host_header)
    {
      // This should never happen, host header is mandatory in HTTP 1.1
      return "http://smartmet.fmi.fi/wms";
    }

    // http/https scheme selection based on 'X-Forwarded-Proto' header
    auto host_protocol = theRequest.getProtocol();
    std::string protocol((host_protocol ? *host_protocol : "http") + "://");

    const auto& host = *host_header;
    if (host == "data.fmi.fi" || host == "wms.fmi.fi")  // These should be configurable
    {
      // These hosts need apikey in order to work
      auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);

      if (!apikey)

        return protocol + host + "/wms";  // No apikey? We'll make do without.
      return protocol + host + "/fmi-apikey/" + *apikey + "/wms";
    }

    // No apikey needed
    return protocol + host + "/wms";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Resolving GetMap URI failed!");
  }
}
#endif

/*
 * Patch protocol for HTTP/GET or HTTP/POST online resources
 */

void patch_protocol(CTPP::CDT& dcptype,
                    const std::string& protocol,
                    const std::string& method,
                    const std::string& newprotocol)
{
  if (dcptype.Exists(protocol) && dcptype.At(protocol).Exists(method) &&
      dcptype.At(protocol).At(method).Exists("online_resource"))
  {
    auto& resource = dcptype.At(protocol).At(method).At("online_resource");
    std::string url = resource.GetString();

    boost::algorithm::replace_first(url, "http://", newprotocol);
    boost::algorithm::replace_first(url, "https://", newprotocol);

    resource = url;
  }
}

/*
 * Patch online_resource settings for a specific request type
 */

void patch_protocols(CTPP::CDT& dcptypes, const std::string& newprotocol)
{
  for (std::size_t i = 0; i < dcptypes.Size(); i++)
  {
    patch_protocol(dcptypes[i], "http", "get", newprotocol);
    patch_protocol(dcptypes[i], "http", "post", newprotocol);
  }
}

/*
 * Parse an optional time option
 */

std::optional<Fmi::DateTime> parse_optional_time(const std::optional<std::string>& time_string)
{
  if (!time_string)
    return {};

  // Special case for legacy reasons
  if (*time_string == "now" || *time_string == "NOW")
    return Fmi::SecondClock::universal_time();

  return Fmi::TimeParser::parse(*time_string);
}

}  // namespace

std::string WMSGetCapabilities::response(const Fmi::SharedFormatter& theFormatter,
                                         const Spine::HTTP::Request& theRequest,
                                         const Engine::Querydata::Engine& /* theQEngine */,
                                         const WMSConfig& theConfig)
{
  try
  {
    std::optional<std::string> apikey;
    try
    {
      // Deduce apikey for layer filtering
      apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Failed to get apikey from the query");
    }

    CTPP::CDT hash;
    try
    {
      hash = theConfig.getCapabilitiesResponseVariables();
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Failed to extract capabilities from configuration file");
    }

    CTPP::CDT configuredLayers;
    // Layer hierarchy type: default value is flat, can be
    // overwritten in configuration file, and finally
    // can be overwritten in URL
    WMSLayerHierarchy::HierarchyType hierarchyType = theConfig.getLayerHierarchyType();
    auto layout = theRequest.getParameter("layout");
    if (layout)
    {
      if (*layout == "flat")
        hierarchyType = WMSLayerHierarchy::HierarchyType::flat;
      else if (*layout == "recursive")
        hierarchyType = WMSLayerHierarchy::HierarchyType::recursive;
      else if (*layout == "recursivetimes")
        hierarchyType = WMSLayerHierarchy::HierarchyType::recursivetimes;
      else
      {
        throw Fmi::Exception::Trace(BCP, ("Error! Invalid layout defined in request: " + *layout));
      }
    }

    try
    {
      auto wms_namespace = theRequest.getParameter("namespace");

      auto starttime = parse_optional_time(theRequest.getParameter("starttime"));
      auto endtime = parse_optional_time(theRequest.getParameter("endtime"));
      auto reference_time = parse_optional_time(theRequest.getParameter("dim_reference_time"));

      auto multipleIntervals = theConfig.multipleIntervals();
      auto enableintervals = theRequest.getParameter("enableintervals");
      // Hidden flag can be overridden in request
      auto hidden_flag = theRequest.getParameter("show_hidden");
      bool show_hidden = (hidden_flag && *hidden_flag == "1");

      // If request option given and it is 1 or 0 use it
      if (enableintervals)
      {
        if (*enableintervals == "1")
          multipleIntervals = true;
        else if (*enableintervals == "0")
          multipleIntervals = false;
      }

      auto language = theConfig.getDaliConfig().defaultLanguage();  // Should this be WMS specific?
      auto query_lang = theRequest.getParameter("LANGUAGE");
      if (query_lang)
        language = *query_lang;

      configuredLayers = theConfig.getCapabilities(apikey,
                                                   language,
                                                   starttime,
                                                   endtime,
                                                   reference_time,
                                                   wms_namespace,
                                                   hierarchyType,
                                                   show_hidden,
                                                   multipleIntervals);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Failed to get capabilities from configured layers");
    }

    if (!hash.Exists("capability"))
      throw Fmi::Exception(BCP, "WMS generated has does not contain a capability section!");

    try
    {
      hash.At("capability")["layer"] = configuredLayers;
      if (hierarchyType != WMSLayerHierarchy::HierarchyType::flat)
        hash["capability"]["newfeature"] =
            (hierarchyType == WMSLayerHierarchy::HierarchyType::recursive ? "1" : "2");
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Setting configured layers to output hash failed");
    }

    // http/https scheme selection based on 'X-Forwarded-Proto' header
    auto host_protocol = theRequest.getProtocol();
    std::string protocol(host_protocol ? *host_protocol : "http");
    if (!protocol.empty())
      protocol += "://";

    if (!protocol.empty())
    {
      try
      {
        patch_protocols(hash.At("capability").At("request").At("getcapabilities").At("dcptype"),
                        protocol);
        patch_protocols(hash.At("capability").At("request").At("getmap").At("dcptype"), protocol);
        if (hash.At("capability").At("request").Exists("getfeatureinfo"))
          patch_protocols(hash.At("capability").At("request").At("getfeatureinfo").At("dcptype"),
                          protocol);
      }
      catch (...)
      {
        throw Fmi::Exception::Trace(BCP, "Patching protocol failed")
            .addParameter("protocol", protocol);
      }
    }

    const bool print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);
    if (print_hash)
      fmt::print("Generated GetCapabilities CDT:\n{}\n", hash.RecursiveDump());

    std::string ret;
    std::string log;
    try
    {
      bool isdebug = Spine::optional_bool(theRequest.getParameter("debug"), false);
      if (isdebug)
        theFormatter->process(hash, ret, log, CTPP2_LOG_DEBUG);
      else
        theFormatter->process(hash, ret, log);
    }
    catch (const std::exception& e)
    {
      throw Fmi::Exception::Trace(BCP, "CTPP formatter failed")
          .addParameter("what", e.what())
          .addParameter("log enabled by debug=1", log);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "CTPP formatter failed")
          .addParameter("log enabled by debug=1", log);
    }

    // Finish up with host name replacements

    auto host_header = theRequest.getHeader("Host");
    try
    {
      if (!host_header)
      {
        // This should never happen, host header is mandatory in HTTP 1.1
        host_header = "smartmet.fmi.fi/wms";
      }

      auto hostString = *host_header;
      if (hostString.length() >= 4)
      {
        std::string hostEnd = hostString.substr(hostString.length() - 4);
        if (hostEnd == "/wms")
          hostString = hostString.substr(0, hostString.length() - 4);
      }

      boost::replace_all(ret, "__hostname__", protocol + hostString);

      // If omit-fmi-apikey header is provided, do not show apikey in GetCapabilities
      std::string apirepl;
      if (apikey)
      {
        auto omit = theRequest.getHeader("omit-fmi-apikey");
        if (!omit || omit == std::string("0") || omit == std::string("false"))
          apirepl = "/fmi-apikey/" + *apikey;
      }
      boost::replace_all(ret, "__apikey__", apirepl);

      return ret;
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Fixing host name failed")
          .addParameter("host", *host_header);
    }
  }

  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "WMSGetCapabilities::response() failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
