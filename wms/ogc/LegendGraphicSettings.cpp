#include "LegendGraphicSettings.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <macgyver/Exception.h>
#include <algorithm>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{
void replace_translations(const std::map<std::string, std::string>& sourceTranslations,
                          std::map<std::string, std::string>& targetTranslations)
{
  for (const auto& tr : sourceTranslations)
  {
    if (targetTranslations.find(tr.first) == targetTranslations.end())
      targetTranslations.insert(tr);
    else
      targetTranslations[tr.first] = tr.second;
  }
}

std::vector<std::string> language_codes = {
    "ab", "aa", "af",      "ak",      "sq", "am", "ar", "an", "hy", "as",   "av", "ae", "ay", "az",
    "bm", "ba", "eu",      "be",      "bn", "bh", "bi", "bs", "br", "bg",   "my", "ca", "ch", "ce",
    "ny", "zh", "zh-Hans", "zh-Hant", "cv", "kw", "co", "cr", "hr", "cs",   "da", "dv", "nl", "dz",
    "en", "eo", "et",      "ee",      "fo", "fj", "fi", "fr", "ff", "gl",   "gd", "gv", "ka", "de",
    "el", "kl", "gn",      "gu",      "ht", "ha", "he", "hz", "hi", "ho",   "hu", "is", "io", "ig",
    "id", "in", "ia",      "ie",      "iu", "ik", "ga", "it", "ja", "jv",   "kl", "kn", "kr", "ks",
    "kk", "km", "ki",      "rw",      "rn", "ky", "kv", "kg", "ko", "ku",   "kj", "lo", "la", "lv",
    "li", "ln", "lt",      "lu",      "lg", "lb", "gv", "mk", "mg", "ms",   "ml", "mt", "mi", "mr",
    "mh", "mo", "mn",      "na",      "nv", "ng", "nd", "ne", "no", "nb",   "nn", "ii", "oc", "oj",
    "cu", "or", "om",      "os",      "pi", "ps", "fa", "pl", "pt", "pa",   "qu", "rm", "ro", "ru",
    "se", "sm", "sg",      "sa",      "sr", "sh", "st", "tn", "sn", "Yiii", "sd", "si", "ss", "sk",
    "sl", "so", "nr",      "es",      "su", "sw", "ss", "sv", "tl", "ty",   "tg", "ta", "tt", "te",
    "th", "bo", "ti",      "to",      "ts", "tr", "tk", "tw", "ug", "uk",   "ur", "uz", "ve", "vi",
    "vo", "wa", "cy",      "wo",      "fy", "xh", "yi", "ji", "yo", "za",   "zu"};

// Replace the target value with the source value if the source value is set
template <typename T>
void set_if_given(T& target, const T& source)
{
  if (source)
    target = source;
}

// Set the value from the configuration if the setting exists
template <typename T>
void lookup_if_exists(const libconfig::Config& config, const char* path, T& value)
{
  if (config.exists(path))
    value = config.lookup(path);
}

// Add the translations of the parameters
void add_parameter_translations(const libconfig::Setting& translation,
                                const std::vector<std::string>& param_names,
                                std::map<std::string, LegendGraphicParameter>& parameters)
{
  for (const auto& language_code : language_codes)
  {
    if (translation.exists(language_code))
    {
      std::string tr = translation[language_code.c_str()];
      for (const auto& p : param_names)
      {
        LegendGraphicParameter& lgp = parameters[p];
        lgp.translations.insert(std::make_pair(language_code, tr));
      }
    }
  }
}

// Parse the parameter settings
void parse_parameters(const libconfig::Setting& parameter_settings,
                      std::map<std::string, LegendGraphicParameter>& parameters)
{
  for (int i = 0; i < parameter_settings.getLength(); i++)
  {
    const auto& param = parameter_settings[i];

    std::string data_name = (param.exists("data_name") ? param["data_name"] : "");
    std::string name = (param.exists("name") ? param["name"] : "");
    std::string unit = (param.exists("unit") ? param["unit"] : "");
    bool hide_title = (param.exists("hide_title") ? (bool)param["hide_title"] : false);
    std::vector<std::string> param_names;

    if (!data_name.empty())
    {
      boost::algorithm::split(param_names, data_name, boost::algorithm::is_any_of(","));

      for (const auto& param_name : param_names)
      {
        parameters.insert(
            std::make_pair(param_name, LegendGraphicParameter(data_name, name, unit, hide_title)));
      }
    }

    if (param.exists("translation"))
      add_parameter_translations(param["translation"], param_names, parameters);
  }
}

