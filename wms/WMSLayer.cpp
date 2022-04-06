#include "WMSLayer.h"
#include "CaseInsensitiveComparator.h"
#include "StyleSelection.h"
#include "TextUtility.h"
#include "WMSConfig.h"
#include "WMSException.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/make_shared.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <engines/gis/Engine.h>
#include <fmt/format.h>
#include <gis/CoordinateTransformation.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>
#include <ogr_spatialref.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
Json::CharReaderBuilder charreaderbuilder;

std::string get_symbol_translation(const LegendGraphicSymbol& symbolSettings,
                                   const std::string& language,
                                   const std::string& defaultValue)
{
  if (symbolSettings.translations.find(language) != symbolSettings.translations.end())
    return symbolSettings.translations.at(language);

  return defaultValue;
}

std::string get_legend_title_string(const LegendGraphicParameter& parameterSettings,
                                    const std::string& language)
{
  // 1) If hide_title == true return empty string
  if (parameterSettings.hide_title)
    return "";

  // 2) Return translation if exists
  if (parameterSettings.translations.find(language) != parameterSettings.translations.end())
	return parameterSettings.translations.at(language);

  // 3) Return name given in parameter settings (wms - or product configuration file)
  if (!parameterSettings.given_name.empty())
    return parameterSettings.given_name;

  // 4) Return data_name by default
  return parameterSettings.data_name;
}

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

void add_attributes(Json::Value& json, const LegendGraphicInfoItem& lgi)
{
  Json::Value attributes = lgi.asJsonValue("attributes");
  if (!attributes.isNull())
    json["attributes"] = attributes;
}

void add_symbol_name(Json::Value& json,
                     const WMSLegendGraphicSettings& lgs,
                     const LegendGraphicInfoItem& lgi,
                     const std::string& language)
{
  std::string name = lgi.asString("lolimit") + "..." + lgi.asString("hilimit");

  if (name.length() == 3)
  {
    std::string symbolName = lgi.asString("symbol");
    if (!symbolName.empty() && lgs.parameters.find(symbolName) != lgs.parameters.end())
    {
      name = lgs.parameters.at(symbolName).given_name;
    }
    else
    {
      std::string translation = symbolName;
      if (lgs.symbols.find(symbolName) != lgs.symbols.end())
        translation = get_symbol_translation(lgs.symbols.at(symbolName), language, symbolName);
      name = translation;
    }
  }

  json = Json::Value(name);
}

Json::Value process_symbol_group_json(const Json::Value& jsonTemplate,
                                      const WMSLegendGraphicSettings& lgs,
                                      const LegendGraphicInfo& legendGraphicInfo,
                                      const std::string& language,
                                      unsigned int xpos,
                                      unsigned int ypos,
                                      unsigned int uniqueId)
{
  Json::Value legendTemplate = jsonTemplate;
  // Take a copy of template, remove layers and add them again with actual values
  Json::Value ret = jsonTemplate;
  ret.removeMember("layers");
  ret["layers"] = Json::Value();
  Json::Value& layers = ret["layers"];

  unsigned int symbol_xpos = xpos + *lgs.layout.legend_xoffset + 10;
  unsigned int symbol_ypos = ypos + *lgs.layout.legend_yoffset;
  unsigned int header_ypos = ypos + *lgs.layout.param_name_yoffset;
  unsigned int header_xpos = xpos + *lgs.layout.param_name_xoffset;

  for (const auto& lgi : legendGraphicInfo)
  {
    std::string symbolName = lgi.asString("symbol");

    // Some symbols need not to be shown in legend graphs, for exmaple logos
    if (lgs.symbolsToIgnore.find(symbolName) != lgs.symbolsToIgnore.end())
      continue;

    // Handling individual symbols
    Json::Value& xPosJson = legendTemplate["x"];
    Json::Value& layersJson = legendTemplate["layers"];
    Json::Value& symbolLayerJson = layersJson[layersJson.size() - 2];
    Json::Value& textLayerJson = layersJson[layersJson.size() - 1];

    bool firstSymbol = (xPosJson.asInt() == 99999);
    // Add header before first symbol
    if (firstSymbol)
    {
      Json::Value& yPosJson = legendTemplate["y"];
      xPosJson = Json::Value(symbol_xpos);
      yPosJson = Json::Value(symbol_ypos);
      Json::Value& headerLayerJson = layersJson[0];
      Json::Value& cdataJson = headerLayerJson["cdata"];
      std::string parameterName = lgi.asString("parameter_name");
      if (lgs.parameters.find(parameterName) != lgs.parameters.end())
        parameterName = get_legend_title_string(lgs.parameters.at(parameterName), language);

      cdataJson = Json::Value(parameterName);
      Json::Value& attributesHeaderJson = headerLayerJson["attributes"];
      Json::Value& xPosHeaderJson = attributesHeaderJson["x"];
      Json::Value& yPosHeaderJson = attributesHeaderJson["y"];
      xPosHeaderJson = Json::Value(header_xpos);
      yPosHeaderJson = Json::Value(header_ypos);

      layers.append(headerLayerJson);
    }
    Json::Value& symbolJson = symbolLayerJson["symbol"];
    symbolJson = Json::Value(lgi.asString("symbol"));
    Json::Value& positionsJson = symbolLayerJson["positions"];
    Json::Value& xPosSymbolJson = positionsJson["x"];
    Json::Value& yPosSymbolJson = positionsJson["y"];
    xPosSymbolJson = Json::Value(symbol_xpos);
    yPosSymbolJson = Json::Value(symbol_ypos - 5);

    Json::Value& cdataTextJson = textLayerJson["cdata"];

    add_attributes(symbolLayerJson, lgi);
    add_symbol_name(cdataTextJson, lgs, lgi, language);

    Json::Value& attributesTextJson = textLayerJson["attributes"];
    Json::Value& xPosSymbolTextJson = attributesTextJson["x"];
    Json::Value& yPosSymbolTextJson = attributesTextJson["y"];
    xPosSymbolTextJson = Json::Value(symbol_xpos + *lgs.layout.symbol_text_xoffset);
    yPosSymbolTextJson = Json::Value(symbol_ypos + *lgs.layout.symbol_text_yoffset);
    layers.append(symbolLayerJson);
    layers.append(textLayerJson);

    symbol_ypos += *lgs.layout.symbol_group_y_padding;
  }

  return ret;
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
    xPosSymbolTextJson = Json::Value(symbol_xpos + *lgs.layout.symbol_text_xoffset);
    yPosSymbolTextJson = Json::Value(symbol_ypos + *lgs.layout.symbol_text_yoffset);
  }

  return legendJson;
}

