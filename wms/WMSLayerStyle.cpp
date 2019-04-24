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
  ost << " format: " << layerStyle.legend_url.format << std::endl;
  ost << " online resource: " << layerStyle.legend_url.online_resource << std::endl;

  return ost;
}

CTPP::CDT WMSLayerStyle::getCapabilities() const
{
  try
  {
    CTPP::CDT style(CTPP::CDT::HASH_VAL);

    // Style name, title and abstract

    if (name.empty())
      throw Spine::Exception::Trace(BCP, "WMS layer style must have a name!");
    if (title.empty())
      throw Spine::Exception::Trace(BCP, "WMS layer style must have a title!");

    style["name"] = name;
    style["title"] = title;

    if (!abstract.empty())
      style["abstract"] = abstract;

    // Style legend URL

    CTPP::CDT style_legend_url_list(CTPP::CDT::ARRAY_VAL);

    CTPP::CDT style_legend_url(CTPP::CDT::HASH_VAL);
    style_legend_url["format"] = legend_url.format;
    style_legend_url["online_resource"] = legend_url.online_resource;
    style_legend_url_list.PushBack(style_legend_url);

    style["legend_url"] = style_legend_url_list;

#if 0
    // Style sheet not implemented

    CTPP::CDT style_style_sheet_url(CTPP::CDT::HASH_VAL);
    style_style_sheet_url["format"] = "css";
    style_style_sheet_url["online_resource"] = "http://www.www.wwws/style.css";
    style["style_sheet_url"] = style_style_sheet_url;

    // Style URL not implemented

    CTPP::CDT style_style_url(CTPP::CDT::HASH_VAL);
    style_style_url["format"] = "css";
    style_style_url["online_resource"] = "http://www.www.wwws/style.css";
    style["style_url"] = style_style_url;

#endif

    return style;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to convert WMSLayerStyle to template output format");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
