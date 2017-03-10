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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
      return "http://brainstormgw.fmi.fi/wms";
    }
    else
    {
      std::string host = *host_header;
      if (host == "data.fmi.fi" || host == "wms.fmi.fi")  // These should be configurable
      {
        // These hosts need apikey in order to work
        auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);

        if (!apikey)
        {
          // No apikey? We'll make do without.
          return "http://" + host + "/wms";
        }
        else
        {
          return "http://" + host + "/fmi-apikey/" + *apikey + "/wms";
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSGetCapabilities::response(const Spine::HTTP::Request& theRequest,
                                         const Engine::Querydata::Engine& theQEngine,
                                         const WMSConfig& theConfig) const
{
  try
  {
    CTPP::CDT hash;

    hash["version_string"] = boost::algorithm::join(theConfig.supportedWMSVersions(), ", ");

    // Deduce apikey for layer filtering
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);

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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
