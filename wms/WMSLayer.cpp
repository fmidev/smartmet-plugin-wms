#include "WMSLayer.h"
#include "TemplateFactory.h"
#include "TextUtility.h"
#include "WMS.h"
#include "WMSConfig.h"
#include "WMSException.h"

#include <engines/gis/Engine.h>
#include <spine/Exception.h>

#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <boost/make_shared.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <fmt/format.h>

#include <gdal/ogr_spatialref.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
// wms.longitude must be in range [-180...180] inclusive
double clamp_longitude(double lon)
{
  lon = fmod(lon, 360);
  if (lon < -180)
    lon += 360;
  else if (lon > 180)
    lon -= 360;
  return lon;
}

// wms.latitude must be in range [-90...90] inclusive
double clamp_latitude(double lat)
{
  lat = fmod(lat, 180);
  if (lat < -90)
    lat += 180;
  else if (lat > 90)
    lat -= 180;
  return lat;
}

Json::Value parse_json_string(const std::string theJsonString)
{
  Json::Value json;

  Json::Reader reader;
  bool parsingSuccessful = reader.parse(theJsonString, json);
  if (!parsingSuccessful)
  {
    // report to the user the failure
    throw Spine::Exception(BCP, reader.getFormattedErrorMessages());
  }

  return json;
}

Json::Value process_symbol_group_json(const Json::Value& jsonTemplate,
                                      const WMSLegendGraphicSettings& lgs,
                                      const LegendGraphicInfo& legendGraphicInfo,
                                      unsigned int xpos,
                                      unsigned int ypos,
                                      unsigned int uniqueId)
{
  Json::Value legendJson = jsonTemplate;
  unsigned int symbol_xpos = xpos + *lgs.layout.legend_xoffset + 10;
  unsigned int symbol_ypos = ypos + *lgs.layout.legend_yoffset;
  unsigned int header_ypos = ypos + *lgs.layout.param_name_yoffset;
  unsigned int header_xpos = xpos + *lgs.layout.legend_xoffset;

  for (auto lgi : legendGraphicInfo)
  {
    std::string symbolName = lgi.asString("symbol");

    // some symbols need not to be shown in legend graphis, for exmaple logos
    if (lgs.symbolsToIgnore.find(symbolName) != lgs.symbolsToIgnore.end())
      continue;

    // handling individual symbols
    Json::Value& xPosJson = legendJson["x"];
    Json::Value& layersJson = legendJson["layers"];
    Json::Value& symbolLayerJson = layersJson[layersJson.size() - 2];
    Json::Value& textLayerJson = layersJson[layersJson.size() - 1];
    bool firstSymbol = (xPosJson.asInt() == 99999);
    if (firstSymbol)
    {
      Json::Value& yPosJson = legendJson["y"];
      xPosJson = Json::Value(symbol_xpos);
      yPosJson = Json::Value(symbol_ypos);
      Json::Value& headerLayerJson = layersJson[0];
      Json::Value& cdataJson = headerLayerJson["cdata"];
      std::string legendHeader = lgi.asString("parameter_name");
      cdataJson = Json::Value(legendHeader);
      Json::Value& attributesHeaderJson = headerLayerJson["attributes"];
      Json::Value& xPosHeaderJson = attributesHeaderJson["x"];
      Json::Value& yPosHeaderJson = attributesHeaderJson["y"];
      xPosHeaderJson = Json::Value(header_xpos);
      yPosHeaderJson = Json::Value(header_ypos);

      Json::Value& symbolJson = symbolLayerJson["symbol"];
      symbolJson = Json::Value(lgi.asString("symbol"));
      Json::Value& positionsJson = symbolLayerJson["positions"];
      Json::Value& xPosSymbolJson = positionsJson["x"];
      Json::Value& yPosSymbolJson = positionsJson["y"];
      xPosSymbolJson = Json::Value(symbol_xpos);
      yPosSymbolJson = Json::Value(symbol_ypos - 5);

      Json::Value& cdataTextJson = textLayerJson["cdata"];
      cdataTextJson = Json::Value(lgi.asString("symbol"));

      Json::Value& attributesTextJson = textLayerJson["attributes"];
      Json::Value& xPosSymbolTextJson = attributesTextJson["x"];
      Json::Value& yPosSymbolTextJson = attributesTextJson["y"];
      xPosSymbolTextJson = Json::Value(symbol_xpos + *lgs.layout.symbol_group_x_padding);
      yPosSymbolTextJson = Json::Value(symbol_ypos);
    }
    else
    {
      Json::Value newSymbolLayerJson = symbolLayerJson;
      Json::Value newTextLayerJson = textLayerJson;
      Json::Value& symbolJson = newSymbolLayerJson["symbol"];
      symbolJson = Json::Value(lgi.asString("symbol"));
      Json::Value& positionsJson = newSymbolLayerJson["positions"];
      Json::Value& xPosSymbolJson = positionsJson["x"];
      Json::Value& yPosSymbolJson = positionsJson["y"];
      xPosSymbolJson = Json::Value(symbol_xpos);
      yPosSymbolJson = Json::Value(symbol_ypos - 5);

      Json::Value& cdataTextJson = newTextLayerJson["cdata"];
      cdataTextJson = Json::Value(lgi.asString("symbol"));

      Json::Value& attributesTextJson = newTextLayerJson["attributes"];
      Json::Value& xPosSymbolTextJson = attributesTextJson["x"];
      Json::Value& yPosSymbolTextJson = attributesTextJson["y"];
      xPosSymbolTextJson = Json::Value(symbol_xpos + *lgs.layout.symbol_group_x_padding);
      yPosSymbolTextJson = Json::Value(symbol_ypos);
      layersJson.append(newSymbolLayerJson);
      layersJson.append(newTextLayerJson);
    }
    symbol_ypos += *lgs.layout.symbol_group_y_padding;
  }

  return legendJson;
}