LegendGraphicInfo get_legend_graphic_info(const Json::Value& layersJson)
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
        if (supportedStyleLayers.find(layerTypeString) == supportedStyleLayers.end())
          continue;

        lgi.add("layer_type", layerTypeJson);

        json = layerJson.get("isobands", nulljson);
        if (!json.isNull())
        {
          lgi.add("isobands", json);
          if (json.isArray())
          {
            for (unsigned int i = 0; i < json.size(); i++)
            {
              Json::Value& isoband = json[i];
              Json::Value label = isoband.get("label", nulljson);
              if (!label.isNull())
              {
                Json::Value::Members members = label.getMemberNames();
                for (unsigned int j = 0; j < members.size(); j++)
                {
                  std::string language = members[j];
                  Json::Value translationJson = label.get(language, nulljson);
                  std::string translation =
                      (!translationJson.isNull() ? translationJson.asString() : "");
                  Dali::text_dimension_t tdim =
                      Dali::getTextDimension(translation, Dali::text_style_t());
                  tdim.width *= 1.6;
                  if (lgi.text_lengths.find(language) == lgi.text_lengths.end())
                    lgi.text_lengths.insert(make_pair(language, 0));
                  if (tdim.width > lgi.text_lengths.at(language))
                    lgi.text_lengths[language] = tdim.width;
                }
              }
            }
          }
        }
        json = layerJson.get("isolines", nulljson);
        if (!json.isNull())
          lgi.add("isolines", json);
        json = layerJson.get("css", nulljson);
        if (!json.isNull())
          lgi.add("css", json);
        // Single symbol
        json = layerJson.get("symbol", nulljson);
        if (!json.isNull())
        {
          lgi.add("symbol", json);
        }
        json = layerJson.get("symbols", nulljson);
        if (!json.isNull())
        {
          // Expand symbol group
          Json::Value symbolGroup = WMSLayer::parseJsonString(json.toStyledString());
          if (symbolGroup.isArray())
          {
            for (unsigned int i = 0; i < symbolGroup.size(); i++)
            {
              LegendGraphicInfoItem lgiSymbol;
              if (!lgi.asJsonValue("parameter_name").isNull())
                lgiSymbol.add("parameter_name", lgi.asJsonValue("parameter_name"));
              if (!lgi.asJsonValue("layer_type").isNull())
                lgiSymbol.add("layer_type", lgi.asJsonValue("layer_type"));
              Json::Value& symbolJson = symbolGroup[i];
              json = symbolJson.get("symbol", nulljson);
              if (!json.isNull())
                lgiSymbol.add("symbol", json);
              json = symbolJson.get("attributes", nulljson);
              if (!json.isNull())
                lgiSymbol.add("attributes", json);
              json = symbolJson.get("lolimit", nulljson);
              if (!json.isNull())
                lgiSymbol.add("lolimit", json);
              json = symbolJson.get("hilimit", nulljson);
              if (!json.isNull())
                lgiSymbol.add("hilimit", json);

              ret.push_back(lgiSymbol);
            }
            continue;
          }
        }
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
      LegendGraphicInfo recursiveRet = get_legend_graphic_info(layersJson);
      ret.insert(ret.end(), recursiveRet.begin(), recursiveRet.end());
    }
  }

  return ret;
}

// Language support
void get_translations(const Json::Value& translationJson,
                      std::map<std::string, std::string>& translations)
{
  if (!translationJson.isNull())
  {
    Json::Value nulljson;
    Json::Value::Members languages = translationJson.getMemberNames();
    for (unsigned int j = 0; j < languages.size(); j++)
    {
      std::string language = languages[j];
      Json::Value translatedJson = translationJson.get(language, nulljson);
      std::string translation = (!translatedJson.isNull() ? translatedJson.asString() : "");
      translations.insert(std::make_pair(language, translation));
    }
  }
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

  auto symbolsJson = legendGraphicJson.get("symbols", nulljson);
  // symbols
  for (unsigned int i = 0; i < symbolsJson.size(); i++)
  {
    auto symbolJson = symbolsJson[i];

    json = symbolJson.get("name", nulljson);
    if (!json.isNull())
    {
      std::string symbol_name = json.asString();
      LegendGraphicSymbol lgs(symbol_name);
      Json::Value translationJson = symbolJson.get("translation", nulljson);
      get_translations(translationJson, lgs.translations);
	  for(const auto& item : lgs.translations)
		settings.languages.insert(item.first);
      if (settings.symbols.find(symbol_name) != settings.symbols.end())
      {
        settings.symbols[symbol_name] = lgs;
      }
      else
      {
        settings.symbols.insert(std::make_pair(symbol_name, lgs));
      }
    }
  }

  auto parametersJson = legendGraphicJson.get("parameters", nulljson);
  // parameters
  for (unsigned int i = 0; i < parametersJson.size(); i++)
  {
    auto parameterJson = parametersJson[i];

    std::string data_name;
    std::string name;
    std::string unit;
    bool hide_title = false;

    json = parameterJson.get("data_name", nulljson);
    if (!json.isNull())
      data_name = json.asString();
    json = parameterJson.get("name", nulljson);
    if (!json.isNull())
      name = json.asString();
    json = parameterJson.get("unit", nulljson);
    if (!json.isNull())
      unit = json.asString();
    json = parameterJson.get("hide_title", nulljson);
    if (!json.isNull())
      hide_title = json.asBool();

    if (!data_name.empty())
    {
      std::vector<std::string> param_names;
      boost::algorithm::split(param_names, data_name, boost::algorithm::is_any_of(","));
      // Language support
      Json::Value translationJson = parameterJson.get("translation", nulljson);
      std::map<std::string, std::string> translations;
      get_translations(translationJson, translations);
	  for(const auto& item : translations)
		settings.languages.insert(item.first);
      for (const auto& param_name : param_names)
      {
        settings.parameters.insert(
            std::make_pair(param_name, LegendGraphicParameter(param_name, name, unit, hide_title)));
        if (translations.size() > 0)
          settings.parameters[param_name].translations = translations;
        for (const auto& item : translations)
        {
          Dali::text_dimension_t tdim = Dali::getTextDimension(item.second, Dali::text_style_t());
          settings.parameters[param_name].text_lengths[item.first] = (tdim.width * 1.6);
        }
      }
    }
  }

  // layout
  auto layoutJson = legendGraphicJson.get("layout", nulljson);

  if (layoutJson.isNull())
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
  json = layoutJson.get("symbol_text_xoffset", nulljson);
  if (!json.isNull())
    settings.layout.symbol_text_xoffset = json.asInt();
  json = layoutJson.get("symbol_text_yoffset", nulljson);
  if (!json.isNull())
    settings.layout.symbol_text_yoffset = json.asInt();
  json = layoutJson.get("legend_xoffset", nulljson);
  if (!json.isNull())
    settings.layout.legend_xoffset = json.asInt();
  json = layoutJson.get("legend_yoffset", nulljson);
  if (!json.isNull())
    settings.layout.legend_yoffset = json.asInt();
  json = layoutJson.get("legend_width", nulljson);
  if (!json.isNull())
    settings.layout.legend_width = json.asInt();

  json = layoutJson.get("legend_width_per_language", nulljson);
  if (!json.isNull())
  {
    Json::Value::Members languages = json.getMemberNames();
    for (unsigned int j = 0; j < languages.size(); j++)
    {
      std::string language = languages[j];
      Json::Value widthJson = json.get(language, nulljson);
      unsigned int l_width =
          (!widthJson.isNull() ? widthJson.asInt() : *(settings.layout.legend_width));
      settings.layout.legend_width_per_language.insert(std::make_pair(language, l_width));
    }
  }
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
    throw Fmi::Exception(BCP,
                         "'fixed_width' attribute must be integer: " + widthJson.toStyledString());
  }

  return ret;
}  // namespace

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
      for (const auto& layer : layersJson)
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

