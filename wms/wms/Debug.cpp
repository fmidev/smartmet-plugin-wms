#include "Debug.h"
#include <macgyver/Exception.h>
#include <spine/Convenience.h>
#include <utility>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
// Print the named options as "NAME = value" lines, missing options as empty values
void print_options(std::ostream& ost,
                   const Spine::HTTP::Request& theHTTPRequest,
                   const std::vector<std::pair<const char*, const char*>>& theOptions)
{
  for (const auto& option : theOptions)
  {
    auto value = theHTTPRequest.getParameter(option.second);
    ost << option.first << " = " << (value ? *value : "") << '\n';
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest)
{
  try
  {
    std::string wmsRequest(
        (theHTTPRequest.getParameter("request") ? *(theHTTPRequest.getParameter("request")) : ""));

    print_options(ost, theHTTPRequest, {{"SERVICE", "service"}, {"VERSION", "version"}});
    ost << "REQUEST = " << wmsRequest << '\n';
    if (wmsRequest == "GetCapabilities")
    {
      print_options(
          ost, theHTTPRequest, {{"FORMAT", "format"}, {"UPDATESEQUENCE", "updatesequence"}});
    }
    else if (wmsRequest == "GetMap")
    {
      print_options(ost,
                    theHTTPRequest,
                    {{"LAYERS", "layers"},
                     {"STYLES", "styles"},
                     {"CRS", "crs"},
                     {"BBOX", "bbox"},
                     {"WIDTH", "width"},
                     {"HEIGHT", "height"},
                     {"FORMAT", "format"},
                     {"TRANSPARENT", "transparent"},
                     {"BGCOLOR", "bgcolor"},
                     {"EXCEPTIONS", "exceptions"},
                     {"TIME", "time"},
                     {"ELEVATION", "elevation"}});
    }
    else if (wmsRequest == "GetLegendGraphic")
    {
      print_options(ost,
                    theHTTPRequest,
                    {{"LAYER", "layer"},
                     {"STYLE", "style"},
                     {"REMOTE_OWS_TYPE", "remote_ows_type"},
                     {"REMOTE_OWS_URL", "remote_ows_url"},
                     {"FEATURETYPE", "featuretype"},
                     {"COVERAGE", "coverage"},
                     {"RULE", "rule"},
                     {"SCALE", "scale"},
                     {"SLD", "sld"},
                     {"FORMAT", "format"},
                     {"WIDTH", "width"},
                     {"HEIGHT", "height"},
                     {"EXCEPIONS", "exceptions"},
                     {"SLD_VERSION", "sld_version"}});
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