Json::Value set_legend_position(const Json::Value& jsonTemplate,
                                unsigned int xpos,
                                unsigned int ypos)
{
  Json::Value nulljson;
  Json::Value legendJson = jsonTemplate;

  // replace all x- and y-variables in layers- and layers.attribute blocks
  Json::Value& layersJson = (legendJson.isMember("layers") ? legendJson["layers"] : nulljson);
  if (!layersJson.isNull() && layersJson.isArray())
  {
    for (unsigned int i = 0; i < layersJson.size(); i++)
    {
      Json::Value& layerJson = layersJson[i];
      if (!layerJson.isNull())
      {
        Json::Value& layerXposJson = (layerJson.isMember("x") ? layerJson["x"] : nulljson);
        Json::Value& layerYposJson = (layerJson.isMember("y") ? layerJson["y"] : nulljson);
        if (!layerXposJson.isNull())
          layerXposJson = Json::Value(xpos + layerXposJson.asInt());
        if (!layerYposJson.isNull())
          layerYposJson = Json::Value(ypos + layerYposJson.asInt());

        Json::Value& attributesJson =
            (layerJson.isMember("attributes") ? layerJson["attributes"] : nulljson);
        if (!attributesJson.isNull())
        {
          Json::Value& attributesXposJson =
              (attributesJson.isMember("x") ? attributesJson["x"] : nulljson);
          Json::Value& attributesYposJson =
              (attributesJson.isMember("y") ? attributesJson["y"] : nulljson);
          if (!attributesXposJson.isNull())
            attributesXposJson = Json::Value(xpos + attributesXposJson.asInt());
          if (!attributesYposJson.isNull())
            attributesYposJson = Json::Value(ypos + attributesYposJson.asInt());
        }
      }
    }
  }

  return legendJson;
}

void get_legend_dimension(const Json::Value& json,
                          const std::string& blockName,
                          unsigned int& xpos,
                          unsigned int& ypos)
{
  Json::Value nulljson;

  auto xposYposJson = json.get(blockName, nulljson);
  unsigned int textLen = 0;
  auto cdataJson = json.get("cdata", nulljson);
  if (!cdataJson.isNull())
  {
    Dali::text_dimension_t tdim =
        Dali::getTextDimension(cdataJson.asString(), Dali::text_style_t());
    textLen = tdim.width;
  }

  if (!xposYposJson.isNull())
  {
    auto XJson = xposYposJson.get("x", nulljson);
    if (!XJson.isNull())
    {
      unsigned int x = XJson.asInt();
      if (x + textLen > xpos)
        xpos = x + textLen;
    }
    auto YJson = xposYposJson.get("y", nulljson);
    if (!YJson.isNull())
    {
      unsigned int y = YJson.asInt();
      if (y > ypos)
        ypos = y;
    }
  }
}

void process_template_layers(Json::Value& layersJson,
                             const std::string& legendId,
                             const LegendGraphicInfoItem& lgi)
{
  if (!layersJson.isNull() && layersJson.isArray())
  {
    Json::Value nulljson;
    for (unsigned int i = 0; i < layersJson.size(); i++)
    {
      Json::Value& layerJson = layersJson[i];
      if (!layerJson.isNull())
      {
        Json::Value& isobandsJson =
            (layerJson.isMember("isobands") ? layerJson["isobands"] : nulljson);
        if (!isobandsJson.isNull() && lgi.exists("isobands"))
          isobandsJson = lgi.asJsonValue("isobands");

        Json::Value& layerCssJson = (layerJson.isMember("css") ? layerJson["css"] : nulljson);

        if (!layerCssJson.isNull())
          layerCssJson = lgi.asJsonValue("css");

        Json::Value& symbolsJson =
            (layerJson.isMember("symbols") ? layerJson["symbols"] : nulljson);
        if (!symbolsJson.isNull())
        {
          Json::Value& symbolsCssJson =
              (symbolsJson.isMember("css") ? symbolsJson["css"] : nulljson);
          if (!symbolsCssJson.isNull() && lgi.exists("css"))
            symbolsCssJson = lgi.asJsonValue("css");
        }

        Json::Value& attributesJson =
            (layerJson.isMember("attributes") ? layerJson["attributes"] : nulljson);
        if (!attributesJson.isNull())
        {
          Json::Value& idJson = attributesJson["id"];
          if (!idJson.isNull())
          {
            idJson = Json::Value(legendId);
          }
        }

        Json::Value& subLayersJson =
            (layerJson.isMember("layers") ? layerJson["layers"] : nulljson);
        for (unsigned int j = 0; j < subLayersJson.size(); j++)
        {
          Json::Value& subLayerJson = subLayersJson[j];
          if (!subLayerJson.isNull())
          {
            Json::Value& cssJson = (subLayerJson.isMember("css") ? subLayerJson["css"] : nulljson);

            if (!cssJson.isNull())
              cssJson = lgi.asJsonValue("css");

            Json::Value& attributesJson =
                (subLayerJson.isMember("attributes") ? subLayerJson["attributes"] : nulljson);

            if (!attributesJson.isNull())
            {
              Json::Value& classJson =
                  (attributesJson.isMember("class") ? attributesJson["class"] : nulljson);

              if (!classJson.isNull())
                classJson = lgi.asJsonValue("class");
            }
          }
        }
      }
    }
  }
}
std::string unique_id(const std::string& prefix, unsigned int& uniqueId)
{
  return (prefix + Fmi::to_string(uniqueId++));
}