std::string get_online_resource_string(const std::string& styleName, const std::string& layerName)
{
  std::string ret =
      "__hostname____apikey__/"
      "wms?service=WMS&amp;request=GetLegendGraphic&amp;version=1.3.0&amp;sld_version=1.1.0&amp;"
      "style=";
  ret += Spine::HTTP::urlencode(styleName);
  ret += +"&amp;format=image%2Fpng&amp;layer=";
  ret += Spine::HTTP::urlencode(layerName);

  return ret;
}

std::map<std::string, WMSLayerStyle> get_styles(const Json::Value& root,
                                                const std::string& layerName,
                                                std::map<std::string, std::string>& legendFiles)
{
  std::map<std::string, WMSLayerStyle> ret;
  Json::Value nulljson;

  // 1) Read "legend_url_layer"-parameter and use it to generate styles entry
  Json::Value json = root.get("legend_url_layer", nulljson);
  if (!json.isNull())
  {
    WMSLayerStyle layerStyle;
    std::string layerName = json.asString();
    layerStyle.legend_url.online_resource = get_online_resource_string(layerStyle.name, layerName);
    legendFiles.insert(make_pair(layerStyle.name, layerName));
    ret.insert(std::make_pair(layerStyle.name, layerStyle));
  }

  if (ret.empty())
  {
    bool addDefaultStyle = true;
    // 2) Read styles vector from configuration and generate layer styles
    json = root.get("styles", nulljson);
    if (!json.isNull())
    {
      if (!json.isArray())
        throw Fmi::Exception(BCP, "WMSLayer styles settings must be an array");

      WMSLayerStyle layerStyle;

      for (unsigned int i = 0; i < json.size(); i++)
      {
        const Json::Value& style_json = json[i];
        auto json_value = style_json.get("name", nulljson);
        if (!json_value.isNull())
		  layerStyle.name = json_value.asString();
        json_value = style_json.get("title", nulljson);
        if (!json_value.isNull())
          layerStyle.title = json_value.asString();
        json_value = style_json.get("abstract", nulljson);
        if (!json_value.isNull())
		  layerStyle.abstract = json_value.asString();

        auto legend_url_json = style_json.get("legend_url", nulljson);
        if (!legend_url_json.isNull())
        {
          json_value = legend_url_json.get("format", nulljson);
          if (!json_value.isNull())
            layerStyle.legend_url.format = json_value.asString();
          json_value = legend_url_json.get("online_resource", nulljson);
          if (!json_value.isNull())
            layerStyle.legend_url.online_resource = json_value.asString();
          ret.insert(std::make_pair(layerStyle.name, layerStyle));
        }
        else
        {
		  auto legend_url_layer_json = style_json.get("legend_url_layer", nulljson);
		  if (!legend_url_layer_json.isNull())
			{
			  std::string legendLayerName = legend_url_layer_json.asString();
			  layerStyle.legend_url.online_resource = get_online_resource_string("default", legendLayerName);
			  legendFiles.insert(make_pair(layerStyle.name, legendLayerName));
			  ret.insert(std::make_pair(layerStyle.name, layerStyle));
			}
		  else
			{
			  layerStyle.legend_url.online_resource =
				get_online_resource_string(layerStyle.name, layerName);
			  ret.insert(std::make_pair(layerStyle.name, layerStyle));
			}
          CaseInsensitiveComparator cicomp;
          if (cicomp(layerStyle.name, "default"))
            addDefaultStyle = false;
        }
      }
      if (addDefaultStyle)
      {
        WMSLayerStyle layerStyle;
        layerStyle.legend_url.online_resource =
            get_online_resource_string(layerStyle.name, layerName);
        ret.insert(std::make_pair(layerStyle.name, layerStyle));
      }
    }
  }

  // 3) Generate layer styles automatically from configuration
  if (ret.empty())
  {
    WMSLayerStyle layerStyle;
    layerStyle.legend_url.online_resource = get_online_resource_string(layerStyle.name, layerName);
    ret.insert(std::make_pair(layerStyle.name, layerStyle));
  }

  return ret;
}

std::map<std::string, Json::Value> readLegendDirectory(const std::string& legendDirectory,
                                                       const Spine::JsonCache& cache)
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
        std::string name = Fmi::ascii_tolower_copy(itr->path().stem().string());
        Json::Value json = cache.get(itr->path());
        ret.insert(std::make_pair(name, json));
      }
    }
  }

  return ret;
}

std::map<std::string, Json::Value> readLegendFiles(const std::string& wmsroot,
                                                   const Spine::JsonCache& cache,
                                                   const std::string& customer)
{
  // First read common templates from wmsroot/legends/templates
  std::map<std::string, Json::Value> legend_templates =
      readLegendDirectory(wmsroot + "/legends/templates", cache);
  // Then read customer specific templates from wmsroot/customer/legends/templates
  std::map<std::string, Json::Value> customer_specific =
      readLegendDirectory(wmsroot + "/customers/" + customer + "/legends/templates", cache);
  // Merge and replace common templates with customer specific templates
  for (const auto& t : customer_specific)
	{
	  std::string  name = Fmi::ascii_tolower_copy(t.first);
	  if (legend_templates.find(name) == legend_templates.end())
		legend_templates.insert(std::make_pair(name, t.second));
	  else
		legend_templates[name] = t.second;
	}

  return legend_templates;
}

}  // namespace

// Remember to run updateLayerMetaData before using the layer!
WMSLayer::WMSLayer(const WMSConfig& config)
    : queryable(true),
      wmsConfig(config),
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