// Parse the symbol settings
void parse_symbols(const libconfig::Setting& symbol_settings,
                   std::map<std::string, LegendGraphicSymbol>& symbols)
{
  for (int i = 0; i < symbol_settings.getLength(); i++)
  {
    const auto& symbol = symbol_settings[i];

    std::string symbol_name = (symbol.exists("name") ? symbol["name"] : "");

    if (symbol_name.empty())
      continue;

    symbols.insert(std::make_pair(symbol_name, LegendGraphicSymbol(symbol_name)));

    if (symbol.exists("translation"))
    {
      const auto& translation = symbol["translation"];
      for (const auto& language_code : language_codes)
      {
        if (translation.exists(language_code))
        {
          std::string tr = translation[language_code.c_str()];
          LegendGraphicSymbol& lgs = symbols[symbol_name];
          lgs.translations.insert(std::make_pair(language_code, tr));
        }
      }
    }
  }
}

// Parse the layout settings
void parse_layout(const libconfig::Config& config, LegendGraphicLayout& layout)
{
  lookup_if_exists(
      config, "wms.get_legend_graphic.layout.param_name_xoffset", layout.param_name_xoffset);
  lookup_if_exists(
      config, "wms.get_legend_graphic.layout.param_name_yoffset", layout.param_name_yoffset);
  lookup_if_exists(
      config, "wms.get_legend_graphic.layout.param_unit_xoffset", layout.param_unit_xoffset);
  lookup_if_exists(
      config, "wms.get_legend_graphic.layout.param_unit_yoffset", layout.param_unit_yoffset);
  lookup_if_exists(config, "wms.get_legend_graphic.layout.legend_xoffset", layout.legend_xoffset);
  lookup_if_exists(config, "wms.get_legend_graphic.layout.legend_yoffset", layout.legend_yoffset);
  lookup_if_exists(config, "wms.get_legend_graphic.layout.legend_width", layout.legend_width);
  lookup_if_exists(config,
                   "wms.get_legend_graphic.layout.symbol_group_x_padding",
                   layout.symbol_group_x_padding);
  lookup_if_exists(config,
                   "wms.get_legend_graphic.layout.symbol_group_y_padding",
                   layout.symbol_group_y_padding);
  lookup_if_exists(
      config, "wms.get_legend_graphic.layout.symbol_text_xoffset", layout.symbol_text_xoffset);
  lookup_if_exists(
      config, "wms.get_legend_graphic.layout.symbol_text_yoffset", layout.symbol_text_yoffset);
  lookup_if_exists(
      config, "wms.get_legend_graphic.layout.output_document_width", layout.output_document_width);
  lookup_if_exists(config,
                   "wms.get_legend_graphic.layout.output_document_height",
                   layout.output_document_height);
}
}  // namespace

LegendGraphicSettings::LegendGraphicSettings(bool initDefaults /*= false*/)
{
  if (initDefaults)
  {
    // default layout settings
    layout.param_name_xoffset = 20;
    layout.param_name_yoffset = 20;
    layout.param_unit_xoffset = 30;
    layout.param_unit_yoffset = 30;
    layout.legend_xoffset = 30;
    layout.legend_yoffset = 40;
    layout.symbol_group_x_padding = 30;
    layout.symbol_group_y_padding = 35;
    layout.symbol_text_xoffset = 30;
    layout.symbol_text_yoffset = 0;
    layout.legend_width = 10;
    layout.output_document_width = 500;
    layout.output_document_height = 500;
  }
  expires = -1;  // No caching
}