Json::Value process_legend_json(const Json::Value& jsonTemplate,
                                const std::string& legendType,
                                const std::string& parameterName,
                                bool isGenericTemplate,
                                const std::string& legendHeader,
                                const std::string& unit,
                                const WMSLegendGraphicSettings& lgs,
                                const LegendGraphicInfoItem& lgi,
                                unsigned int xpos,
                                unsigned int ypos,
                                unsigned int uniqueId)
{
  Json::Value nulljson;
  Json::Value legendJson = jsonTemplate;
  std::string legendId = (legendType + "_" + parameterName + "_" + Fmi::to_string(uniqueId));

  // If legend is parameter-specific, update layers and position-variables
  if (!isGenericTemplate)
  {
    Json::Value& layersJson = (legendJson.isMember("layers") ? legendJson["layers"] : nulljson);
    process_template_layers(layersJson, legendId, lgi);

    return set_legend_position(legendJson, xpos, ypos);
  }

  // Update definitions
  auto json = legendJson.get("defs", nulljson);
  if (!json.isNull())
  {
    Json::Value& defsJson = legendJson["defs"];
    Json::Value& layersJson = (defsJson.isMember("layers") ? defsJson["layers"] : nulljson);
    process_template_layers(layersJson, legendId, lgi);
  }
  Json::Value& xposJson = legendJson["x"];
  Json::Value& yposJson = legendJson["y"];

  if (legendType != "symbol")
  {
    xposJson = Json::Value(xpos + *lgs.layout.legend_xoffset);
    yposJson = Json::Value(ypos + *lgs.layout.legend_yoffset);
  }

  if (legendType == "isoband")
  {
    Json::Value& isobandsJson = legendJson["isobands"];
    isobandsJson = lgi.asJsonValue("isobands");
    Json::Value& symbolsJson = legendJson["symbols"];
    Json::Value& cssJson = symbolsJson["css"];
    Json::Value& idJson = symbolsJson["symbol"];
    cssJson = lgi.asJsonValue("css");
    idJson = Json::Value(legendId);

    Json::Value& layersJson = legendJson["layers"];
    if (!layersJson.isNull() && layersJson.isArray())
    {
      for (unsigned int i = 0; i < layersJson.size(); i++)
      {
        Json::Value& layerJson = layersJson[i];
        if (!layerJson.isNull())
        {
          Json::Value& cdataJson = layerJson["cdata"];
          if (!cdataJson.isNull())
          {
            bool isHeader = (cdataJson.asString() == "isoband_header");
            Json::Value& attributesJson = layerJson["attributes"];
            if (isHeader)
            {
              cdataJson = Json::Value(legendHeader);
              Json::Value& headerXPosJson = attributesJson["x"];
              headerXPosJson = Json::Value(xpos + *lgs.layout.param_name_xoffset);
              Json::Value& headerYPosJson = attributesJson["y"];
              headerYPosJson = Json::Value(ypos + *lgs.layout.param_name_yoffset);
            }
            else
            {
              cdataJson = Json::Value(unit);
              Json::Value& headerXPosJson = attributesJson["x"];
              headerXPosJson = Json::Value(xpos + *lgs.layout.param_unit_xoffset);
              Json::Value& headerYPosJson = attributesJson["y"];
              headerYPosJson = Json::Value(ypos + *lgs.layout.param_unit_yoffset);
            }
          }
        }
      }
    }
  }
  else if (legendType == "isoline")
  {
    Json::Value& layersJson = legendJson["layers"];
    if (!layersJson.isNull() && layersJson.isArray())
    {
      for (unsigned int i = 0; i < layersJson.size(); i++)
      {
        Json::Value& layerJson = layersJson[i];
        if (!layerJson.isNull())
        {
          Json::Value& symbolJson = layerJson["symbol"];
          if (!symbolJson.isNull())
            symbolJson = Json::Value(legendId);
          Json::Value& positionsJson = layerJson["positions"];

          if (!positionsJson.isNull())
          {
            Json::Value& xPosJson = positionsJson["x"];
            Json::Value& yPosJson = positionsJson["y"];
            xPosJson = Json::Value(xpos + *lgs.layout.legend_xoffset);
            yPosJson = Json::Value(ypos + *lgs.layout.legend_yoffset - 5);
          }
          Json::Value& cdataJson = layerJson["cdata"];
          if (!cdataJson.isNull())
          {
            cdataJson = Json::Value(legendHeader + " isoline");
            Json::Value& attributesJson = layerJson["attributes"];
            if (!attributesJson.isNull())
            {
              Json::Value& xPosJson = attributesJson["x"];
              Json::Value& yPosJson = attributesJson["y"];
              xPosJson = Json::Value(xpos + *lgs.layout.param_name_xoffset);
              yPosJson = Json::Value(ypos + *lgs.layout.param_name_yoffset);
            }
          }
        }
      }
    }
  }
  else if (legendType == "symbol")
  {
    if (parameterName == "PrecipitationForm")
    {
      Json::Value& xPosJson = legendJson["x"];
      Json::Value& yPosJson = legendJson["y"];
      xPosJson = Json::Value(xpos);
      yPosJson = Json::Value(ypos);
      Json::Value& layersJson = legendJson["layers"];
      for (unsigned int i = 0; i < layersJson.size(); i++)
      {
        Json::Value& layerJson = layersJson[i];
        if (!layerJson.isNull())
        {
          Json::Value& cdataJson = layerJson["cdata"];
          if (!cdataJson.isNull())
          {
            std::string cdata = cdataJson.asString();
            Json::Value& attributesJson = layerJson["attributes"];
            Json::Value& xPosJson = attributesJson["x"];
            Json::Value& yPosJson = attributesJson["y"];

            if (cdata == "precipitation_form_header")
            {
              cdataJson = Json::Value(legendHeader);
              xPosJson = Json::Value(xpos + 20);
              yPosJson = Json::Value(ypos + 20);
            }
            else if (cdata == "rain")
            {
              xPosJson = Json::Value(xpos + *lgs.layout.param_name_xoffset +
                                     *lgs.layout.symbol_group_x_padding);
              yPosJson = Json::Value(ypos + *lgs.layout.legend_yoffset);
            }
            else if (cdata == "drizzle")
            {
              xPosJson = Json::Value(xpos + *lgs.layout.param_name_xoffset +
                                     *lgs.layout.symbol_group_x_padding);
              yPosJson = Json::Value(ypos + *lgs.layout.legend_yoffset +
                                     *lgs.layout.symbol_group_y_padding);
            }
            else if (cdata == "snow")
            {
              xPosJson = Json::Value(xpos + *lgs.layout.param_name_xoffset +
                                     *lgs.layout.symbol_group_x_padding);
              yPosJson = Json::Value(ypos + *lgs.layout.legend_yoffset +
                                     (*lgs.layout.symbol_group_y_padding * 2));
            }
          }
          Json::Value& positionsJson = layerJson["positions"];
          if (!positionsJson.isNull())
          {
            Json::Value& xPosJson = positionsJson["x"];
            Json::Value& yPosJson = positionsJson["y"];
            Json::Value& symbolJson = layerJson["symbol"];
            std::string symbol = symbolJson.asString();
            if (symbol == "rain")
            {
              xPosJson = Json::Value(xpos + *lgs.layout.legend_xoffset);
              yPosJson = Json::Value(ypos + *lgs.layout.legend_yoffset);
            }
            else if (symbol == "drizzle")
            {
              xPosJson = Json::Value(xpos + *lgs.layout.legend_xoffset);
              yPosJson = Json::Value(ypos + *lgs.layout.legend_yoffset +
                                     *lgs.layout.symbol_group_y_padding);
            }
            else if (symbol == "snow")
            {
              xPosJson = Json::Value(xpos + *lgs.layout.legend_xoffset);
              yPosJson = Json::Value(ypos + *lgs.layout.legend_yoffset +
                                     (*lgs.layout.symbol_group_y_padding * 2));
            }
          }
        }
      }
    }
    else
    {
      Json::Value& yPosLegendJson = legendJson["y"];
      Json::Value& xPosLegendJson = legendJson["x"];
      xPosLegendJson = Json::Value(xpos);
      yPosLegendJson = Json::Value(ypos);
      Json::Value& layersJson = legendJson["layers"];
      Json::Value& headerLayerJson = layersJson[0];
      Json::Value& cdataJson = headerLayerJson["cdata"];
      cdataJson = Json::Value(parameterName);
      Json::Value& attributesHeaderJson = headerLayerJson["attributes"];
      Json::Value& xPosHeaderJson = attributesHeaderJson["x"];
      Json::Value& yPosHeaderJson = attributesHeaderJson["y"];
      xPosHeaderJson = Json::Value(xpos);
      yPosHeaderJson = Json::Value(ypos);

      Json::Value& symbolLayerJson = layersJson[1];
      Json::Value& textLayerJson = layersJson[2];
      Json::Value& symbolJson = symbolLayerJson["symbol"];
      symbolJson = lgi.asJsonValue("symbol");
      Json::Value& positionsJson = symbolLayerJson["positions"];
      Json::Value& xPosSymbolJson = positionsJson["x"];
      Json::Value& yPosSymbolJson = positionsJson["y"];
      unsigned int symbol_xpos = (xpos + *lgs.layout.legend_xoffset);
      unsigned int symbol_ypos = (ypos + *lgs.layout.legend_yoffset);
      xPosSymbolJson = Json::Value(symbol_xpos);
      yPosSymbolJson = Json::Value(symbol_ypos);

      Json::Value& cdataTextJson = textLayerJson["cdata"];
      cdataTextJson = lgi.asJsonValue("symbol");

      Json::Value& attributesTextJson = textLayerJson["attributes"];
      Json::Value& xPosSymbolTextJson = attributesTextJson["x"];
      Json::Value& yPosSymbolTextJson = attributesTextJson["y"];
      xPosSymbolTextJson = Json::Value(symbol_xpos + *lgs.layout.symbol_group_x_padding);
      yPosSymbolTextJson = Json::Value(symbol_ypos + 10);
    }
  }

  return legendJson;
}

