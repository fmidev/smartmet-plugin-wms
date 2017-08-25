#include "WMSGetCapabilities.h"
#include "TemplateFactory.h"
#include "WMSException.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/shared_ptr.hpp>
#include <algorithm>

#include <ctpp2/CTPP2Logger.hpp>  // logging level defines

#include <spine/Convenience.h>
#include <spine/Exception.h>
#include <spine/FmiApiKey.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
WMSGetCapabilities::WMSGetCapabilities(const std::string& theTemplatePath)
{
  try
  {
    Dali::TemplateFactory templateFactory;
    itsResponseFormatter = templateFactory.get(theTemplatePath);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Initializing WMS repsonse template failed!", NULL);
  }
}

std::string WMSGetCapabilities::resolveGetMapURI(const Spine::HTTP::Request& theRequest) const
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
    else
    {
      // http/https scheme selection based on 'X-Forwarded-Proto' header
      auto host_protocol = theRequest.getProtocol();
      std::string protocol((host_protocol ? *host_protocol : "http") + "://");

      std::string host = *host_header;
      if (host == "data.fmi.fi" || host == "wms.fmi.fi")  // These should be configurable
      {
        // These hosts need apikey in order to work
        auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);

        if (!apikey)
        {
          // No apikey? We'll make do without.
          return protocol + host + "/wms";
        }
        else
        {
          return protocol + host + "/fmi-apikey/" + *apikey + "/wms";
        }
      }
      else
      {
        // No apikey needed
        return protocol + host + "/wms";
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Resolving GetMap URI failed!", NULL);
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
  if (dcptype.Exists(protocol))
  {
    auto& resource = dcptype.At(protocol).At(method).At("online_resource");
    std::string url = resource.GetString();

    if (boost::find_first(url, "http://"))
      boost::algorithm::replace_first(url, "http://", newprotocol);
    else if (boost::find_first(url, "https://"))
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

std::string WMSGetCapabilities::response(const Spine::HTTP::Request& theRequest,
                                         const Engine::Querydata::Engine& theQEngine,
                                         const WMSConfig& theConfig) const
{
  try
  {
    CTPP::CDT hash;
    try
    {
      hash = theConfig.getCapabilitiesResponseVariables();
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "Failed to extract capabilities from configuration file");
    }

    boost::optional<std::string> apikey;
    try
    {
      // Deduce apikey for layer filtering
      apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "Failed to get apikey from the query");
    }

    CTPP::CDT configuredLayers;
    try
    {
      auto wms_namespace = theRequest.getParameter("namespace");
      configuredLayers = theConfig.getCapabilities(apikey, wms_namespace);
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "Failed to get capabilities from configured layers");
    }

    if (!hash.Exists("capability"))
      throw Spine::Exception(BCP, "WMS generated has does not contain a capability section!");
    try
    {
      hash.At("capability")["layer"] = configuredLayers;
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "Setting configured layers to output hash failed");
    }

    // http/https scheme selection based on 'X-Forwarded-Proto' header
    auto host_protocol = theRequest.getProtocol();
    std::string protocol(host_protocol ? *host_protocol : "" /*"http"*/);
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
        throw Spine::Exception(BCP, "Patching protocol failed").addParameter("protocol", protocol);
      }
    }

    std::stringstream outstream;
    std::stringstream logstream;
    try
    {
      itsResponseFormatter->process(hash, outstream, logstream);
      // itsResponseFormatter->process(hash, outstream, logstream, CTPP2_LOG_DEBUG);
    }
    catch (std::exception& e)
    {
      throw Spine::Exception(BCP, "CTPP formatter failed")
          .addParameter("what", e.what())
          .addParameter("log", logstream.str());
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "CTPP formatter failed").addParameter("log", logstream.str());
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

      boost::replace_all(ret, "__hostname__", hostString);
      apikey = (apikey ? ("/fmi-apikey/" + *apikey) : "");
      boost::replace_all(ret, "__apikey__", *apikey);
      return ret;
    }
    catch (...)
    {
      throw Spine::Exception(BCP, "Fixing host name failed").addParameter("host", *host_header);
    }
  }

  catch (...)
  {
    throw Spine::Exception(BCP, "WMSGetCapabilities::response() failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
