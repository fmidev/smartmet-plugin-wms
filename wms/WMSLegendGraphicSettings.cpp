#include "WMSLegendGraphicSettings.h"

#include <vector>

#include <algorithm>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
unsigned int numberOfLanguages = 193;
const char* language_codes[] = {
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

WMSLegendGraphicSettings::WMSLegendGraphicSettings(const libconfig::Config& config)
{
  // default layout settings
  layout.param_name_xoffset = 20;
  layout.param_name_yoffset = 20;
  layout.param_unit_xoffset = 30;
  layout.param_unit_yoffset = 40;
  layout.legend_xoffset = 30;
  layout.legend_yoffset = 40;
  layout.symbol_group_x_padding = 30;
  layout.symbol_group_y_padding = 30;
  layout.legend_width = 150;
  layout.output_document_width = 500;
  layout.output_document_height = 500;
  expires = -1;  // No caching

  if (config.exists("wms.get_legend_graphic"))
  {
    const auto& legend_graphic_config = config.lookup("wms.get_legend_graphic");
    if (legend_graphic_config.isGroup())
    {
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
              parameters.insert(std::make_pair(
                  param_name, LegendGraphicParameter(data_name, name, unit, hide_title)));
            }
          }

          if (param.exists("translation"))
          {
            const auto& translation = param["translation"];
            for (unsigned int k = 0; k < numberOfLanguages; k++)
            {
              if (translation.exists(language_codes[k]))
              {
                std::string language_code = language_codes[k];
                std::string tr = translation[language_code];
                for (auto p : param_names)
                {
                  LegendGraphicParameter& lgp = parameters[p];
                  lgp.translations.insert(std::make_pair(language_code, tr));
                }
              }
            }
          }
        }
      }

      if (config.exists("wms.get_legend_graphic.symbols"))
      {
        // symbol settings
        const auto& symbol_settings = config.lookup("wms.get_legend_graphic.symbols");

        if (!symbol_settings.isList())
          throw Fmi::Exception(BCP, "wms.get_legend_graphic.symbols must be a list");

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
            for (unsigned int k = 0; k < numberOfLanguages; k++)
            {
              if (translation.exists(language_codes[k]))
              {
                std::string language_code = language_codes[k];
                std::string tr = translation[language_code];
                LegendGraphicSymbol& lgs = symbols[symbol_name];
                lgs.translations.insert(std::make_pair(language_code, tr));
              }
            }
          }
        }
      }

      // defaults can be overwritten from configuration
      if (config.exists("wms.get_legend_graphic.layout.param_name_xoffset"))
        layout.param_name_xoffset =
            config.lookup("wms.get_legend_graphic.layout.param_name_xoffset");
      if (config.exists("wms.get_legend_graphic.layout.param_name_yoffset"))
        layout.param_name_yoffset =
            config.lookup("wms.get_legend_graphic.layout.param_name_yoffset");
      if (config.exists("wms.get_legend_graphic.layout.param_unit_xoffset"))
        layout.param_unit_xoffset =
            config.lookup("wms.get_legend_graphic.layout.param_unit_xoffset");
      if (config.exists("wms.get_legend_graphic.layout.param_unit_yoffset"))
        layout.param_unit_yoffset =
            config.lookup("wms.get_legend_graphic.layout.param_unit_yoffset");
      if (config.exists("wms.get_legend_graphic.layout.legend_xoffset"))
        layout.legend_xoffset = config.lookup("wms.get_legend_graphic.layout.legend_xoffset");
      if (config.exists("wms.get_legend_graphic.layout.legend_yoffset"))
        layout.legend_yoffset = config.lookup("wms.get_legend_graphic.layout.legend_yoffset");
      if (config.exists("wms.get_legend_graphic.layout.legend_width"))
        layout.legend_width = config.lookup("wms.get_legend_graphic.layout.legend_width");
      if (config.exists("wms.get_legend_graphic.layout.symbol_group_x_padding"))
        layout.symbol_group_x_padding =
            config.lookup("wms.get_legend_graphic.layout.symbol_group_x_padding");
      if (config.exists("wms.get_legend_graphic.layout.symbol_group_y_padding"))
        layout.symbol_group_y_padding =
            config.lookup("wms.get_legend_graphic.layout.symbol_group_y_padding");
      if (config.exists("wms.get_legend_graphic.layout.output_document_width"))
        layout.output_document_width =
            config.lookup("wms.get_legend_graphic.layout.output_document_width");
      if (config.exists("wms.get_legend_graphic.layout.output_document_height"))
        layout.output_document_height =
            config.lookup("wms.get_legend_graphic.layout.output_document_height");
    }
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
