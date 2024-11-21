#include "WMSLayerStyle.h"
#include <ostream>

#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
std::ostream& operator<<(std::ostream& ost, const WMSLayerStyle& layerStyle)
{
  ost << "name: " << layerStyle.name << std::endl;
  ost << "title: " << (layerStyle.title ? layerStyle.title->dump() : "-") << std::endl;
  ost << "abstract: " << (layerStyle.abstract ? layerStyle.abstract->dump() : "-") << std::endl;
  ost << "LegendURL: " << std::endl;
  ost << " format: " << layerStyle.legend_url.format << std::endl;
  ost << " online resource: " << layerStyle.legend_url.online_resource << std::endl;

  return ost;
}

CTPP::CDT WMSLayerStyle::getCapabilities(const std::string& language,
                                         const std::string& defaultLanguage) const
{
  try
  {
    CTPP::CDT style(CTPP::CDT::HASH_VAL);

    // Style name, title and abstract

    if (name.empty())
      throw Fmi::Exception::Trace(BCP, "WMS layer style must have a name!");
    style["name"] = name;

    if (!title)
      throw Fmi::Exception::Trace(BCP, "WMS layer style must have a title!");
    style["title"] = title->translate(language, defaultLanguage);

    if (abstract)
      style["abstract"] = abstract->translate(language, defaultLanguage);

    // Style legend URL

    CTPP::CDT style_legend_url_list(CTPP::CDT::ARRAY_VAL);

    CTPP::CDT style_legend_url(CTPP::CDT::HASH_VAL);
    style_legend_url["format"] = legend_url.format;
    style_legend_url["online_resource"] = legend_url.online_resource;
    if (!legend_url.width.empty() && !legend_url.height.empty())
    {
      style_legend_url["width"] = legend_url.width;
      style_legend_url["height"] = legend_url.height;
    }

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
    throw Fmi::Exception::Trace(BCP, "Failed to convert WMSLayerStyle to template output format");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
