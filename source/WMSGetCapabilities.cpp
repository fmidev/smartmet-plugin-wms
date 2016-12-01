#include "WMSGetCapabilities.h"
#include "WMSException.h"
#include "TemplateFactory.h"

#include <algorithm>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

#include <spine/Exception.h>
#include <spine/Convenience.h>

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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSGetCapabilities::resolveGetMapURI(
    const SmartMet::Spine::HTTP::Request& theRequest) const
{
  try
  {
    // See what we need to present in the GetMap query
    auto host_header = theRequest.getHeader("Host");
    if (!host_header)
    {
      // This should never happen, host header is mandatory in HTTP 1.1
      return "http://brainstormgw.fmi.fi/wms";
    }
    else
    {
      std::string host = *host_header;
      if (host == "data.fmi.fi" || host == "wms.fmi.fi")  // These should be configurable
      {
        // These hosts need apikey in order to work
        auto apikey_header = theRequest.getHeader("fmi-apikey");

        if (!apikey_header)
        {
          // No apikey? We'll make do without.
          return "http://" + host + "/wms";
        }
        else
        {
          return "http://" + host + "/fmi-apikey/" + *apikey_header + "/wms";
        }
      }
      else
      {
        // No apikey needed
        return "http://" + host + "/wms";
      }
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSGetCapabilities::response(const SmartMet::Spine::HTTP::Request& theRequest,
                                         const SmartMet::Engine::Querydata::Engine& theQEngine,
                                         const WMSConfig& theConfig) const
{
  try
  {
    CTPP::CDT hash;

    hash["version_string"] = boost::algorithm::join(theConfig.supportedWMSVersions(), ", ");

    // Deduce apikey for layer filtering
    boost::optional<std::string> apikey;
    auto query_apikey = theRequest.getParameter("fmi-apikey");
    if (!query_apikey)
    {
      auto header_apikey = theRequest.getHeader("fmi-apikey");
      apikey = header_apikey;
    }
    else
      apikey = query_apikey;

    std::string configuredLayers = theConfig.getCapabilities(apikey);

    hash["wms_layers"] = configuredLayers;

    hash["getmap_uri"] = resolveGetMapURI(theRequest);

    if (theConfig.inspireExtensionSupported())
    {
      hash["inspire_extended_capabilities"] = 1;
      hash["inspire_default_language"] = theConfig.inspireExtensionDefaultLanguage();
      hash["inspire_supported_language"] = theConfig.inspireExtensionSupportedLanguage();
      hash["inspire_response_language"] = theConfig.inspireExtensionResponseLanguage();
    }

    std::stringstream tmpl_ss;
    std::stringstream logstream;

    itsResponseFormatter->process(hash, tmpl_ss, logstream);

    return tmpl_ss.str();
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
