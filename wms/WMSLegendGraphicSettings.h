// ======================================================================
/*!
 * \brief Settimgs for legend graphic
 */
// ======================================================================

#pragma once

#include <boost/optional.hpp>
#include <libconfig.h++>
#include <map>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct LegendGraphicParameter
{
  LegendGraphicParameter() {}
  LegendGraphicParameter(const std::string& n, const std::string& u) : name(n), unit(u) {}
  LegendGraphicParameter(const LegendGraphicParameter& lgp) : name(lgp.name), unit(lgp.unit) {}

  std::string name;
  std::string unit;
};

struct LegendGraphicLayout
{
  boost::optional<int> param_name_xoffset;  // parameter name
  boost::optional<int> param_name_yoffset;
  boost::optional<int> param_unit_xoffset;  // unit
  boost::optional<int> param_unit_yoffset;
  boost::optional<int> legend_xoffset;  // legend
  boost::optional<int> legend_yoffset;
  boost::optional<int> symbol_group_x_padding;  // space between symbols in symbol-group
  boost::optional<int> symbol_group_y_padding;
  boost::optional<int> legend_width;  // width of one legend
  int output_document_width;  // size of non-automatically generated documents, default 500*500
  int output_document_height;
};

class WMSLegendGraphicSettings
{
 public:
  WMSLegendGraphicSettings(const libconfig::Config& config);
  WMSLegendGraphicSettings() {}

  LegendGraphicLayout layout;
  std::map<std::string, LegendGraphicParameter> parameters;
  std::set<std::string> symbolsToIgnore;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