LegendGraphicInfo handle_json_layers(Json::Value layersJson)
{
  LegendGraphicInfo ret;
  Json::Value nulljson;

  for (unsigned int i = 0; i < layersJson.size(); i++)
  {
    const Json::Value& layerJson = layersJson[i];
    if (!layerJson.isNull())
    {
      LegendGraphicInfoItem lgi;
      auto layerTypeJson = layerJson.get("layer_type", nulljson);
      if (!layerTypeJson.isNull())
      {
        std::string layerTypeString = layerTypeJson.asString();
        auto json = layerJson.get("parameter", nulljson);

        if (!json.isNull())
          lgi.add("parameter_name", json);

        if (layerTypeString == "icemap")
        {
          json = layerJson.get("layer_subtype", nulljson);

          if (!json.isNull())
            layerTypeString = json.asString();
          lgi.add("layer_subtype", json);
        }
        if (layerTypeString != "isoband" && layerTypeString != "isoline" &&
            layerTypeString != "symbol")
          continue;

        lgi.add("layer_type", layerTypeJson);

        json = layerJson.get("isobands", nulljson);
        if (!json.isNull())
          lgi.add("isobands", json);
        json = layerJson.get("isolines", nulljson);
        if (!json.isNull())
          lgi.add("isolines", json);
        json = layerJson.get("css", nulljson);
        if (!json.isNull())
          lgi.add("css", json);
        json = layerJson.get("symbol", nulljson);
        if (!json.isNull())
          lgi.add("symbol", json);
        json = layerJson.get("attributes", nulljson);
        if (!json.isNull())
        {
          json = json.get("class", nulljson);
          if (!json.isNull())
            lgi.add("class", json);
        }
        ret.push_back(lgi);
      }
    }

    auto layersJson = layerJson.get("layers", nulljson);
    if (!layersJson.isNull() && layersJson.isArray())
    {
      LegendGraphicInfo recursiveRet = handle_json_layers(layersJson);
      ret.insert(ret.end(), recursiveRet.begin(), recursiveRet.end());
    }
  }

  return ret;
}

void get_legend_graphic_settings(const Json::Value& root, WMSLegendGraphicSettings& settings)
{
  Json::Value nulljson;

  auto json = root.get("defs", nulljson);

  if (json.isNull())
    return;

  auto legendGraphicJson = json.get("get_legend_graphic", nulljson);

  if (legendGraphicJson.isNull())
    return;

  auto parametersJson = legendGraphicJson.get("parameters", nulljson);

  // parameters
  for (unsigned int i = 0; i < parametersJson.size(); i++)
  {
    auto parameterJson = parametersJson[i];

    std::string data_name;
    std::string name;
    std::string unit;

    json = parameterJson.get("data_name", nulljson);
    if (!json.isNull())
      data_name = json.asString();
    json = parameterJson.get("name", nulljson);
    if (!json.isNull())
      name = json.asString();
    json = parameterJson.get("unit", nulljson);
    if (!json.isNull())
      unit = json.asString();

    if (!data_name.empty())
    {
      std::vector<std::string> param_names;
      boost::algorithm::split(param_names, data_name, boost::algorithm::is_any_of(","));

      for (const auto& param_name : param_names)
      {
        settings.parameters.insert(std::make_pair(param_name, LegendGraphicParameter(name, unit)));
      }
    }
  }

  // layout
  auto layoutJson = legendGraphicJson.get("layout", nulljson);

  if (json.isNull())
    return;

  json = layoutJson.get("param_name_xoffset", nulljson);
  if (!json.isNull())
    settings.layout.param_name_xoffset = json.asInt();
  json = layoutJson.get("param_name_yoffset", nulljson);
  if (!json.isNull())
    settings.layout.param_name_yoffset = json.asInt();
  json = layoutJson.get("param_unit_xoffset", nulljson);
  if (!json.isNull())
    settings.layout.param_unit_xoffset = json.asInt();
  json = layoutJson.get("param_unit_yoffset", nulljson);
  if (!json.isNull())
    settings.layout.param_unit_yoffset = json.asInt();
  json = layoutJson.get("symbol_group_x_padding", nulljson);
  if (!json.isNull())
    settings.layout.symbol_group_x_padding = json.asInt();
  json = layoutJson.get("symbol_group_y_padding", nulljson);
  if (!json.isNull())
    settings.layout.symbol_group_y_padding = json.asInt();
  json = layoutJson.get("legend_xoffset", nulljson);
  if (!json.isNull())
    settings.layout.legend_xoffset = json.asInt();
  json = layoutJson.get("legend_yoffset", nulljson);
  if (!json.isNull())
    settings.layout.legend_yoffset = json.asInt();
  json = layoutJson.get("legend_width", nulljson);
  if (!json.isNull())
    settings.layout.legend_width = json.asInt();
}

unsigned int isoband_legend_width(const Json::Value& json, unsigned int def)
{
  Json::Value nulljson;
  auto widthJson = json.get("fixed_width", nulljson);
  unsigned int ret = def;

  try
  {
    if (!widthJson.isNull())
    {
      if (widthJson.isConvertibleTo(Json::ValueType::uintValue))
        ret = widthJson.asUInt();
      if (widthJson.isConvertibleTo(Json::ValueType::stringValue))
        ret = Fmi::stoi(widthJson.asString());
    }
  }
  catch (...)
  {
    throw Spine::Exception(
        BCP, "'fixed_width' attribute must be integer: " + widthJson.toStyledString());
  }

  return ret;
}

