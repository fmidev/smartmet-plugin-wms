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
        translations(lgp.translations),
        text_lengths(lgp.text_lengths)
  {
  }

  std::string data_name;
  std::string given_name;
  std::string unit;
  bool hide_title{false};
  std::map<std::string, std::string> translations;
  std::map<std::string, unsigned int> text_lengths;
};

struct LegendGraphicSymbol
{
  LegendGraphicSymbol() {}
  LegendGraphicSymbol(const std::string& sn) : symbol_name(sn) {}
  LegendGraphicSymbol(const LegendGraphicSymbol& lgs)
      : symbol_name(lgs.symbol_name), translations(lgs.translations)
  {
  }

  LegendGraphicSymbol& operator = (const LegendGraphicSymbol&) = default;

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
  boost::optional<unsigned int> symbol_text_xoffset;              // x offset of symbol text
  boost::optional<unsigned int> symbol_text_yoffset;              // y offset of symbol text
  boost::optional<unsigned int> legend_width;                     // width of one legend
  std::map<std::string, unsigned int> legend_width_per_language;  // legend width per langauge
  unsigned int
      output_document_width;  // size of non-automatically generated documents, default 500*500
  unsigned int output_document_height;
};

class WMSLegendGraphicSettings
{
 public:
  WMSLegendGraphicSettings(const libconfig::Config& config);
  WMSLegendGraphicSettings(bool initDefaults = false);
  void merge(const WMSLegendGraphicSettings& settings);

  LegendGraphicLayout layout;
  std::map<std::string, LegendGraphicParameter> parameters;
  std::map<std::string, LegendGraphicSymbol> symbols;
  std::set<std::string> symbolsToIgnore;
  std::set<std::string> languages;
  int expires;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
