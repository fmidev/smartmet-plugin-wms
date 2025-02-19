// ======================================================================
/*!
 * \brief Settimgs for legend graphic
 */
// ======================================================================

#pragma once

#include <libconfig.h++>
#include <map>
#include <optional>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct LegendGraphicParameter
{
  explicit LegendGraphicParameter(std::string dn) : data_name(std::move(dn)) {}
  LegendGraphicParameter() = default;
  LegendGraphicParameter(std::string dn, std::string gn, std::string u, bool ht)
      : data_name(std::move(dn)), given_name(std::move(gn)), unit(std::move(u)), hide_title(ht)
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
  explicit LegendGraphicSymbol(std::string sn) : symbol_name(std::move(sn)) {}
  LegendGraphicSymbol() = default;

  std::string symbol_name;

  std::map<std::string, std::string> translations;
};

struct LegendGraphicLayout
{
  std::optional<int> param_name_xoffset;  // parameter name
  std::optional<int> param_name_yoffset;
  std::optional<int> param_unit_xoffset;  // unit
  std::optional<int> param_unit_yoffset;
  std::optional<int> legend_xoffset;  // legend
  std::optional<int> legend_yoffset;
  std::optional<unsigned int> symbol_group_x_padding;  // space between symbols in symbol-group
  std::optional<unsigned int> symbol_group_y_padding;
  std::optional<unsigned int> symbol_text_xoffset;                // x offset of symbol text
  std::optional<unsigned int> symbol_text_yoffset;                // y offset of symbol text
  std::optional<unsigned int> legend_width;                       // width of one legend
  std::map<std::string, unsigned int> legend_width_per_language;  // legend width per langauge
  unsigned int
      output_document_width;  // size of non-automatically generated documents, default 500*500
  unsigned int output_document_height;
};

class WMSLegendGraphicSettings
{
 public:
  explicit WMSLegendGraphicSettings(const libconfig::Config& config);
  explicit WMSLegendGraphicSettings(bool initDefaults = false);
  void merge(const WMSLegendGraphicSettings& settings);

  LegendGraphicLayout layout;
  std::map<std::string, LegendGraphicParameter> parameters;
  std::map<std::string, LegendGraphicSymbol> symbols;
  std::set<std::string> symbolsToIgnore;
  std::set<std::string> languages;
  int expires;
};

std::ostream& operator<<(std::ostream& ost, const LegendGraphicParameter& lgp);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicSymbol& lgs);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicLayout& layout);
std::ostream& operator<<(std::ostream& ost, const WMSLegendGraphicSettings& lgs);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