void WMSLayer::initLegendGraphicInfo(const Json::Value& root)
{
  Json::Value nulljson;

  std::set<std::string> languages;
  languages.insert(wmsConfig.getDaliConfig().defaultLanguage());

  // Read width, height from product file
  auto json = root.get("width", nulljson);
  if (!json.isNull())
    width = json.asInt();
  json = root.get("height", nulljson);
  if (!json.isNull())
    height = json.asInt();

  // 1) Default settings
  WMSLegendGraphicSettings actualSettings(true); // Deafult values
  // 2) Settings from wms.conf
  WMSLegendGraphicSettings configSettings = wmsConfig.getLegendGraphicSettings();
  actualSettings.merge(configSettings);
  // 3) Settings from layer file (product file)
  WMSLegendGraphicSettings layerSettings;
  get_legend_graphic_settings(root, layerSettings);
  languages.insert(layerSettings.languages.begin(), layerSettings.languages.end());

  // 4) Read generic and parameter specifig template files and merge them
  std::map<std::string, Json::Value> templateFiles = readLegendFiles(
      wmsConfig.getDaliConfig().rootDirectory(true), wmsConfig.getJsonCache(), customer);
  std::map<std::string, WMSLegendGraphicSettings> templateSettings;

  for(auto item : templateFiles)
	{
	  WMSLegendGraphicSettings settings;
	  get_legend_graphic_settings(item.second, settings);
	  templateSettings.insert(std::make_pair(item.first, settings));
	  if(settings.languages.size() > 0)
		languages.insert(settings.languages.begin(), settings.languages.end());
	}

  // Get layer styles
  itsStyles = get_styles(root, name, itsLegendFiles);

  // If external legend file used, no need to continue
  // width, height is set later
  if (itsLegendFiles.find("default") != itsLegendFiles.end())
	return;

  NamedLegendGraphicInfo named_lgi;

  //  std::map<std::string, WMSLayerStyle*> new_styles;
  for (const auto& item : itsStyles)
  {
    std::string styleName = item.first;

	// If external legend file used, no need to continue
	// width, height is set later
	if (itsLegendFiles.find(styleName) != itsLegendFiles.end())
	  continue;

    std::string legendGraphicInfoName = name + "::" + styleName;

    if (named_lgi.find(legendGraphicInfoName) == named_lgi.end())
      named_lgi.insert(make_pair(legendGraphicInfoName, LegendGraphicInfo()));

    // Read layer file and gather info needed for legends
    LegendGraphicInfo& legendGraphicInfo = named_lgi[legendGraphicInfoName];
    // Set right style add getCapabilities Style info
    Json::Value modifiedLayer = root;
    useStyle(modifiedLayer, styleName);
    auto viewsJson = modifiedLayer.get("views", nulljson);
    if (!viewsJson.isNull() && viewsJson.isArray())
    {
      for (unsigned int i = 0; i < viewsJson.size(); i++)
      {
        auto viewJson = viewsJson[i];
        auto layersJson = viewJson.get("layers", nulljson);
        if (!layersJson.isNull() && layersJson.isArray())
        {
          LegendGraphicInfo lgi = get_legend_graphic_info(layersJson);
          if (lgi.size() > 0)
            legendGraphicInfo.insert(legendGraphicInfo.end(), lgi.begin(), lgi.end());
        }
      }
    }
  }

#ifdef LATER
  for (const auto& item : original_styles)
  {
    if (new_styles.find(item.first) == new_styles.end())
    {
      std::cout << "ERROR1: style: " << name << " -> " << item.first << " NOT found from new styles"
                << std::endl;
      continue;
    }
    const WMSLayerStyle* original_style = item.second;
    const WMSLayerStyle* new_style = new_styles.at(item.first);
    if (original_style->name != new_style->name || original_style->title != new_style->title ||
        original_style->abstract != new_style->abstract)
    {
      std::cout << "ERROR2: name,title,abstract: " << name << " -> " << original_style->name
                << " AND " << new_style->name << ", " << original_style->title << " AND "
                << new_style->title << ", " << original_style->abstract << " AND "
                << new_style->abstract << std::endl;
      continue;
    }
    if (original_style->legend_url.format != new_style->legend_url.format)
    {
      std::cout << "ERROR3: legend_url.format: " << name << " -> "
                << original_style->legend_url.format << " != " << new_style->legend_url.format
                << std::endl;
      continue;
    }
    if (original_style->legend_url.online_resource != new_style->legend_url.online_resource)
    {
      std::cout << "ERROR4: legend_url.online_resource: " << name << " -> "
                << original_style->legend_url.online_resource
                << " != " << new_style->legend_url.online_resource << std::endl;
      continue;
    }
    std::cout << "OK style: " << name << " -> " << item.first << std::endl;
  }
#endif
  
  unsigned int uniqueId = 0;
  for (const auto language : languages)
  {
    for (const auto& item : named_lgi)
    {
      std::set<std::string> parameterNames;
      unsigned int xpos = 0;
      unsigned int ypos = 0;
      const auto& legendGraphicInfo = item.second;

      LegendGraphicResultPerLanguage& languageResult = itsLegendGraphicResults[item.first];
      LegendGraphicResult& result = languageResult[language];

      // Vector of symbol groups for PrecipitationForm, CloudBase2, ...
      NamedLegendGraphicInfo lgiSymbols;
      for (const auto& lgi : legendGraphicInfo)
      {
        std::string layerType = lgi.asString("layer_type");
        std::string layerSubType = lgi.asString("layer_subtype");
        std::string parameterName = lgi.asString("parameter_name");
		
        parameterNames.insert(parameterName);

        if (layerType == "icemap" && layerSubType == "symbol")
          layerType = "symbol";

        // Handle all symbols together in the end
        if (layerType == "symbol")
        {
          lgiSymbols[parameterName].push_back(lgi);
          continue;
        }

        bool isGenericTemplate = false;
        // Legends are identified by parameter name + layer type (e.g. Temperature_isoline,
        // Temperature_isoband), or by plain layer type (e.g. isoline, isoband)
        std::string key = Fmi::ascii_tolower_copy(parameterName + "_" + layerType);
        if (parameterName.empty() || templateFiles.find(key) == templateFiles.end())
		  {
			key = Fmi::ascii_tolower_copy(layerType);		
			if (templateFiles.find(key) != templateFiles.end())
			  {
				isGenericTemplate = true;
			  }
			else
			  {
				key.clear();
			  }
		  }
		if(key.empty())
		  continue;

		// 3) Settings from template file will overwrite the previous settings (default settings, wms.conf)
        const auto& templateJson = templateFiles.at(key);
		actualSettings.merge(templateSettings.at(key));

		// 4) Settings from layer file (product file) will overwrite the previous settings (default settings, wms.conf, template file)
		actualSettings.merge(layerSettings);

		LegendGraphicParameter lgp(parameterName);
        const LegendGraphicParameter& parameterSettings =
            (actualSettings.parameters.find(parameterName) != actualSettings.parameters.end()
                 ? actualSettings.parameters.at(parameterName)
                 : lgp);

        std::string legendHeader = get_legend_title_string(parameterSettings, language);

        // Generic template file is the same for all layers of same type (e.g. all isobands),
        // non-generic is parameter-specific and only location need to be modified here
        auto legendJson = process_legend_json(templateJson,
                                              layerType,
                                              parameterName,
                                              isGenericTemplate,
                                              legendHeader,
                                              parameterSettings.unit,
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
          unsigned int localizedWidth = lgi.labelWidth(language);
          if (actualSettings.layout.legend_xoffset)
            localizedWidth += *(actualSettings.layout.legend_xoffset);

          if (localizedWidth > isobandWidth)
            isobandWidth = localizedWidth;
        }
        else if (layerType == "isoline")
        {
          labelHeight = ypos + (*actualSettings.layout.legend_yoffset * 2);
        }

        if (labelHeight > result.height)
        {
          result.height = labelHeight;
        }

        result.legendLayers.push_back(legendJson.toStyledString());

        xpos += (*(actualSettings.layout.legend_width) > isobandWidth
                     ? *(actualSettings.layout.legend_width)
                     : isobandWidth);
      }

      result.width = xpos;

      // Finally all symbol groups are handled
      if (lgiSymbols.size() > 0)
      {
        for (std::map<std::string, LegendGraphicInfo>::const_iterator iter = lgiSymbols.begin();
             iter != lgiSymbols.end();
             ++iter)
        {
		  std::string templateName = Fmi::ascii_tolower_copy(iter->first + "_symbol");
		  if (iter->first.empty() || templateFiles.find(templateName) == templateFiles.end())
			templateName = "symbol";

		  WMSLegendGraphicSettings actualSymbolSettings = actualSettings;
		  // 3) Settings from template file will overwrite the previous settings (default settings, wms.conf)
		  actualSymbolSettings.merge(templateSettings.at(templateName));
		  // 4) Settings from layer file (product file) will overwrite the previous settings (default settings, wms.conf, template file)
		  actualSymbolSettings.merge(layerSettings);			  

          Json::Value symbolTemplate = templateFiles.at(templateName);
		  const LegendGraphicInfo& legendGraphicInfo = iter->second;
		  
		  symbolTemplate = process_symbol_group_json(symbolTemplate,
													 actualSymbolSettings,
													 legendGraphicInfo,
													 language,
													 xpos,
													 ypos,
													 uniqueId);

          result.legendLayers.push_back(symbolTemplate.toStyledString());
          Json::Value nulljson;
          auto layersJson = symbolTemplate.get("layers", nulljson);
          if (!layersJson.isNull() && layersJson.isArray())
          {
            for (const auto& layer : layersJson)
            {
              get_legend_dimension(layer, "attributes", result.width, result.height);
              get_legend_dimension(layer, "positions", result.width, result.height);
            }
          }
          result.width += *actualSymbolSettings.layout.symbol_group_x_padding;
          result.height += *actualSettings.layout.symbol_group_y_padding;
        }
      }



      for (const auto& name : parameterNames)
      {
        Dali::text_dimension_t tdim = Dali::getTextDimension(name, Dali::text_style_t());
        unsigned int data_name_width = (tdim.width * 1.6);
        unsigned int parameter_name_width = data_name_width;

        if (actualSettings.parameters.find(name) != actualSettings.parameters.end())
        {
          const auto& lgp = actualSettings.parameters.at(name);
          tdim = Dali::getTextDimension(lgp.given_name, Dali::text_style_t());
          unsigned int given_name_width = (tdim.width * 1.6);
          parameter_name_width = std::max(data_name_width, given_name_width);
          // Translation of parameter overrides original parameter name
          if (lgp.text_lengths.find(language) != lgp.text_lengths.end())
            parameter_name_width = lgp.text_lengths.at(language);
        }
        if (actualSettings.layout.legend_xoffset)
          parameter_name_width += *actualSettings.layout.legend_xoffset;
        result.width = std::max(parameter_name_width, result.width);
      }
    }
  }

  // Set width, height in GetLegendGraphic URL
  for (const auto& item : itsLegendGraphicResults)
  {
    const LegendGraphicResultPerLanguage& languageResult = itsLegendGraphicResults[item.first];
    for (const auto& item2 : item.second)
    {
      if (item2.first == wmsConfig.getDaliConfig().defaultLanguage())
      {
        const LegendGraphicResult& result =
            languageResult.at(wmsConfig.getDaliConfig().defaultLanguage());

        std::string styleName = item.first.substr(item.first.rfind("::") + 2);
        if (itsStyles.find(styleName) != itsStyles.end())
        {
          // Width, height must be > 0
          if (result.width == 0 || result.height == 0)
          {
            itsStyles.erase(styleName);
            continue;
          }
          itsStyles[styleName].legend_url.online_resource +=
              ("&amp;width=" + Fmi::to_string(result.width) +
               "&amp;height=" + Fmi::to_string(result.height));
          itsStyles[styleName].legend_url.width = Fmi::to_string(result.width);
          itsStyles[styleName].legend_url.height = Fmi::to_string(result.height);
        }
      }
    }
  }
}

