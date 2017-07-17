#include "WMSLayer.h"
#include "TemplateFactory.h"
#include "WMS.h"
#include "WMSException.h"

#include <engines/gis/Engine.h>
#include <spine/Exception.h>

#include <macgyver/StringConversion.h>
#include <macgyver/TimeParser.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/foreach.hpp>
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
LegendGraphicInfo handle_json_layers(Json::Value layersJson)
{
  LegendGraphicInfo ret;
  Json::Value nulljson;

  for (unsigned int i = 0; i < layersJson.size(); i++)
  {
    const Json::Value& layerJson = layersJson[i];
    if (!layerJson.isNull())
    {
      std::map<std::string, std::string> lgParameters;
      auto layerTypeJson = layerJson.get("layer_type", nulljson);
      if (!layerTypeJson.isNull())
      {
        std::string layerTypeString = layerTypeJson.asString();
        std::string parameterName;
        auto json = layerJson.get("parameter", nulljson);
        if (!json.isNull())
          parameterName = json.asString();
        lgParameters.insert(make_pair("parameter_name", parameterName));

        if (layerTypeString == "icemap")
        {
          json = layerJson.get("layer_subtype", nulljson);

          if (!json.isNull())
            layerTypeString = json.asString();
        }

        if (layerTypeString != "isoband" && layerTypeString != "isoline" &&
            layerTypeString != "symbol")
          continue;

        lgParameters.insert(make_pair("layer_type", layerTypeString));

        json = layerJson.get("isobands", nulljson);
        if (!json.isNull())
          lgParameters.insert(make_pair("isobands", json.toStyledString()));
        json = layerJson.get("isolines", nulljson);
        if (!json.isNull())
          lgParameters.insert(make_pair("isolines", json.toStyledString()));
        json = layerJson.get("css", nulljson);
        if (!json.isNull())
          lgParameters.insert(make_pair("css", json.asString()));
        json = layerJson.get("symbol", nulljson);
        if (!json.isNull())
          lgParameters.insert(make_pair("symbol", json.asString()));
        json = layerJson.get("attributes", nulljson);
        if (!json.isNull())
        {
          json = json.get("class", nulljson);
          if (!json.isNull())
            lgParameters.insert(make_pair("class", json.asString()));
        }
        ret.push_back(lgParameters);
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

}  // anonymous namespace

// Remember to run updateLayerMetaData before using the layer!
WMSLayer::WMSLayer() : hidden(false)
{
  geographicBoundingBox.xMin = 0.0;
  geographicBoundingBox.xMax = 0.0;
  geographicBoundingBox.yMin = 0.0;
  geographicBoundingBox.yMax = 0.0;
}

void WMSLayer::addStyles(const Json::Value& root, const std::string& layerName)
{
  //  std::cout << "LAYERNAME: " << layerName << std::endl;

  Json::Value nulljson;

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
    addStyle(layerName);
}

void WMSLayer::addStyle(const std::string& layerName)
{
  WMSLayerStyle layerStyle;
  layerStyle.legend_url.width = 200;
  layerStyle.legend_url.height = 200;
  layerStyle.legend_url.format = "image/png";
  layerStyle.legend_url.online_resource =
      "xmlns:xlink=\"http://www.w3.org/1999/xlink\" xlink:type=\"simple\" "
      "xlink:href=\"__hostname__/wms?"
      "service=WMS&amp;request=GetLegendGraphic&amp;version=1.3.0&amp;sld_version=1.1.0&"
      "amp;"
      "layer=";
  layerStyle.legend_url.online_resource += layerName;
  layerStyle.legend_url.online_resource += "&amp;style=&amp;format=image%2Fpng\"";
  styles.push_back(layerStyle);
}

std::vector<std::string> WMSLayer::getLegendGraphic(const std::string& templateDirectory) const
{
  std::vector<std::string> ret;
  unsigned int xpos = 20;
  unsigned int ypos = 20;
  unsigned int symbol_xpos = 0;  // set later
  unsigned int symbol_ypos = 0;
  bool firstSymbol = true;
  unsigned int symbolGroupIndex = 0;

  struct hash_item
  {
    std::string templateFile;
    CTPP::CDT hash;
  };
  std::vector<hash_item> legendGraphicHashVector;

  hash_item symbolGroupHashItem;
  symbolGroupHashItem.templateFile = templateDirectory + "/wms_get_legend_graphic_symbol.c2t";
  for (auto lgi : legendGraphicInfo)
  {
    // lgi["layer_type"]];
    std::string layerType = lgi["layer_type"];
    std::string parameterName = lgi["parameter_name"];

    //    std::cout << "LAYER TYPE: " << layerType << std::endl;
    if (layerType == "symbol")
    {
      if (parameterName == "PrecipitationForm")
      {
        hash_item hi;
        hi.hash["header_xpos"] = xpos;
        hi.hash["header_ypos"] = ypos;
        hi.hash["rain_symbol_xpos"] = xpos + 20;
        hi.hash["rain_symbol_ypos"] = ypos + 15;
        hi.hash["rain_text_xpos"] = xpos + 30;
        hi.hash["rain_text_ypos"] = ypos + 20;
        hi.hash["drizzle_symbol_xpos"] = xpos + 20;
        hi.hash["drizzle_symbol_ypos"] = ypos + 35;
        hi.hash["drizzle_text_xpos"] = xpos + 30;
        hi.hash["drizzle_text_ypos"] = ypos + 40;
        hi.hash["snow_symbol_xpos"] = xpos + 20;
        hi.hash["snow_symbol_ypos"] = ypos + 55;
        hi.hash["snow_text_xpos"] = xpos + 30;
        hi.hash["snow_text_ypos"] = ypos + 60;
        hi.templateFile = (templateDirectory + "/wms_get_legend_graphic_precipitation_form.c2t");
        legendGraphicHashVector.push_back(hi);
        xpos += 150;
      }
      else
      {
        // template handling: all symnbols to the same group
        if (firstSymbol)
        {
          symbolGroupHashItem.hash["symbol_group_header"] = lgi["parameter_name"];
          symbolGroupHashItem.hash["header_xpos"] = xpos;
          symbolGroupHashItem.hash["header_ypos"] = ypos;
          symbolGroupHashItem.hash["symbol_group"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
          firstSymbol = false;
          symbol_xpos = xpos + 20;
          symbol_ypos = ypos + 50;
          xpos += 150;
        }
        CTPP::CDT& symbolHash = symbolGroupHashItem.hash["symbol_group"][symbolGroupIndex];
        symbolHash["name"] = lgi["symbol"];
        symbolHash["xpos"] = symbol_xpos;
        symbolHash["ypos"] = symbol_ypos;
        symbolHash["text"] = lgi["symbol"];
        symbolHash["text_xpos"] = symbol_xpos + 20;
        symbolHash["text_ypos"] = symbol_ypos + 15;
        symbol_ypos += 20;
        symbolGroupIndex++;
      }
    }
    else if (layerType == "isoband")
    {
      hash_item hi;
      hi.templateFile = templateDirectory + "/wms_get_legend_graphic_isoband.c2t";
      hi.hash["isoband_json"] = lgi["isobands"];
      hi.hash["isoband_css"] = lgi["css"];
      hi.hash["isoband_text_xpos"] = xpos;
      hi.hash["isoband_text_ypos"] = ypos;
      hi.hash["isoband_unit_xpos"] = xpos + 10;
      hi.hash["isoband_unit_ypos"] = ypos + 20;
      hi.hash["isoband_header"] = parameterName;
      hi.hash["isoband_id"] = (parameterName + "_label");

      if (parameterName == "Precipitation1h")
      {
        hi.hash["isoband_unit"] = "mm/h";
      }
      else if (parameterName == "Temperature")
      {
        hi.hash["isoband_unit"] = "celcius";
      }
      else if (parameterName == "WindSpeedMS")
      {
        hi.hash["isoband_unit"] = "m/s";
      }
      else
      {
        hi.hash["isoband_unit"] = "";
      }
      legendGraphicHashVector.push_back(hi);
      xpos += 150;
    }
    else if (layerType == "isoline")
    {
      hash_item hi;
      hi.templateFile = templateDirectory + "/wms_get_legend_graphic_isoline.c2t";
      hi.hash["isoline_header"] = parameterName + " isoline";
      hi.hash["isoline_css"] = lgi["css"];
      hi.hash["isoline_class"] = lgi["class"];
      hi.hash["isoline_text_xpos"] = xpos;
      hi.hash["isoline_text_ypos"] = ypos;
      hi.hash["isoline_symbol_xpos"] = xpos + 10;
      hi.hash["isoline_symbol_ypos"] = ypos + 20;
      hi.hash["isoline_symbol_id"] = (parameterName + "_isoline");
      legendGraphicHashVector.push_back(hi);
      xpos += 150;
    }
  }

  if (symbolGroupIndex > 0)
    legendGraphicHashVector.push_back(symbolGroupHashItem);

  Dali::TemplateFactory templateFactory;
  for (auto item : legendGraphicHashVector)
  {
    Dali::SharedFormatter formatter = templateFactory.get(item.templateFile);
    std::stringstream tmpl_ss;
    std::stringstream logstream;
    formatter->process(item.hash, tmpl_ss, logstream);
    /*
std::cout << "processed template: " << item.templateFile << std::endl
          << legendGraphicHashVector.size() << "," << std::endl
          << tmpl_ss.str() << std::endl;
    */
    ret.push_back(tmpl_ss.str());
  }

  return ret;
}

std::string WMSLayer::getName() const
{
  return name;
}
std::string WMSLayer::getCustomer() const
{
  return customer;
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

bool WMSLayer::isValidStyle(const std::string& theStyle) const
{
  try
  {
    for (auto style : styles)
      if (style.name == theStyle)
        return true;

    return false;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::ostream& operator<<(std::ostream& ost, const WMSLayer& layer)
{
  try
  {
    ost << "********** "
        << "name: " << layer.name << " **********" << std::endl
        << "title: " << layer.title << std::endl
        << "abstract: " << layer.abstract << std::endl
        << "queryable: " << (layer.queryable ? "true" : "false") << std::endl
        << "keywords: " << boost::algorithm::join(layer.keywords, " ") << std::endl
        << "geographicBoundingBox: " << std::endl
        << " westBoundLongitude=" << layer.geographicBoundingBox.xMin << " "
        << "southBoundLatitude=" << layer.geographicBoundingBox.yMin << " "
        << "eastBoundLongitude=" << layer.geographicBoundingBox.xMax << " "
        << "northBoundLatitude=" << layer.geographicBoundingBox.yMax << std::endl
        << "crs: " << boost::algorithm::join(layer.crs, " ") << std::endl
        << "styles: ";
    for (auto style : layer.styles)
      ost << style.name << " ";
    if (layer.styles.size() > 0)
      ost << " " << std::endl;
    ost << "customer: " << layer.customer << std::endl;

    if (layer.timeDimension)
    {
      ost << "timeDimension: " << layer.timeDimension;
    }
    else
    {
      ost << "timeDimension: Empty";
    }

    ost << "*******************************" << std::endl;

    return ost;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

std::string WMSLayer::generateGetCapabilities(const Engine::Gis::Engine& gisengine)
{
  try
  {
    if (hidden)
      return "";

    std::string ss = std::string(" <Layer queryable=") +
                     enclose_with_quotes(queryable ? "1" : "0") + " opaque=\"1\">" + "\n" +
                     "   <Name>" + name + "</Name>" + "\n" + "   <Title>" + title + "</Title>" +
                     "\n" + "   <Abstract>" + abstract + "</Abstract>" + "\n";

    std::vector<std::string> supportedCRS;

    double lonMin = geographicBoundingBox.xMin;
    double lonMax = geographicBoundingBox.xMax;
    double latMin = geographicBoundingBox.yMin;
    double latMax = geographicBoundingBox.yMax;

    // EX_GeographicBoundingBox
    std::string bbox_ss =
        std::string("   <EX_GeographicBoundingBox>") + "\n" + "\t<westBoundLongitude>" +
        Fmi::to_string(lonMin) + "</westBoundLongitude>" + "\n" + "\t<eastBoundLongitude>" +
        Fmi::to_string(lonMax) + "</eastBoundLongitude>" + "\n" + "\t<southBoundLatitude>" +
        Fmi::to_string(latMin) + "</southBoundLatitude>" + "\n" + "\t<northBoundLatitude>" +
        Fmi::to_string(latMax) + "</northBoundLatitude>" + "\n" + "   </EX_GeographicBoundingBox>" +
        "\n";

    bbox_ss += "   <BoundingBox CRS=" + enclose_with_quotes("EPSG:4326");
    bbox_ss += " minx=\"" + Fmi::to_string(latMin) + "\"";
    bbox_ss += " miny=\"" + Fmi::to_string(lonMin) + "\"";
    bbox_ss += " maxx=\"" + Fmi::to_string(latMax) + "\"";
    bbox_ss += " maxy=\"" + Fmi::to_string(lonMax) + "\"";
    bbox_ss += " />\n";

    // crs bounding boxes
    // convert lon/lat-coordinated to crs coordinates
    for (std::set<std::string>::const_iterator iter = crs.begin(); iter != crs.end(); iter++)
    {
      std::string crs(*iter);

      if (crs == "EPSG:4326")
      {
        supportedCRS.push_back(crs);
        continue;
      }

      OGRSpatialReference oSourceSRS, oTargetSRS;

      double x1(lonMin), y1(latMin), x2(lonMax), y2(latMax);

      oSourceSRS.importFromEPSGA(4326);

      int targetEPSGNumber = stoi(iter->substr(5));
      OGRErr err = oTargetSRS.importFromEPSGA(targetEPSGNumber);

      if (err != OGRERR_NONE)
        throw Spine::Exception(BCP, "Unknown CRS: '" + crs + "'");

      boost::shared_ptr<OGRCoordinateTransformation> poCT(
          OGRCreateCoordinateTransformation(&oSourceSRS, &oTargetSRS));

      if (poCT == NULL)
        throw Spine::Exception(BCP, "OGRCreateCoordinateTransformation function call failed");

      // Intersect with target EPSG bounding box

      Engine::Gis::BBox epsg_box = gisengine.getBBox(targetEPSGNumber);
      x1 = std::max(epsg_box.west, x1);
      x2 = std::min(epsg_box.east, x2);
      y1 = std::max(epsg_box.south, y1);
      y2 = std::min(epsg_box.north, y2);

      if (x1 >= x2 || y1 >= y2)
      {
        // Non-overlapping bbox
        continue;
      }

      bool ok = (poCT->Transform(1, &x1, &y1) && poCT->Transform(1, &x2, &y2));
      if (!ok)
      {
        // Corner transformations failed
        continue;
      }

      supportedCRS.push_back(crs);

      bbox_ss += "   <BoundingBox CRS=" + enclose_with_quotes(crs);

      // check if coordinate ordering must be changed
      if (oTargetSRS.EPSGTreatsAsLatLong())
      {
        bbox_ss += " minx=\"" + fmt::sprintf("%f", y1) + "\"";
        bbox_ss += " miny=\"" + fmt::sprintf("%f", x1) + "\"";
        bbox_ss += " maxx=\"" + fmt::sprintf("%f", y2) + "\"";
        bbox_ss += " maxy=\"" + fmt::sprintf("%f", x2) + "\"";
      }
      else
      {
        bbox_ss += " minx=\"" + fmt::sprintf("%f", x1) + "\"";
        bbox_ss += " miny=\"" + fmt::sprintf("%f", y1) + "\"";
        bbox_ss += " maxx=\"" + fmt::sprintf("%f", x2) + "\"";
        bbox_ss += " maxy=\"" + fmt::sprintf("%f", y2) + "\"";
      }

      bbox_ss += " />\n";
    }

    // first insert list of supported CRS
    BOOST_FOREACH (const std::string& crs, supportedCRS)
      ss += "   <CRS>" + crs + "</CRS>\n";

    ss += bbox_ss;

    if (timeDimension)
    {
      // then bounding boxes of these CRS
      ss += "   <Dimension name=\"time\" units=" + enclose_with_quotes("ISO8601") +
            "  multipleValues=" + enclose_with_quotes("0") +
            "  nearestValue=" + enclose_with_quotes("0") +
            "  current=" + enclose_with_quotes(timeDimension->currentValue() ? "1" : "0") + ">" +
            timeDimension->getCapabilities() + "</Dimension>" + "\n";
    }

    // add layer styles
    for (auto style : styles)
      ss += style.toXML();

    ss += " </Layer>\n";

    return ss;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
