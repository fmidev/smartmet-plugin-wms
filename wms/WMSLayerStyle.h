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

#include "Text.h"
#include <ctpp2/CDT.hpp>
#include <optional>
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
  WMSLayerStyle() = default;

  std::string name = "default";
  std::optional<Dali::Text> title;
  std::optional<Dali::Text> abstract;

  LegendURL legend_url;
  CTPP::CDT getCapabilities(const std::string& language) const;
};

// Layers with optional alternative styles
static std::set<std::string> supportedStyleLayers = {"isoline",
                                                     "isoband",
                                                     "symbol",
                                                     "arrow",
                                                     "null",
                                                     "number",
                                                     "isolabel",
                                                     "map",
                                                     "postgis",
                                                     "wkt",
                                                     "location",
                                                     "finnish_road_observation",
                                                     "present_weather_observation",
                                                     "cloud_ceiling"};

std::ostream& operator<<(std::ostream& ost, const WMSLayerStyle& layerStyle);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