LegendGraphicSettings::LegendGraphicSettings(const libconfig::Config& config) : expires(-1)
{
  // No caching

  if (!config.exists("wms.get_legend_graphic"))
    return;

  const auto& legend_graphic_config = config.lookup("wms.get_legend_graphic");
  if (!legend_graphic_config.isGroup())
    return;

  // symbols to ignore
  if (config.exists("wms.get_legend_graphic.symbols_to_ignore"))
  {
    std::string symbolNames = config.lookup("wms.get_legend_graphic.symbols_to_ignore");
    boost::algorithm::split(symbolsToIgnore, symbolNames, boost::algorithm::is_any_of(","));
  }

  // Control cache expiration
  if (config.exists("wms.get_legend_graphic.expire"))
    expires = config.lookup("wms.get_legend_graphic.expire");

  if (config.exists("wms.get_legend_graphic.parameters"))
  {
    // parameter settings
    const auto& parameter_settings = config.lookup("wms.get_legend_graphic.parameters");

    if (!parameter_settings.isList())
      throw Fmi::Exception(BCP, "wms.get_legend_graphic.parameters must be a list");

    parse_parameters(parameter_settings, parameters);
  }

  if (config.exists("wms.get_legend_graphic.symbols"))
  {
    // symbol settings
    const auto& symbol_settings = config.lookup("wms.get_legend_graphic.symbols");

    if (!symbol_settings.isList())
      throw Fmi::Exception(BCP, "wms.get_legend_graphic.symbols must be a list");

    parse_symbols(symbol_settings, symbols);
  }

  // Overwritten from configuration
  parse_layout(config, layout);
}