LegendGraphicResult WMSLayer::getLegendGraphic(const std::string& legendGraphicID,
                                               const std::string& language) const
{
  if (itsLegendGraphicResults.find(legendGraphicID) != itsLegendGraphicResults.end())
  {
    const auto& layerLGR = itsLegendGraphicResults.at(legendGraphicID);
    if (layerLGR.find(language) != layerLGR.end())
	  return layerLGR.at(language);
    else
	  return layerLGR.at(wmsConfig.getDaliConfig().defaultLanguage());
  }

  return LegendGraphicResult();
}

bool WMSLayer::isValidCRS(const std::string& theCRS) const
{
  try
  {
    // Disabled explicitly?
    if (disabled_refs.find(theCRS) != disabled_refs.end())
      return false;

    // Disabled if not in the known list at all
    auto ref = refs.find(theCRS);
    if (ref == refs.end())
      return false;

    // Known, and enabled explicitly?
    if (enabled_refs.find(theCRS) != enabled_refs.end())
      return true;

    // Enabled by default?
    return ref->second.enabled;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Validating CRS failed!");
  }
}

bool WMSLayer::isValidStyle(const std::string& theStyle) const
{
  try
  {
    return (itsStyles.find(theStyle) != itsStyles.end());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Validating style failed!");
  }
}

bool WMSLayer::isValidElevation(int theElevation) const
{
  try
  {
    if (elevationDimension)
      return elevationDimension->isValidElevation(theElevation);
    else
      return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Validating time failed!");
  }
}