unsigned int isoband_legend_height(const Json::Value& json)
{
  unsigned int ret = 0;
  unsigned int numberOfIsobands = 0;
  unsigned int dy = 0;
  unsigned int yOffset = 0;
  Json::Value nulljson;

  if (json.isNull())
    return 0;

  Json::Value jsonValue = json;

  auto jsonItem = jsonValue.get("isobands", nulljson);
  Spine::JSON::dereference(jsonItem);
  if (!jsonItem.isNull() && jsonItem.isArray())
  {
    numberOfIsobands = jsonItem.size();
    jsonItem = jsonValue.get("dy", nulljson);
    if (!jsonItem.isNull())
      dy = jsonItem.asInt();
    jsonItem = jsonValue.get("y", nulljson);
    if (!jsonItem.isNull())
      yOffset = jsonItem.asInt();
    ret = (yOffset + (numberOfIsobands * dy) + 20);
  }
  else
  {
    auto layersJson = jsonValue.get("layers", nulljson);

    if (!layersJson.isNull() && layersJson.isArray())
    {
      unsigned int size = 0;
      for (auto layer : layersJson)
      {
        unsigned int sizeOfIsobands = isoband_legend_height(layer);
        if (sizeOfIsobands > size)
          size = sizeOfIsobands;
      }
      ret = size;
    }
  }

  return ret;
}

std::map<std::string, Json::Value> readLegendDirectory(const std::string& legendDirectory)
{
  std::map<std::string, Json::Value> ret;

  boost::filesystem::path p(legendDirectory);
  if (boost::filesystem::exists(p) && boost::filesystem::is_directory(p))
  {
    boost::filesystem::directory_iterator end_itr;
    for (boost::filesystem::directory_iterator itr(legendDirectory); itr != end_itr; ++itr)
    {
      if (is_regular_file(itr->status()))
      {
        std::string paramname = itr->path().stem().string();
        Json::Value json = WMSLayer::readJsonFile(itr->path().string());
        ret.insert(std::make_pair(paramname, json));
      }
    }
  }

  return ret;
}

std::map<std::string, Json::Value> readLegendFiles(const std::string& wmsroot,
                                                   const std::string& customer)
{
  // First read common templates from wmsroot/legends/templates
  std::map<std::string, Json::Value> legend_templates =
      readLegendDirectory(wmsroot + "/legends/templates");
  // Then read customer specific templates from wmsroot/customer/legends/templates
  std::map<std::string, Json::Value> customer_specific =
      readLegendDirectory(wmsroot + "/customers/" + customer + "/legends/templates");
  // Merge and replace common templates with customer specific templates
  for (auto t : customer_specific)
    if (legend_templates.find(t.first) == legend_templates.end())
      legend_templates.insert(std::make_pair(t.first, t.second));
    else
      legend_templates[t.first] = t.second;

  return legend_templates;
}

}  // anonymous namespace

// Remember to run updateLayerMetaData before using the layer!
WMSLayer::WMSLayer(const WMSConfig& config)
    : wmsConfig(config),
      hidden(false),
      metadataTimestamp(boost::posix_time::not_a_date_time),
      metadataUpdateInterval(5)
{
  wmsConfig.getDaliConfig().getConfig().lookupValue("wms.get_capabilities.update_interval",
                                                    metadataUpdateInterval);

  geographicBoundingBox.xMin = 0.0;
  geographicBoundingBox.xMax = 0.0;
  geographicBoundingBox.yMin = 0.0;
  geographicBoundingBox.yMax = 0.0;
}

void WMSLayer::addStyle(const Json::Value& root,
                        const std::string& layerName,
                        const WMSLayerStyle& layerStyle)
{
  Json::Value nulljson;

  std::string legendGraphicInfoName = layerName + "::" + layerStyle.name;
  if (itsNamedLegendGraphicInfo.find(legendGraphicInfoName) == itsNamedLegendGraphicInfo.end())
    itsNamedLegendGraphicInfo.insert(make_pair(legendGraphicInfoName, LegendGraphicInfo()));

  LegendGraphicInfo& legendGraphicInfo = itsNamedLegendGraphicInfo[legendGraphicInfoName];

  auto viewsJson = root.get("views", nulljson);
  if (!viewsJson.isNull() && viewsJson.isArray())
  {
    for (unsigned int i = 0; i < viewsJson.size(); i++)
    {
      auto viewJson = viewsJson[i];
      auto layersJson = viewJson.get("layers", nulljson);
      if (!layersJson.isNull() && layersJson.isArray())
      {
        LegendGraphicInfo lgi = handle_json_layers(layersJson);

        if (lgi.size() > 0)
          legendGraphicInfo.insert(legendGraphicInfo.end(), lgi.begin(), lgi.end());
      }
    }
  }

  if (!legendGraphicInfo.empty())
  {
    get_legend_graphic_settings(root, legendGraphicSettings);
    std::string legendGraphicID =
        layerName + "::" + (layerStyle.name.empty() ? "default" : layerStyle.name);
    //   WMSLegendGraphicSettings itsLegendGraphicSettings;
    //	WMSLegendGraphicSettings lgs = wmsConfig.getLegendGraphicSettings();
    LegendGraphicResult legendGraphicResult =
        getLegendGraphic(wmsConfig.getLegendGraphicSettings(), legendGraphicID);
    WMSLayerStyle style = layerStyle;
    style.legend_url.width = legendGraphicResult.width;
    style.legend_url.height = legendGraphicResult.height;
    addStyle(layerName, style);
  }
}

void WMSLayer::addStyle(const std::string& layerName, const WMSLayerStyle& layerStyle)
{
  WMSLayerStyle layersStyle(layerStyle);

  layersStyle.legend_url.online_resource =
      std::string(
          "__hostname____apikey__/wms"
          "?service=WMS&amp;request=GetLegendGraphic&amp;version=1.3.0&amp;sld_version=1.1.0&amp;"
          "style=" +
          Spine::HTTP::urlencode(layersStyle.name) + "&amp;format=image%2Fpng&amp;layer=") +
      Spine::HTTP::urlencode(layerName);

  itsStyles.push_back(layersStyle);
}

