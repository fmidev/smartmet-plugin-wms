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
  LegendGraphicParameter(const std::string& dn) : data_name(dn) {}
  LegendGraphicParameter(const std::string& dn,
                         const std::string& gn,
                         const std::string& u,
                         bool ht)
      : data_name(dn), given_name(gn), unit(u), hide_title(ht)
  {
  }
  LegendGraphicParameter(const LegendGraphicParameter& lgp)
      : data_name(lgp.data_name),
        given_name(lgp.given_name),
        unit(lgp.unit),
        hide_title(lgp.hide_title),
        translations(lgp.translations)
  {
  }

  std::string data_name;
  std::string given_name;
  std::string unit;
  bool hide_title{false};
  std::map<std::string, std::string> translations;
};

struct LegendGraphicSymbol
{
  LegendGraphicSymbol() {}
  LegendGraphicSymbol(const std::string& sn) : symbol_name(sn) {}
  LegendGraphicSymbol(const LegendGraphicSymbol& lgs)
      : symbol_name(lgs.symbol_name), translations(lgs.translations)
  {
  }

  std::string symbol_name;
  std::map<std::string, std::string> translations;
};

struct LegendGraphicLayout
{
  boost::optional<int> param_name_xoffset;  // parameter name
  boost::optional<int> param_name_yoffset;
  boost::optional<int> param_unit_xoffset;  // unit
  boost::optional<int> param_unit_yoffset;
  boost::optional<int> legend_xoffset;  // legend
  boost::optional<int> legend_yoffset;
  boost::optional<unsigned int> symbol_group_x_padding;  // space between symbols in symbol-group
  boost::optional<unsigned int> symbol_group_y_padding;
  boost::optional<unsigned int> legend_width;  // width of one legend
  unsigned int
      output_document_width;  // size of non-automatically generated documents, default 500*500
  unsigned int output_document_height;
};

class WMSLegendGraphicSettings
{
 public:
  WMSLegendGraphicSettings(const libconfig::Config& config);
  WMSLegendGraphicSettings() : expires(-1) {}

  LegendGraphicLayout layout;
  std::map<std::string, LegendGraphicParameter> parameters;
  std::map<std::string, LegendGraphicSymbol> symbols;
  std::set<std::string> symbolsToIgnore;
  int expires;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
