#include "WMSDebug.h"
#include <spine/Convenience.h>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    std::string wmsRequest(
        (theHTTPRequest.getParameter("request") ? *(theHTTPRequest.getParameter("request")) : ""));

    ost << "SERVICE = "
        << (theHTTPRequest.getParameter("service") ? *(theHTTPRequest.getParameter("service")) : "")
        << std::endl;
    ost << "VERSION = "
        << (theHTTPRequest.getParameter("version") ? *(theHTTPRequest.getParameter("version")) : "")
        << std::endl;
    ost << "REQUEST = " << wmsRequest << std::endl;
    if (wmsRequest == "GetCapabilities")
    {
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << std::endl;
      ost << "UPDATESEQUENCE = "
          << (theHTTPRequest.getParameter("updatesequence")
                  ? *(theHTTPRequest.getParameter("updatesequence"))
                  : "")
          << std::endl;
    }
    else if (wmsRequest == "GetMap")
    {
      ost << "LAYERS = "
          << (theHTTPRequest.getParameter("layers") ? *(theHTTPRequest.getParameter("layers")) : "")
          << std::endl;
      ost << "STYLES = "
          << (theHTTPRequest.getParameter("styles") ? *(theHTTPRequest.getParameter("styles")) : "")
          << std::endl;
      ost << "CRS = "
          << (theHTTPRequest.getParameter("crs") ? *(theHTTPRequest.getParameter("crs")) : "")
          << std::endl;
      ost << "BBOX = "
          << (theHTTPRequest.getParameter("bbox") ? *(theHTTPRequest.getParameter("bbox")) : "")
          << std::endl;
      ost << "WIDTH = "
          << (theHTTPRequest.getParameter("width") ? *(theHTTPRequest.getParameter("width")) : "")
          << std::endl;
      ost << "HEIGHT = "
          << (theHTTPRequest.getParameter("height") ? *(theHTTPRequest.getParameter("height")) : "")
          << std::endl;
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << std::endl;
      ost << "TRANSPARENT = "
          << (theHTTPRequest.getParameter("transparent")
                  ? *(theHTTPRequest.getParameter("transparent"))
                  : "")
          << std::endl;
      ost << "BGCOLOR = "
          << (theHTTPRequest.getParameter("bgcolor") ? *(theHTTPRequest.getParameter("bgcolor"))
                                                     : "")
          << std::endl;
      ost << "EXCEPTIONS = "
          << (theHTTPRequest.getParameter("exceptions")
                  ? *(theHTTPRequest.getParameter("exceptions"))
                  : "")
          << std::endl;
      ost << "TIME = "
          << (theHTTPRequest.getParameter("time") ? *(theHTTPRequest.getParameter("time")) : "")
          << std::endl;
      ost << "ELEVATION = "
          << (theHTTPRequest.getParameter("elevation") ? *(theHTTPRequest.getParameter("elevation"))
                                                       : "")
          << std::endl;
    }
    else if (wmsRequest == "GetLegendGraphic")
    {
      ost << "LAYER = "
          << (theHTTPRequest.getParameter("layer") ? *(theHTTPRequest.getParameter("layer")) : "")
          << std::endl;
      ost << "STYLE = "
          << (theHTTPRequest.getParameter("style") ? *(theHTTPRequest.getParameter("style")) : "")
          << std::endl;
      ost << "REMOTE_OWS_TYPE = "
          << (theHTTPRequest.getParameter("remote_ows_type")
                  ? *(theHTTPRequest.getParameter("remote_ows_type"))
                  : "")
          << std::endl;
      ost << "REMOTE_OWS_URL = "
          << (theHTTPRequest.getParameter("remote_ows_url")
                  ? *(theHTTPRequest.getParameter("remote_ows_url"))
                  : "")
          << std::endl;
      ost << "FEATURETYPE = "
          << (theHTTPRequest.getParameter("featuretype")
                  ? *(theHTTPRequest.getParameter("featuretype"))
                  : "")
          << std::endl;
      ost << "COVERAGE = "
          << (theHTTPRequest.getParameter("coverage") ? *(theHTTPRequest.getParameter("coverage"))
                                                      : "")
          << std::endl;
      ost << "RULE = "
          << (theHTTPRequest.getParameter("rule") ? *(theHTTPRequest.getParameter("rule")) : "")
          << std::endl;
      ost << "SCALE = "
          << (theHTTPRequest.getParameter("scale") ? *(theHTTPRequest.getParameter("scale")) : "")
          << std::endl;
      ost << "SLD = "
          << (theHTTPRequest.getParameter("sld") ? *(theHTTPRequest.getParameter("sld")) : "")
          << std::endl;
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << std::endl;
      ost << "WIDTH = "
          << (theHTTPRequest.getParameter("width") ? *(theHTTPRequest.getParameter("width")) : "")
          << std::endl;
      ost << "HEIGHT = "
          << (theHTTPRequest.getParameter("height") ? *(theHTTPRequest.getParameter("height")) : "")
          << std::endl;
      ost << "EXCEPIONS = "
          << (theHTTPRequest.getParameter("exceptions")
                  ? *(theHTTPRequest.getParameter("exceptions"))
                  : "")
          << std::endl;
      ost << "SLD_VERSION = "
          << (theHTTPRequest.getParameter("sld_version")
                  ? *(theHTTPRequest.getParameter("sld_version"))
                  : "")
          << std::endl;
    }

    return ost;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Printing WMS request failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