LegendGraphicResult WMSLayer::getLegendGraphic(const WMSLegendGraphicSettings& settings,
                                               const std::string& legendGraphicID) const
{
  LegendGraphicResult ret;

  if (itsNamedLegendGraphicInfo.find(legendGraphicID) == itsNamedLegendGraphicInfo.end())
    return ret;

  const LegendGraphicInfo& legendGraphicInfo = itsNamedLegendGraphicInfo.at(legendGraphicID);

  ret.height = 20;
  ret.width = 150;
  unsigned int xpos = 0;
  unsigned int ypos = 0;

  // settings from wms configuration file
  WMSLegendGraphicSettings actualSettings = settings;

  // settings from layer file (product file) will overwrite wms configuration file settings
  for (const auto& pair : legendGraphicSettings.parameters)
    actualSettings.parameters[pair.first] = pair.second;

  if (legendGraphicSettings.layout.param_name_xoffset)
    actualSettings.layout.param_name_xoffset = legendGraphicSettings.layout.param_name_xoffset;
  if (legendGraphicSettings.layout.param_name_yoffset)
    actualSettings.layout.param_name_yoffset = legendGraphicSettings.layout.param_name_yoffset;
  if (legendGraphicSettings.layout.param_unit_xoffset)
    actualSettings.layout.param_unit_xoffset = legendGraphicSettings.layout.param_unit_xoffset;
  if (legendGraphicSettings.layout.param_unit_yoffset)
    actualSettings.layout.param_unit_yoffset = legendGraphicSettings.layout.param_unit_yoffset;
  if (legendGraphicSettings.layout.symbol_group_x_padding)
    actualSettings.layout.symbol_group_x_padding =
        legendGraphicSettings.layout.symbol_group_x_padding;
  if (legendGraphicSettings.layout.symbol_group_y_padding)
    actualSettings.layout.symbol_group_y_padding =
        legendGraphicSettings.layout.symbol_group_y_padding;
  if (legendGraphicSettings.layout.legend_xoffset)
    actualSettings.layout.legend_xoffset = legendGraphicSettings.layout.legend_xoffset;
  if (legendGraphicSettings.layout.legend_yoffset)
    actualSettings.layout.legend_yoffset = legendGraphicSettings.layout.legend_yoffset;
  if (legendGraphicSettings.layout.legend_width)
    actualSettings.layout.legend_width = legendGraphicSettings.layout.legend_width;

  std::map<std::string, Json::Value> legends =
      readLegendFiles(wmsConfig.getDaliConfig().rootDirectory(true), customer);
  std::set<std::string> processedParameters;
  unsigned int uniqueId = 0;

  //  symbol_group symbolGroup;
  LegendGraphicInfo symbolGroupInfo;
  Json::Value symbolGroupJson;
  for (auto lgi : legendGraphicInfo)
  {
    std::string key;
    std::string layerType = lgi.asString("layer_type");
    std::string layerSubType = lgi.asString("layer_subtype");
    std::string parameterName = lgi.asString("parameter_name");
    if (layerType == "icemap" && layerSubType == "symbol")
      layerType = "symbol";

    // handle all symbols together in the end
    if (layerType == "symbol" && parameterName != "PrecipitationForm")
    {
      symbolGroupInfo.push_back(lgi);
      continue;
    }

    const LegendGraphicParameter* parameterSettings = nullptr;
    if (actualSettings.parameters.find(parameterName) != actualSettings.parameters.end())
      parameterSettings = &(actualSettings.parameters.at(parameterName));

    std::string legendHeader = (parameterSettings ? parameterSettings->name : parameterName);

    bool isGenericTemplate = false;
    // Legends are identified by parameter name + layer type (e.g. Temperature_isoline,
    // Temperature_isoband), or by plain layer type (e.g. isoline, isoband)
    // Precipitation form is special case
    if (layerType == "symbol" && parameterName == "PrecipitationForm")
    {
      isGenericTemplate = true;
      key = parameterName;
    }
    else if (legends.find(parameterName + "_" + layerType) != legends.end())
      key = (parameterName + "_" + layerType);
    else if (legends.find(layerType) != legends.end())
    {
      isGenericTemplate = true;
      key = layerType;
    }

    if (key.empty())
      continue;

    Json::Value templateJson = legends.at(key);

    // Generic template file is same for all layers of same type (e.g. all isobands),
    // non-generig is parameter-specific and only location need to be modified here
    std::string unit = (parameterSettings ? parameterSettings->unit : "");
    auto legendJson = process_legend_json(templateJson,
                                          layerType,
                                          parameterName,
                                          isGenericTemplate,
                                          legendHeader,
                                          unit,
                                          actualSettings,
                                          lgi,
                                          xpos,
                                          ypos,
                                          uniqueId++);

    unsigned int labelHeight = 0;
    unsigned int isobandWidth = 0;
    if (layerType == "isoband")
    {
      std::string customerRoot =
          (wmsConfig.getDaliConfig().rootDirectory(true) + "/customers/" + customer);
      std::string layersRoot = customerRoot + "/layers/";
      Spine::JSON::preprocess(legendJson,
                              wmsConfig.getDaliConfig().rootDirectory(true),
                              layersRoot,
                              wmsConfig.getJsonCache());

      labelHeight = isoband_legend_height(legendJson);
      isobandWidth = isoband_legend_width(legendJson, *(actualSettings.layout.legend_width));
    }
    else if (layerType == "isoline")
      labelHeight = ypos + (*actualSettings.layout.legend_yoffset * 2);
    else if (layerType == "symbol")
    {
      Json::Value nulljson;
      auto layersJson = legendJson.get("layers", nulljson);

      if (!layersJson.isNull() && layersJson.isArray() &&
          actualSettings.layout.symbol_group_y_padding)
      {
        for (auto layerJson : layersJson)
        {
          auto layerTypeJson = layerJson.get("layer_type", nulljson);
          if (!layerTypeJson.isNull())
            labelHeight += *(actualSettings.layout.symbol_group_y_padding);
        }
        if (actualSettings.layout.symbol_group_y_padding)
          labelHeight += *(actualSettings.layout.symbol_group_y_padding);
        if (actualSettings.layout.param_name_yoffset)
          labelHeight += *(actualSettings.layout.param_name_yoffset);
      }
    }

    if (labelHeight > ret.height)
      ret.height = labelHeight;

    ret.legendLayers.push_back(legendJson.toStyledString());

    xpos += (*(actualSettings.layout.legend_width) > isobandWidth
                 ? *(actualSettings.layout.legend_width)
                 : isobandWidth);

    processedParameters.insert(parameterName + layerType);
  }

  ret.width = xpos;

  // symbol group
  if (symbolGroupInfo.size() > 0)
  {
    Json::Value symbolGroup = process_symbol_group_json(
        legends.at("symbol"), actualSettings, symbolGroupInfo, xpos, ypos, uniqueId);
    ret.legendLayers.push_back(symbolGroup.toStyledString());

    Json::Value nulljson;
    auto layersJson = symbolGroup.get("layers", nulljson);
    if (!layersJson.isNull() && layersJson.isArray())
    {
      for (const auto& layer : layersJson)
      {
        get_legend_dimension(layer, "attributes", ret.width, ret.height);
        get_legend_dimension(layer, "positions", ret.width, ret.height);
      }
    }
    ret.width += *actualSettings.layout.symbol_group_x_padding;
    ret.height += *actualSettings.layout.symbol_group_y_padding;
  }

  return ret;
}

