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

    const std::map<std::string, std::string>& responseVariables =
        theConfig.getCapabilitiesResponseVariables();

    hash["version_string"] = boost::algorithm::join(theConfig.supportedWMSVersions(), ", ");

    // Deduce apikey for layer filtering
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest);

    std::string configuredLayers = theConfig.getCapabilities(apikey);

    hash["wms_layers"] = configuredLayers;

    hash["getmap_uri"] = resolveGetMapURI(theRequest);

    if (theConfig.inspireExtensionSupported())
    {
      hash["inspire_extended_capabilities"] = 1;
      hash["inspire_default_language"] = responseVariables.at("default_language");
      hash["inspire_supported_language"] = responseVariables.at("supported_language");
      hash["inspire_response_language"] = responseVariables.at("response_language");
    }

    hash["title"] = responseVariables.at("title");
    hash["abstract"] = responseVariables.at("abstract");
    hash["online_resource"] = responseVariables.at("online_resource");
    hash["contact_person"] = responseVariables.at("contact_person");
    hash["organization"] = responseVariables.at("organization");
    hash["contact_position"] = responseVariables.at("contact_position");
    hash["contact_address_type"] = responseVariables.at("contact_address_type");
    hash["contact_address"] = responseVariables.at("contact_address");
    hash["contact_address_city"] = responseVariables.at("contact_address_city");
    hash["contact_address_province"] = responseVariables.at("contact_address_province");
    hash["contact_address_post_code"] = responseVariables.at("contact_address_post_code");
    hash["contact_address_country"] = responseVariables.at("contact_address_country");
    hash["telephone_number"] = responseVariables.at("telephone_number");
    hash["facimile_number"] = responseVariables.at("facimile_number");
    hash["fees"] = responseVariables.at("fees");
    hash["access_constraints"] = responseVariables.at("access_constraints");
    hash["layers_title"] = responseVariables.at("layers_title");

    std::vector<std::string> keywords;
    boost::algorithm::split(keywords,
                            responseVariables.at("keywords"),
                            boost::is_any_of(","),
                            boost::token_compress_on);
    for (unsigned int i = 0; i < keywords.size(); i++)
      hash["keywords"][i] = keywords[i];

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