void LegendGraphicSettings::merge(const LegendGraphicSettings& settings)
{
  // Parameter settings
  for (const auto& pair : settings.parameters)
  {
    if (parameters.find(pair.first) == parameters.end())
    {
      parameters.insert(std::make_pair(pair.first, pair.second));
    }
    else
    {
      LegendGraphicParameter& lgp = parameters[pair.first];
      const LegendGraphicParameter& settings_lgp = pair.second;
      lgp.data_name = settings_lgp.data_name;
      lgp.given_name = settings_lgp.given_name;
      lgp.unit = settings_lgp.unit;
      lgp.hide_title = settings_lgp.hide_title;
      replace_translations(settings_lgp.translations, lgp.translations);
    }
    for (const auto& item : pair.second.translations)
      languages.insert(item.first);
  }

  // Symbol settings
  for (const auto& pair : settings.symbols)
  {
    if (symbols.find(pair.first) == symbols.end())
    {
      symbols.insert(std::make_pair(pair.first, pair.second));
    }
    else
    {
      LegendGraphicSymbol& lgs = symbols[pair.first];
      const LegendGraphicSymbol& settings_lgs = pair.second;
      replace_translations(settings_lgs.translations, lgs.translations);
      for (const auto& item : lgs.translations)
        languages.insert(item.first);
    }
  }

  // Symbols to ignore
  if (!settings.symbolsToIgnore.empty())
    symbolsToIgnore.insert(settings.symbolsToIgnore.begin(), settings.symbolsToIgnore.end());

  // Layout settings
  set_if_given(layout.param_name_xoffset, settings.layout.param_name_xoffset);
  set_if_given(layout.param_name_yoffset, settings.layout.param_name_yoffset);
  set_if_given(layout.param_unit_xoffset, settings.layout.param_unit_xoffset);
  set_if_given(layout.param_unit_yoffset, settings.layout.param_unit_yoffset);
  set_if_given(layout.symbol_group_x_padding, settings.layout.symbol_group_x_padding);
  set_if_given(layout.symbol_group_y_padding, settings.layout.symbol_group_y_padding);
  set_if_given(layout.symbol_text_xoffset, settings.layout.symbol_text_xoffset);
  set_if_given(layout.symbol_text_yoffset, settings.layout.symbol_text_yoffset);
  set_if_given(layout.legend_xoffset, settings.layout.legend_xoffset);
  set_if_given(layout.legend_yoffset, settings.layout.legend_yoffset);
  set_if_given(layout.legend_width, settings.layout.legend_width);
  // Replace widths per language if exist
  for (const auto& item : settings.layout.legend_width_per_language)
  {
    layout.legend_width_per_language[item.first] = item.second;
    languages.insert(item.first);
  }
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicLayout& layout)
{
  ost << "LegendGraphicLayout:\n";
  if (layout.param_name_xoffset)
    ost << " param_name_xoffset: " << *layout.param_name_xoffset << '\n';
  if (layout.param_name_yoffset)
    ost << " param_name_yoffset: " << *layout.param_name_yoffset << '\n';
  if (layout.param_unit_xoffset)
    ost << " param_unit_xoffset: " << *layout.param_unit_xoffset << '\n';
  if (layout.param_unit_yoffset)
    ost << " param_unit_yoffset: " << *layout.param_unit_yoffset << '\n';
  if (layout.legend_xoffset)
    ost << " legend_xoffset: " << *layout.legend_xoffset << '\n';
  if (layout.legend_yoffset)
    ost << " legend_yoffset: " << *layout.legend_yoffset << '\n';
  if (layout.symbol_group_x_padding)
    ost << " symbol_group_x_padding: " << *layout.symbol_group_x_padding << '\n';
  if (layout.symbol_group_y_padding)
    ost << " symbol_group_y_padding: " << *layout.symbol_group_y_padding << '\n';
  if (layout.symbol_text_xoffset)
    ost << " symbol_text_xoffset: " << *layout.symbol_text_xoffset << '\n';
  if (layout.symbol_text_yoffset)
    ost << " symbol_text_yoffset: " << *layout.symbol_text_yoffset << '\n';
  if (layout.legend_width)
    ost << " legend_width: " << *layout.legend_width << '\n';
  if (!layout.legend_width_per_language.empty())
  {
    ost << " legend_width_per_language: \n";
    for (const auto& item : layout.legend_width_per_language)
      ost << "  - " << item.first << ": " << item.second << '\n';
  }
  ost << " output_doxument_width: " << layout.output_document_width << '\n';
  ost << " output_doxument_height: " << layout.output_document_height << '\n';

  return ost;
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicParameter& lgp)
{
  ost << "LegendGraphicParameter: \n";
  ost << " data_name: " << lgp.data_name << '\n';
  ost << " given_name: " << lgp.given_name << '\n';
  ost << " unit: " << lgp.unit << '\n';
  ost << " hide_title: " << lgp.hide_title << '\n';
  if (!lgp.translations.empty())
  {
    ost << " translations: \n";
    for (const auto& item : lgp.translations)
      ost << "  - " << item.first << ": " << item.second << '\n';
  }
  if (!lgp.text_lengths.empty())
  {
    ost << " text_lengths: \n";
    for (const auto& item : lgp.text_lengths)
      ost << "  - " << item.first << ": " << item.second << '\n';
  }
  return ost;
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicSymbol& lgs)
{
  ost << "LegendGraphicSymbol: \n";
  ost << " symbol_name: " << lgs.symbol_name << '\n';
  if (!lgs.translations.empty())
  {
    ost << " translations: \n";
    for (const auto& item : lgs.translations)
      ost << "  - " << item.first << ": " << item.second << '\n';
  }

  return ost;
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicSettings& lgs)
{
  ost << "**** LegendGraphicSettings begin****\n";
  if (!lgs.parameters.empty())
  {
    ost << "* Parameters *\n";
    for (const auto& p : lgs.parameters)
      ost << " parameter: " << p.first << '\n' << p.second << '\n';
  }
  if (!lgs.symbols.empty())
  {
    ost << "* Symbols *\n";
    for (const auto& s : lgs.symbols)
      ost << " symbol: " << s.first << '\n' << s.second << '\n';
  }
  if (!lgs.symbolsToIgnore.empty())
  {
    ost << "symbolsToIgnore: \n";
    for (const auto& s : lgs.symbolsToIgnore)
      ost << s << '\n';
  }
  if (!lgs.languages.empty())
  {
    ost << "languages: \n";
    for (const auto& l : lgs.languages)
      ost << l << '\n';
  }
  ost << lgs.layout;
  ost << "expires: " << lgs.expires << '\n';

  ost << "**** LegendGraphicSettings end****\n";

  return ost;
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