bool WMSLayer::isValidReferenceTime(const boost::posix_time::ptime& theReferenceTime) const
{
  try
  {
    if (timeDimensions)
      return timeDimensions->isValidReferenceTime(theReferenceTime);
    else
      return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Validating reference time failed!");
  }
}

bool WMSLayer::isValidTime(const boost::posix_time::ptime& theTime,
                           const boost::optional<boost::posix_time::ptime>& theReferenceTime) const
{
  try
  {
    if (timeDimensions)
      return timeDimensions->isValidTime(theTime, theReferenceTime);
    else
      return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Validating time failed!");
  }
}

bool WMSLayer::currentValue() const
{
  try
  {
    // if time dimension is not present, time-option is irrelevant
    return (timeDimensions ? timeDimensions->currentValue() : true);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Validating current time failed!");
  }
}

boost::posix_time::ptime WMSLayer::mostCurrentTime(
    const boost::optional<boost::posix_time::ptime>& reference_time) const
{
  try
  {
    return (timeDimensions ? timeDimensions->mostCurrentTime(reference_time)
                           : boost::posix_time::not_a_date_time);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Validating most current time!");
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
    throw Fmi::Exception::Trace(BCP, "WMS info operation failed!");
  }
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfoItem& lgi)
{
  Json::StyledWriter writer;
  for (const auto& elem : lgi.info)
  {
    std::cout << elem.first << ":\n" << writer.write(elem.second) << std::endl;
  }

  return ost;
}

std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfo& lgi)
{
  Json::StyledWriter writer;
  for (const auto& elem : lgi)
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
    for (const auto& c : layer.refs)
      ost << c.first << "=" << c.second.proj << " ";
    ost << "styles:";
    for (const auto& style : layer.itsStyles)
      ost << style.first << " ";
    if (!layer.customer.empty())
      ost << " \ncustomer: " << layer.customer << "\n";

    if (layer.timeDimensions)
      ost << "timeDimensions: " << layer.timeDimensions;
    else
      ost << "timeDimensions: -";

    ost << "*******************************" << std::endl;

    return ost;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Printing the request failed!");
  }
}

void WMSLayer::initProjectedBBoxes()
{
  for (const auto& id_ref : refs)
  {
    const auto& id = id_ref.first;
    const auto& ref = id_ref.second;

    if (!isValidCRS(id))
      continue;

    if (ref.geographic)
      continue;

    // Intersect with target EPSG bounding box (latlon) if it is available

    auto x1 = geographicBoundingBox.xMin;
    auto x2 = geographicBoundingBox.xMax;
    auto y1 = geographicBoundingBox.yMin;
    auto y2 = geographicBoundingBox.yMax;

    auto& epsg_box = ref.bbox;
    x1 = std::max(x1, epsg_box.west);
    x2 = std::min(x2, epsg_box.east);
    y1 = std::max(y1, epsg_box.south);
    y2 = std::min(y2, epsg_box.north);

    // Produce bbox only if there is overlap

    if (x1 < x2 && y1 < y2)
    {
      Fmi::SpatialReference wgs84("WGS84");
      Fmi::SpatialReference target(ref.proj);
      Fmi::CoordinateTransformation transformation(wgs84, target);

      bool ok = (transformation.transform(x1, y1) && transformation.transform(x2, y2));

      // Produce bbox only if projection succeeds

      if (ok)
      {
        // Use proper coordinate ordering for the EPSG

        if (target.EPSGTreatsAsLatLong())
        {
          std::swap(x1, y1);
          std::swap(x2, y2);
        }
        Engine::Gis::BBox bbox(x1, x2, y1, y2);
        projected_bbox.insert(std::make_pair(id, std::move(bbox)));
      }
    }
  }
}

boost::optional<CTPP::CDT> WMSLayer::getLayerBaseInfo() const
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
      CTPP::CDT kwords(CTPP::CDT::ARRAY_VAL);
      for (const auto& kword : *keywords)
        kwords.PushBack(kword);
      layer["keyword"] = kwords;
    }

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