bool WMSLayer::isValidCRS(const std::string& theCRS) const
{
  try
  {
    if (crs.find(theCRS) != crs.end())
      return true;

    return false;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Validating CRS failed!");
  }
}

bool WMSLayer::isValidStyle(const std::string& theStyle) const
{
  try
  {
    for (auto style : itsStyles)
      if (style.name == theStyle)
        return true;

    return false;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Validating style failed!");
  }
}

bool WMSLayer::isValidTime(const boost::posix_time::ptime& theTime) const
{
  try
  {
    if (timeDimension)
      return timeDimension->isValidTime(theTime);
    else
      return true;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Validating time failed!");
  }
}

bool WMSLayer::currentValue() const
{
  try
  {
    // if time dimension is not present, time-option is irrelevant
    return (timeDimension ? timeDimension->currentValue() : true);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Validating current time failed!");
  }
}

boost::posix_time::ptime WMSLayer::mostCurrentTime() const
{
  try
  {
    return (timeDimension ? timeDimension->mostCurrentTime() : boost::posix_time::not_a_date_time);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Validating most current time!");
  }
}

std::string WMSLayer::info() const
{
  try
  {
    std::stringstream ss;

    ss << *this;

    return ss.str();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "WMS info operation failed!");
  }
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfoItem& lgi)
{
  Json::StyledWriter writer;
  for (auto elem : lgi.info)
  {
    std::cout << elem.first << ":\n" << writer.write(elem.second) << std::endl;
  }

  return ost;
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfo& lgi)
{
  Json::StyledWriter writer;
  for (auto elem : lgi)
  {
    std::cout << elem << std::endl;
  }

  return ost;
}

std::ostream& operator<<(std::ostream& ost, const boost::optional<int>& var)
{
  if (!var)
    ost << "-";
  else
    ost << *var;
  return ost;
}

std::ostream& operator<<(std::ostream& ost, const WMSLayer& layer)
{
  try
  {
    const std::string missing = "-";
    ost << "********** "
        << "name: " << layer.name << " **********"
        << "\n"
        << "title: " << layer.title << "\n"
        << "abstract: " << layer.abstract << "\n"
        << "keywords: "
        << (!layer.keywords ? missing : boost::algorithm::join(*layer.keywords, " ")) << "\n"
        << "opaque: " << layer.opaque << "\n"
        << "queryable: " << layer.queryable << "\n"
        << "cascaded: " << layer.cascaded << "\n"
        << "no_subsets: " << layer.no_subsets << "\n"
        << "fixed_width: " << layer.fixed_width << "\n"
        << "fixed_height:" << layer.fixed_height << "\n"
        << "geographicBoundingBox: \n"
        << " westBoundLongitude=" << layer.geographicBoundingBox.xMin << " "
        << "southBoundLatitude=" << layer.geographicBoundingBox.yMin << " "
        << "eastBoundLongitude=" << layer.geographicBoundingBox.xMax << " "
        << "northBoundLatitude=" << layer.geographicBoundingBox.yMax << "\n"
        << "crs:";
    for (auto c : layer.crs)
      ost << c.first << "=" << c.second << " ";
    ost << "styles:";
    for (auto style : layer.itsStyles)
      ost << style.name << " ";
    if (layer.itsStyles.size() > 0)
      ost << " \ncustomer: " << layer.customer << "\n";

    if (layer.timeDimension)
      ost << "timeDimension: " << layer.timeDimension;
    else
      ost << "timeDimension: -";

    ost << "*******************************" << std::endl;

    return ost;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Printing the request failed!");
  }
}

