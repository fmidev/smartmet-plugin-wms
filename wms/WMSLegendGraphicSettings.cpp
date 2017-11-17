#include "WMSLegendGraphicSettings.h"

#include <vector>

#include <algorithm>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
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

      if (config.exists("wms.get_legend_graphic.parameters"))
      {
        // parameter_settings
        const auto& parameter_settings = config.lookup("wms.get_legend_graphic.parameters");

        if (!parameter_settings.isList())
          throw Spine::Exception(BCP, "wms.get_legend_graphic.parameters must be a list");

        for (int i = 0; i < parameter_settings.getLength(); i++)
        {
          const auto& param = parameter_settings[i];

          std::string data_name = (param.exists("data_name") ? param["data_name"] : "");
          std::string name = (param.exists("name") ? param["name"] : "");
          std::string unit = (param.exists("unit") ? param["unit"] : "");

          if (!data_name.empty())
          {
            std::vector<std::string> param_names;
            boost::algorithm::split(param_names, data_name, boost::algorithm::is_any_of(","));

            for (const auto& param_name : param_names)
            {
              parameters.insert(std::make_pair(param_name, LegendGraphicParameter(name, unit)));
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

  /*
  std::cout << "layout.param_name_xoffset: " << layout.param_name_xoffset << std::endl;
  std::cout << "layout.param_name_yoffset: " << layout.param_name_yoffset << std::endl;
  std::cout << "layout.param_unit_xoffset: " << layout.param_unit_xoffset << std::endl;
  std::cout << "layout.param_unit_yoffset: " << layout.param_unit_yoffset << std::endl;
  std::cout << "layout.legend_xoffset: " << layout.legend_xoffset << std::endl;
  std::cout << "layout.legend_yoffset: " << layout.legend_yoffset << std::endl;
  std::cout << "layout.symbol_group_x_padding: " << layout.symbol_group_x_padding << std::endl;
  std::cout << "layout.symbol_group_y_padding: " << layout.symbol_group_y_padding << std::endl;
  std::cout << "layout.legend_width: " << layout.symbol_group_y_padding << std::endl;
  std::cout << "layout.output_document_width: " << layout.output_document_width << std::endl;
  std::cout << "layout.output_document_height: " << layout.output_document_height << std::endl;

  if (!parameters.empty())
  {
    std::cout << "parameters: " << std::endl;
    for (const auto& item : parameters)
      std::cout << item.first << " -> " << item.second.name << ", " << item.second.unit
                << std::endl;
  }
  */
}
}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
