#include "WMSDebug.h"
#include <macgyver/Exception.h>
#include <spine/Convenience.h>

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
        << '\n';
    ost << "VERSION = "
        << (theHTTPRequest.getParameter("version") ? *(theHTTPRequest.getParameter("version")) : "")
        << '\n';
    ost << "REQUEST = " << wmsRequest << '\n';
    if (wmsRequest == "GetCapabilities")
    {
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << '\n';
      ost << "UPDATESEQUENCE = "
          << (theHTTPRequest.getParameter("updatesequence")
                  ? *(theHTTPRequest.getParameter("updatesequence"))
                  : "")
          << '\n';
    }
    else if (wmsRequest == "GetMap")
    {
      ost << "LAYERS = "
          << (theHTTPRequest.getParameter("layers") ? *(theHTTPRequest.getParameter("layers")) : "")
          << '\n';
      ost << "STYLES = "
          << (theHTTPRequest.getParameter("styles") ? *(theHTTPRequest.getParameter("styles")) : "")
          << '\n';
      ost << "CRS = "
          << (theHTTPRequest.getParameter("crs") ? *(theHTTPRequest.getParameter("crs")) : "")
          << '\n';
      ost << "BBOX = "
          << (theHTTPRequest.getParameter("bbox") ? *(theHTTPRequest.getParameter("bbox")) : "")
          << '\n';
      ost << "WIDTH = "
          << (theHTTPRequest.getParameter("width") ? *(theHTTPRequest.getParameter("width")) : "")
          << '\n';
      ost << "HEIGHT = "
          << (theHTTPRequest.getParameter("height") ? *(theHTTPRequest.getParameter("height")) : "")
          << '\n';
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << '\n';
      ost << "TRANSPARENT = "
          << (theHTTPRequest.getParameter("transparent")
                  ? *(theHTTPRequest.getParameter("transparent"))
                  : "")
          << '\n';
      ost << "BGCOLOR = "
          << (theHTTPRequest.getParameter("bgcolor") ? *(theHTTPRequest.getParameter("bgcolor"))
                                                     : "")
          << '\n';
      ost << "EXCEPTIONS = "
          << (theHTTPRequest.getParameter("exceptions")
                  ? *(theHTTPRequest.getParameter("exceptions"))
                  : "")
          << '\n';
      ost << "TIME = "
          << (theHTTPRequest.getParameter("time") ? *(theHTTPRequest.getParameter("time")) : "")
          << '\n';
      ost << "ELEVATION = "
          << (theHTTPRequest.getParameter("elevation") ? *(theHTTPRequest.getParameter("elevation"))
                                                       : "")
          << '\n';
    }
    else if (wmsRequest == "GetLegendGraphic")
    {
      ost << "LAYER = "
          << (theHTTPRequest.getParameter("layer") ? *(theHTTPRequest.getParameter("layer")) : "")
          << '\n';
      ost << "STYLE = "
          << (theHTTPRequest.getParameter("style") ? *(theHTTPRequest.getParameter("style")) : "")
          << '\n';
      ost << "REMOTE_OWS_TYPE = "
          << (theHTTPRequest.getParameter("remote_ows_type")
                  ? *(theHTTPRequest.getParameter("remote_ows_type"))
                  : "")
          << '\n';
      ost << "REMOTE_OWS_URL = "
          << (theHTTPRequest.getParameter("remote_ows_url")
                  ? *(theHTTPRequest.getParameter("remote_ows_url"))
                  : "")
          << '\n';
      ost << "FEATURETYPE = "
          << (theHTTPRequest.getParameter("featuretype")
                  ? *(theHTTPRequest.getParameter("featuretype"))
                  : "")
          << '\n';
      ost << "COVERAGE = "
          << (theHTTPRequest.getParameter("coverage") ? *(theHTTPRequest.getParameter("coverage"))
                                                      : "")
          << '\n';
      ost << "RULE = "
          << (theHTTPRequest.getParameter("rule") ? *(theHTTPRequest.getParameter("rule")) : "")
          << '\n';
      ost << "SCALE = "
          << (theHTTPRequest.getParameter("scale") ? *(theHTTPRequest.getParameter("scale")) : "")
          << '\n';
      ost << "SLD = "
          << (theHTTPRequest.getParameter("sld") ? *(theHTTPRequest.getParameter("sld")) : "")
          << '\n';
      ost << "FORMAT = "
          << (theHTTPRequest.getParameter("format") ? *(theHTTPRequest.getParameter("format")) : "")
          << '\n';
      ost << "WIDTH = "
          << (theHTTPRequest.getParameter("width") ? *(theHTTPRequest.getParameter("width")) : "")
          << '\n';
      ost << "HEIGHT = "
          << (theHTTPRequest.getParameter("height") ? *(theHTTPRequest.getParameter("height")) : "")
          << '\n';
      ost << "EXCEPIONS = "
          << (theHTTPRequest.getParameter("exceptions")
                  ? *(theHTTPRequest.getParameter("exceptions"))
                  : "")
          << '\n';
      ost << "SLD_VERSION = "
          << (theHTTPRequest.getParameter("sld_version")
                  ? *(theHTTPRequest.getParameter("sld_version"))
                  : "")
          << '\n';
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