boost::optional<CTPP::CDT> WMSLayer::generateGetCapabilities(const Engine::Gis::Engine& gisengine)
{
  try
  {
    if (hidden)
      return {};

    CTPP::CDT layer(CTPP::CDT::HASH_VAL);

    // Layer name, title and abstract
    if (!name.empty())
      layer["name"] = name;
    if (title)
      layer["title"] = *title;
    if (abstract)
      layer["abstract"] = *abstract;
    if (opaque)
      layer["opaque"] = *opaque;
    if (queryable)
      layer["queryable"] = *queryable;
    if (cascaded)
      layer["cascaded"] = *cascaded;
    if (no_subsets)
      layer["no_subsets"] = *no_subsets;
    if (fixed_width)
      layer["fixed_width"] = *fixed_width;
    if (fixed_height)
      layer["fixed_height"] = *fixed_height;

    if (keywords)
    {
      CTPP::CDT keys(CTPP::CDT::ARRAY_VAL);
      for (const auto& key : *keywords)
        keys.PushBack(key);
      layer["keyword"] = keys;
    }

    // Layer geographic bounding boxes

    CTPP::CDT layer_bbox(CTPP::CDT::HASH_VAL);
    layer_bbox["west_bound_longitude"] = clamp_longitude(geographicBoundingBox.xMin);
    layer_bbox["east_bound_longitude"] = clamp_longitude(geographicBoundingBox.xMax);
    layer_bbox["south_bound_latitude"] = clamp_latitude(geographicBoundingBox.yMin);
    layer_bbox["north_bound_latitude"] = clamp_latitude(geographicBoundingBox.yMax);
    layer["ex_geographic_bounding_box"] = layer_bbox;

    // Layer CRS list and their bounding boxes

    // Calculate CRS bbox from latlon bbox
    OGRSpatialReference srs;
    srs.importFromEPSGA(4326);

    if (!crs.empty())
    {
      CTPP::CDT layer_crs_list(CTPP::CDT::ARRAY_VAL);
      CTPP::CDT layer_bbox_list(CTPP::CDT::ARRAY_VAL);

      for (const auto& id_decl : crs)
      {
        CTPP::CDT layer_bbox(CTPP::CDT::HASH_VAL);

        const auto& id = id_decl.first;
        const auto& decl = id_decl.second;

        layer_bbox["crs"] = id;

        if (id == "EPSG:4326")
        {
          layer_crs_list.PushBack(id);
          layer_bbox["minx"] = geographicBoundingBox.xMin;
          layer_bbox["miny"] = geographicBoundingBox.yMin;
          layer_bbox["maxx"] = geographicBoundingBox.xMax;
          layer_bbox["maxy"] = geographicBoundingBox.yMax;
          layer_bbox_list.PushBack(layer_bbox);
        }
        else
        {
          OGRSpatialReference target;
          auto err = target.SetFromUserInput(decl.c_str());
          if (err != OGRERR_NONE)
            throw Spine::Exception(BCP, "Unknown spatial reference declaration: '" + decl + "'");

          boost::shared_ptr<OGRCoordinateTransformation> transformation(
              OGRCreateCoordinateTransformation(&srs, &target));

          if (transformation == nullptr)
            throw Spine::Exception(BCP, "OGRCreateCoordinateTransformation function call failed");

          // Intersect with target EPSG bounding box (latlon) if it is available

          Engine::Gis::BBox epsg_box = crs_bbox.at(id);
          auto x1 = std::max(geographicBoundingBox.xMin, epsg_box.west);
          auto x2 = std::min(geographicBoundingBox.xMax, epsg_box.east);
          auto y1 = std::max(geographicBoundingBox.yMin, epsg_box.south);
          auto y2 = std::min(geographicBoundingBox.yMax, epsg_box.north);

          // Produce bbox only if there is overlap

          if (x1 < x2 && y1 < y2)
          {
            bool ok =
                (transformation->Transform(1, &x1, &y1) && transformation->Transform(1, &x2, &y2));

            // Produce bbox only if projection succeeds

            if (ok)
            {
              // Acceptable CRS
              layer_crs_list.PushBack(id);

              // Use proper coordinate ordering for the EPSG
              if (target.EPSGTreatsAsLatLong())
              {
                std::swap(x1, y1);
                std::swap(x2, y2);
              }
              layer_bbox["minx"] = x1;
              layer_bbox["miny"] = y1;
              layer_bbox["maxx"] = x2;
              layer_bbox["maxy"] = y2;
              layer_bbox_list.PushBack(layer_bbox);
            }
          }
        }
      }

      layer["crs"] = layer_crs_list;
      layer["bounding_box"] = layer_bbox_list;
    }

    // Layer dimensions

    if (timeDimension)
    {
      CTPP::CDT layer_dimension_list(CTPP::CDT::ARRAY_VAL);
      CTPP::CDT layer_dimension(CTPP::CDT::HASH_VAL);
      layer_dimension["name"] = "time";
      layer_dimension["units"] = "ISO8601";
      layer_dimension["multiple_values"] = 0;
      layer_dimension["nearest_value"] = 0;
      layer_dimension["current"] = (timeDimension->currentValue() ? 1 : 0);

      layer_dimension["value"] = timeDimension->getCapabilities();  // a string
      layer_dimension_list.PushBack(layer_dimension);
      layer["dimension"] = layer_dimension_list;
    }

    // Layer attribution

    if (false)
    {
      CTPP::CDT layer_attribution(CTPP::CDT::HASH_VAL);
      layer_attribution["title"] = "";
      layer_attribution["online_resource"] = "http://www.www.www";
      CTPP::CDT layer_attribution_logo_url(CTPP::CDT::HASH_VAL);
      layer_attribution_logo_url["width"] = 0;
      layer_attribution_logo_url["height"] = 0;
      layer_attribution_logo_url["format"] = "png";
      layer_attribution_logo_url["online_resource"] = "http://www.www.www/logo.png";
      layer_attribution["logo_url"] = layer_attribution_logo_url;
      layer["attribution"] = layer_attribution;
    }

    // Layer authority URL

    if (false)  // NOT IMPLEMENTED
    {
      CTPP::CDT layer_authority_url_list(CTPP::CDT::ARRAY_VAL);

      CTPP::CDT layer_authority_url(CTPP::CDT::HASH_VAL);
      layer_authority_url["name"] = "FMI";
      layer_authority_url["online_resource"] = "http://www.www.www";
      layer_authority_url_list.PushBack(layer_authority_url);

      // Insert more authorities here if desired

      layer["authority_url"] = layer_authority_url_list;
    }

    // Layer identifier

    if (false)  // NOT IMPLEMENTED
    {
      CTPP::CDT layer_identifier_list(CTPP::CDT::ARRAY_VAL);

      CTPP::CDT layer_identifier(CTPP::CDT::HASH_VAL);
      layer_identifier["authority"] = "FMI";
      layer_identifier["value"] = "foo bar";
      layer_identifier_list.PushBack(layer_identifier);

      // Insert more identifiers here if desired

      layer["identifier"] = layer_identifier_list;
    }

    // Layer metadata URL

    if (false)  // NOT IMPLEMENTED
    {
      CTPP::CDT layer_metadata_url_list(CTPP::CDT::ARRAY_VAL);

      CTPP::CDT layer_metadata_url(CTPP::CDT::HASH_VAL);
      layer_metadata_url["type"] = "XML";
      layer_metadata_url["format"] = "XML";
      layer_metadata_url["online_resource"] = "http://www.www.www";
      layer_metadata_url_list.PushBack(layer_metadata_url);

      // Insert more metadata sources here if desired

      layer["metadata_url"] = layer_metadata_url_list;
    }

    // Layer data URL

    if (false)  // NOT IMPLEMENTED
    {
      CTPP::CDT layer_data_url_list(CTPP::CDT::ARRAY_VAL);

      CTPP::CDT layer_data_url(CTPP::CDT::HASH_VAL);
      layer_data_url["type"] = "XML";
      layer_data_url["format"] = "XML";
      layer_data_url["online_resource"] = "http://www.www.www";
      layer_data_url_list.PushBack(layer_data_url);

      // Insert more data sources here if desired

      layer["data_url"] = layer_data_url_list;
    }

    // Layer styles
    if (!itsStyles.empty())
    {
      CTPP::CDT layer_style_list(CTPP::CDT::ARRAY_VAL);
      for (const auto& style : itsStyles)
      {
        layer_style_list.PushBack(style.getCapabilities());
      }
      if (layer_style_list.Size() > 0)
        layer["style"] = layer_style_list;
    }

    // Layer scale denominators

    if (false)  // NOT IMPLEMENTED
    {
      layer["min_scale_denominator"] = 1;
    }

    if (false)  // NOT IMPLEMENTED
    {
      layer["max_scale_denominator"] = 1;
    }

    return layer;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

Json::Value WMSLayer::readJsonFile(const std::string theFileName)
{
  try
  {
    std::string content;
    std::ifstream in(theFileName.c_str());

    if (!in)
      throw Spine::Exception(BCP, "Failed to open '" + theFileName + "' for reading!");

    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    Json::Value json = parse_json_string(content);

    Json::Reader reader;
    bool parsingSuccessful = reader.parse(content, json);
    if (!parsingSuccessful)
    {
      // report to the user the failure
      throw Spine::Exception(BCP, reader.getFormattedErrorMessages());
    }

    return json;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Error in reading json-file" + theFileName);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
