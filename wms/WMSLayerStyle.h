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
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct LegendURL
{
  LegendURL() : format("image/png") {}
  std::string format;
  std::string online_resource;
  std::string width;
  std::string height;
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

// Layers with optional alternative styles
static std::set<std::string> supportedStyleLayers = {
    "isoline", "isoband", "symbol", "arrow", "number", "isolabel"};

std::ostream& operator<<(std::ostream& ost, const WMSLayerStyle& layerStyle);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
