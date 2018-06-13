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

#include <ctpp2/CDT.hpp>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct LegendURL
{
  LegendURL() : width(200), height(200), format("image/png") {}
  int width;
  int height;
  std::string format;
  std::string online_resource;
};

struct WMSLayerStyle
{
  WMSLayerStyle() : name("default"), title("Default style") {}

  std::string name;
  std::string title;
  std::string abstract;
  LegendURL legend_url;
  CTPP::CDT getCapabilities() const;
};

std::ostream& operator<<(std::ostream& ost, const WMSLayerStyle& layerStyle);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
