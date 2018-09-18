#include "WMSGetCapabilities.h"
#include "TemplateFactory.h"
#include "WMSException.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/shared_ptr.hpp>
#include <ctpp2/CTPP2Logger.hpp>  // logging level defines
#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/FmiApiKey.h>
#include <algorithm>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
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

    std::string host = *host_header;
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
    throw Spine::Exception::Trace(BCP, "Resolving GetMap URI failed!");
  }
}

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

std::string WMSGetCapabilities::response(const Dali::SharedFormatter& theFormatter,
                                         const Spine::HTTP::Request& theRequest,
                                         const Engine::Querydata::Engine& /* theQEngine */,
                                         const WMSConfig& theConfig)
{
  try
  {
    boost::optional<std::string> apikey;
    try
    {
      // Deduce apikey for layer filtering
      apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Failed to get apikey from the query");
    }

    CTPP::CDT hash;
    try
    {
      hash = theConfig.getCapabilitiesResponseVariables();
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Failed to extract capabilities from configuration file");
    }

    CTPP::CDT configuredLayers;
    try
    {
      auto wms_namespace = theRequest.getParameter("namespace");
      configuredLayers = theConfig.getCapabilities(apikey, wms_namespace);
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Failed to get capabilities from configured layers");
    }

    if (!hash.Exists("capability"))
      throw Spine::Exception(BCP, "WMS generated has does not contain a capability section!");

    try
    {
      hash.At("capability")["layer"] = configuredLayers;
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Setting configured layers to output hash failed");
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
        throw Spine::Exception::Trace(BCP, "Patching protocol failed")
            .addParameter("protocol", protocol);
      }
    }

    std::stringstream outstream;
    std::stringstream logstream;
    try
    {
      bool isdebug = Spine::optional_bool(theRequest.getParameter("debug"), false);
      if (isdebug)
        theFormatter->process(hash, outstream, logstream, CTPP2_LOG_DEBUG);
      else
        theFormatter->process(hash, outstream, logstream);
    }
    catch (const std::exception& e)
    {
      throw Spine::Exception::Trace(BCP, "CTPP formatter failed")
          .addParameter("what", e.what())
          .addParameter("log enabled by debug=1", logstream.str());
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "CTPP formatter failed")
          .addParameter("log enabled by debug=1", logstream.str());
    }

    // Finish up with host name replacements

    auto host_header = theRequest.getHeader("Host");
    try
    {
      std::string ret = outstream.str();

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
      apikey = (apikey ? ("/fmi-apikey/" + *apikey) : "");
      boost::replace_all(ret, "__apikey__", *apikey);
      return ret;
    }
    catch (...)
    {
      throw Spine::Exception::Trace(BCP, "Fixing host name failed")
          .addParameter("host", *host_header);
    }
  }

  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "WMSGetCapabilities::response() failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