boost::optional<CTPP::CDT> WMSLayer::getGeographicBoundingBoxInfo() const
{
  try
  {
    if (hidden)
      return {};

    // Layer geographic bounding boxes
    CTPP::CDT layer(CTPP::CDT::HASH_VAL);

    CTPP::CDT layer_bbox(CTPP::CDT::HASH_VAL);
    layer_bbox["west_bound_longitude"] = clamp_longitude(geographicBoundingBox.xMin);
    layer_bbox["east_bound_longitude"] = clamp_longitude(geographicBoundingBox.xMax);
    layer_bbox["south_bound_latitude"] = clamp_latitude(geographicBoundingBox.yMin);
    layer_bbox["north_bound_latitude"] = clamp_latitude(geographicBoundingBox.yMax);
    layer["ex_geographic_bounding_box"] = layer_bbox;

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

boost::optional<CTPP::CDT> WMSLayer::getProjectedBoundingBoxInfo() const
{
  try
  {
    if (hidden)
      return {};

    CTPP::CDT layer(CTPP::CDT::HASH_VAL);

    // Layer CRS list and their bounding boxes
    if (!refs.empty())
    {
      CTPP::CDT layer_crs_list(CTPP::CDT::ARRAY_VAL);
      CTPP::CDT layer_bbox_list(CTPP::CDT::ARRAY_VAL);

      for (const auto& id_ref : refs)
      {
        const auto& id = id_ref.first;
        const auto& ref = id_ref.second;

        if (!isValidCRS(id))
          continue;

        CTPP::CDT layer_bbox(CTPP::CDT::HASH_VAL);

        layer_bbox["crs"] = id;

        if (ref.geographic)
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
          auto pos = projected_bbox.find(id);

          if (pos != projected_bbox.end())
          {
            // Acceptable CRS
            layer_crs_list.PushBack(id);

            layer_bbox["minx"] = pos->second.west;
            layer_bbox["miny"] = pos->second.south;
            layer_bbox["maxx"] = pos->second.east;
            layer_bbox["maxy"] = pos->second.north;
            layer_bbox_list.PushBack(layer_bbox);
          }
        }
      }

      if (layer_crs_list.Size() > 0)
      {
        layer["crs"] = layer_crs_list;
        layer["bounding_box"] = layer_bbox_list;
      }
    }
    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

boost::optional<CTPP::CDT> WMSLayer::getTimeDimensionInfo(
    bool multiple_intervals,
    const boost::optional<std::string>& starttime,
    const boost::optional<std::string>& endtime,
    const boost::optional<std::string>& reference_time) const
{
  try
  {
    if (hidden)
      return {};

    CTPP::CDT layer(CTPP::CDT::HASH_VAL);

    CTPP::CDT layer_dimension_list(CTPP::CDT::ARRAY_VAL);
    if (timeDimensions)
    {
      if (reference_time)
      {
        boost::posix_time::ptime ref_time = Fmi::TimeParser::parse(*reference_time);
        // Show time dimension of reference time

        if (timeDimensions->origintimeOK(ref_time))
        {
          const WMSTimeDimension& td = timeDimensions->getTimeDimension(ref_time);
          auto dim_string = td.getCapabilities(multiple_intervals, starttime, endtime);
          if (dim_string.empty())
            return {};

          CTPP::CDT layer_dimension(CTPP::CDT::HASH_VAL);
          layer_dimension["name"] = "time";
          layer_dimension["units"] = "ISO8601";
          layer_dimension["multiple_values"] = 0;
          layer_dimension["nearest_value"] = 0;
          layer_dimension["current"] = (td.currentValue() ? 1 : 0);
          layer_dimension["value"] = dim_string;
          CTPP::CDT reference_time_dimension(CTPP::CDT::HASH_VAL);
          reference_time_dimension["name"] = "reference_time";
          reference_time_dimension["units"] = "ISO8601";
          reference_time_dimension["multiple_values"] = 0;
          reference_time_dimension["nearest_value"] = 0;
          reference_time_dimension["value"] = (Fmi::to_iso_extended_string(ref_time) + "Z");
          layer_dimension_list.PushBack(layer_dimension);
          layer_dimension_list.PushBack(reference_time_dimension);
        }
      }
      else
      {
        const WMSTimeDimension& td = timeDimensions->getDefaultTimeDimension();
        auto dim_string = td.getCapabilities(multiple_intervals, starttime, endtime);
        if (dim_string.empty())
          return {};

        CTPP::CDT layer_dimension(CTPP::CDT::HASH_VAL);
        layer_dimension["name"] = "time";
        layer_dimension["units"] = "ISO8601";
        layer_dimension["multiple_values"] = 0;
        layer_dimension["nearest_value"] = 0;
        layer_dimension["current"] = (td.currentValue() ? 1 : 0);
        layer_dimension["value"] = dim_string;
        CTPP::CDT reference_time_dimension(CTPP::CDT::HASH_VAL);
        const std::vector<boost::posix_time::ptime>& origintimes = timeDimensions->getOrigintimes();
        bool showOrigintimes = !origintimes.empty() && !origintimes.front().is_not_a_date_time();
        if (showOrigintimes)
        {
          StepTimeDimension orgintimesDimension(timeDimensions->getOrigintimes());
          reference_time_dimension["name"] = "reference_time";
          reference_time_dimension["units"] = "ISO8601";
          reference_time_dimension["multiple_values"] = 0;
          reference_time_dimension["nearest_value"] = 0;
          reference_time_dimension["current"] = 1;
          reference_time_dimension["default"] =
              (Fmi::to_iso_extended_string(orgintimesDimension.mostCurrentTime()) + "Z");
          boost::optional<std::string> t;
          reference_time_dimension["value"] = orgintimesDimension.getCapabilities(false, t, t);
        }
        layer_dimension_list.PushBack(layer_dimension);
        if (showOrigintimes)
          layer_dimension_list.PushBack(reference_time_dimension);
      }
    }

    if (layer_dimension_list.Size() > 0)
      layer["time_dimension"] = layer_dimension_list;

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

boost::optional<CTPP::CDT> WMSLayer::getReferenceDimensionInfo() const
{
  try
  {
    if (hidden)
      return {};

    CTPP::CDT layer(CTPP::CDT::HASH_VAL);

    CTPP::CDT layer_dimension_list(CTPP::CDT::ARRAY_VAL);
    if (timeDimensions)
    {
      CTPP::CDT reference_time_dimension(CTPP::CDT::HASH_VAL);
      const std::vector<boost::posix_time::ptime>& origintimes = timeDimensions->getOrigintimes();
      bool showOrigintimes = !origintimes.empty() && !origintimes.front().is_not_a_date_time();
      if (showOrigintimes)
      {
        StepTimeDimension orgintimesDimension(timeDimensions->getOrigintimes());
        reference_time_dimension["name"] = "reference_time";
        reference_time_dimension["units"] = "ISO8601";
        reference_time_dimension["multiple_values"] = 0;
        reference_time_dimension["nearest_value"] = 0;
        reference_time_dimension["current"] = 1;
        reference_time_dimension["default"] =
            (Fmi::to_iso_extended_string(orgintimesDimension.mostCurrentTime()) + "Z");
        boost::optional<std::string> t;
        reference_time_dimension["value"] = orgintimesDimension.getCapabilities(false, t, t);
      }
      if (showOrigintimes)
        layer_dimension_list.PushBack(reference_time_dimension);
    }

    if (layer_dimension_list.Size() > 0)
      layer["time_dimension"] = layer_dimension_list;

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

boost::optional<CTPP::CDT> WMSLayer::getElevationDimensionInfo() const
{
  try
  {
    if (hidden)
      return {};

    CTPP::CDT layer(CTPP::CDT::HASH_VAL);
    CTPP::CDT layer_dimension_list(CTPP::CDT::ARRAY_VAL);

    if (elevationDimension && elevationDimension->isOK())
    {
      CTPP::CDT layer_dimension(CTPP::CDT::HASH_VAL);
      layer_dimension["name"] = "elevation";
      layer_dimension["units"] = elevationDimension->getLevelName();
      layer_dimension["unit_symbol"] = elevationDimension->getUnitSymbol();
      layer_dimension["multiple_values"] = 0;
      layer_dimension["nearest_value"] = 0;
      layer_dimension["default"] = elevationDimension->getDefaultElevation();
      layer_dimension["value"] = elevationDimension->getCapabilities();
      layer_dimension_list.PushBack(layer_dimension);
    }

    if (layer_dimension_list.Size() > 0)
      layer["elevation_dimension"] = layer_dimension_list;

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

boost::optional<CTPP::CDT> WMSLayer::getStyleInfo() const
{
  try
  {
    if (hidden)
      return {};

    CTPP::CDT layer(CTPP::CDT::HASH_VAL);

    // Layer styles
    if (!itsStyles.empty())
    {
      CTPP::CDT layer_style_list(CTPP::CDT::ARRAY_VAL);
      for (const auto& style : itsStyles)
      {
        layer_style_list.PushBack(style.second.getCapabilities());
      }
      if (layer_style_list.Size() > 0)
        layer["style"] = layer_style_list;
    }

    return layer;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

const boost::shared_ptr<WMSTimeDimensions>& WMSLayer::getTimeDimensions() const
{
  return timeDimensions;
}

boost::optional<CTPP::CDT> WMSLayer::generateGetCapabilities(
    bool multiple_intervals,
    const Engine::Gis::Engine& gisengine,
    const boost::optional<std::string>& starttime,
    const boost::optional<std::string>& endtime,
    const boost::optional<std::string>& reference_time)
{
  try
  {
    if (hidden)
      return {};

    CTPP::CDT layer(CTPP::CDT::HASH_VAL);

    // Layer dimensions handled first in case there is nothing available

    if (timeDimensions)
    {
      const WMSTimeDimension& td = timeDimensions->getDefaultTimeDimension();

      auto dim_string = td.getCapabilities(multiple_intervals, starttime, endtime);
      if (dim_string.empty())
        return {};

      CTPP::CDT layer_dimension_list(CTPP::CDT::ARRAY_VAL);
      CTPP::CDT layer_dimension(CTPP::CDT::HASH_VAL);

      layer_dimension["name"] = "time";
      layer_dimension["units"] = "ISO8601";
      layer_dimension["multiple_values"] = 0;
      layer_dimension["nearest_value"] = 0;
      layer_dimension["current"] = (td.currentValue() ? 1 : 0);
      layer_dimension["value"] = dim_string;

      layer_dimension_list.PushBack(layer_dimension);
      layer["time_dimension"] = layer_dimension_list;
    }

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

    if (!refs.empty())
    {
      CTPP::CDT layer_crs_list(CTPP::CDT::ARRAY_VAL);
      CTPP::CDT layer_bbox_list(CTPP::CDT::ARRAY_VAL);

      for (const auto& id_ref : refs)
      {
        const auto& id = id_ref.first;
        const auto& ref = id_ref.second;

        if (!isValidCRS(id))
          continue;

        CTPP::CDT layer_bbox(CTPP::CDT::HASH_VAL);

        layer_bbox["crs"] = id;

        if (ref.geographic)
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
          auto pos = projected_bbox.find(id);

          if (pos != projected_bbox.end())
          {
            // Acceptable CRS
            layer_crs_list.PushBack(id);

            layer_bbox["minx"] = pos->second.west;
            layer_bbox["miny"] = pos->second.south;
            layer_bbox["maxx"] = pos->second.east;
            layer_bbox["maxy"] = pos->second.north;
            layer_bbox_list.PushBack(layer_bbox);
          }
        }
      }

      if (layer_crs_list.Size() > 0)
      {
        layer["crs"] = layer_crs_list;
        layer["bounding_box"] = layer_bbox_list;
      }
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
      for (const auto& item : itsStyles)
      {
        layer_style_list.PushBack(item.second.getCapabilities());
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
    throw Fmi::Exception::Trace(BCP, "Failed to generate capabilities for the layer!");
  }
}

Json::Value WMSLayer::parseJsonString(const std::string theJsonString)
{
  Json::Value json;

  std::unique_ptr<Json::CharReader> reader(charreaderbuilder.newCharReader());
  std::string errors;
  if (!reader->parse(
          theJsonString.c_str(), theJsonString.c_str() + theJsonString.size(), &json, &errors))
    throw Fmi::Exception(BCP, "Legend template file parsing failed!")
        .addParameter("Message", errors);

  return json;
}

void WMSLayer::setCustomer(const std::string& c)
{
  customer = c;

  std::map<std::string, Json::Value> legends = readLegendFiles(
      wmsConfig.getDaliConfig().rootDirectory(true), wmsConfig.getJsonCache(), customer);
  std::string missingTemplateFiles;
  if (legends.find("symbol") == legends.end())
    missingTemplateFiles += "symbol.json";
  if (legends.find("isoband") == legends.end())
    missingTemplateFiles += ",isoband.json";
  if (legends.find("isoline") == legends.end())
    missingTemplateFiles += ",isoline.json";
  if (missingTemplateFiles.size() > 0)
  {
    if (missingTemplateFiles.at(0) == ',')
      missingTemplateFiles = missingTemplateFiles.substr(1);

    throw Fmi::Exception(BCP, "Missing legend template file(s): " + missingTemplateFiles + "!");
  }
}

bool WMSLayer::identicalGeographicBoundingBox(const WMSLayer& layer) const
{
  return geographicBoundingBox == layer.geographicBoundingBox;
}

bool WMSLayer::identicalProjectedBoundingBox(const WMSLayer& layer) const
{
  for (const auto& item : projected_bbox)
  {
    if (layer.projected_bbox.find(item.first) == layer.projected_bbox.end())
      return false;
    const Engine::Gis::BBox& bbox1 = item.second;
    const Engine::Gis::BBox& bbox2 = layer.projected_bbox.at(item.first);
    if (bbox1.west != bbox2.west || bbox1.east != bbox2.east || bbox1.south != bbox2.south ||
        bbox1.north != bbox2.north)
      return false;
  }

  return true;
}

bool WMSLayer::identicalTimeDimension(const WMSLayer& layer) const
{
  // If both are nullptr -> identical
  if (layer.timeDimensions == nullptr && timeDimensions == nullptr)
    return true;

  // If one is nullptr -> not identical
  if (layer.timeDimensions == nullptr || timeDimensions == nullptr)
    return false;

  return layer.timeDimensions->isIdentical(*timeDimensions);
}

bool WMSLayer::identicalElevationDimension(const WMSLayer& layer) const
{
  // If both are nullptr -> identical
  if (layer.elevationDimension == nullptr && elevationDimension == nullptr)
    return true;

  // If one is nullptr -> not identical
  if (layer.elevationDimension == nullptr || elevationDimension == nullptr)
    return false;

  return layer.elevationDimension->isIdentical(*elevationDimension);
}

// When external legend file used this is called to set dimension
void WMSLayer::setLegendDimension(const WMSLayer& legendLayer, const std::string& styleLayerName)
{ 
  for (auto& style : itsStyles)
  {
	if(styleLayerName != style.first)
	  continue;
    if (legendLayer.getWidth() && legendLayer.getHeight())
    {
      style.second.legend_url.online_resource +=
          ("&amp;width=" + Fmi::to_string(*legendLayer.getWidth()) +
           "&amp;height=" + Fmi::to_string(*legendLayer.getHeight()));
      style.second.legend_url.width = Fmi::to_string(*legendLayer.getWidth());
      style.second.legend_url.height = Fmi::to_string(*legendLayer.getHeight());
    }
    else
    {
      // If width, height are NOT given in separate legend file, use defult 10
      style.second.legend_url.online_resource += ("&amp;width=10&amp;height=10");
      style.second.legend_url.width = "10";
      style.second.legend_url.height = "10";
    }
  }
}

void WMSLayer::setProductFile(const std::string& theProductFile)
{
  productFile = theProductFile;
  
  // Save modification time of product file
  boost::system::error_code ec;
  auto modtime = boost::filesystem::last_write_time(productFile, ec);
  if (ec.value() == boost::system::errc::success)
	itsProductFileModificationTime = boost::posix_time::from_time_t(modtime);
}

const boost::posix_time::ptime& WMSLayer::modificationTime() const
{
  return itsProductFileModificationTime;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
