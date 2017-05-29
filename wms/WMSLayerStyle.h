// ======================================================================
/*!
 * \brief A Meb Maps Service Layer Style data structure
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct LegendURL
{
  int width;
  int height;
  std::string format;
  std::string online_resource;
};
struct WMSLayerStyle
{
  std::string name;
  std::string title;
  std::string abstract;
  LegendURL legend_url;
  std::string toXML();
};

std::ostream& operator<<(std::ostream& ost, const WMSLayerStyle& layerStyle);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
