#include "WMSLayerStyle.h"
#include <ostream>

#include <macgyver/StringConversion.h>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
std::ostream& operator<<(std::ostream& ost, const WMSLayerStyle& layerStyle)
{
  ost << "name: " << layerStyle.name << std::endl;
  ost << "title: " << layerStyle.title << std::endl;
  ost << "abstract: " << layerStyle.abstract << std::endl;
  ost << "LegendURL: " << std::endl;
  ost << " width: " << layerStyle.legend_url.width << std::endl;
  ost << " height: " << layerStyle.legend_url.height << std::endl;
  ost << " format: " << layerStyle.legend_url.format << std::endl;
  ost << " online resource: " << layerStyle.legend_url.online_resource << std::endl;

  return ost;
}

std::string WMSLayerStyle::toXML()
{
  std::string ret;
  /*
  if (name.size() == 0)
    throw Spine::Exception(BCP, "WMS layer style must have a name!", NULL);
  if (title.size() == 0)
    throw Spine::Exception(BCP, "WMS layer style must have a title!", NULL);
  */
  try
  {
    ret += "<Style>\n";
    /*
ret += "<Name>";
ret += name;
ret += "</Name>\n";
ret += "<Title>";
ret += title;
ret += "</Title>\n";
ret += "<Abstract>";
ret += abstract;
ret += "</Abstract>\n";
    */
    ret += "<LegendURL width=\"";
    ret += Fmi::to_string(legend_url.width);
    ret += "\" height=\"";
    ret += Fmi::to_string(legend_url.height);
    ret += "\">\n";
    ret += "<Format>";
    ret += legend_url.format;
    ret += "</Format>\n";
    ret += "<OnlineResource ";
    ret += legend_url.online_resource;
    ret += "/>\n";
    ret += "</LegendURL>\n";
    ret += "</Style>\n";
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Failed to convert WMSLayerStyle to XML!", NULL);
  }

  return ret;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
