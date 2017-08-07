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
      std::string(
          "http://__hostname__/wms"
          "?service=WMS&amp;request=GetLegendGraphic&amp;version=1.3.0&amp;sld_version=1.1.0&amp;"
          "style=&amp;format=image%2Fpng&amp;layer=") +
      Spine::HTTP::urlencode(layerName);

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
    throw Spine::Exception(BCP, "Validating CRS failed!", NULL);
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
    throw Spine::Exception(BCP, "Validating style failed!", NULL);
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
    throw Spine::Exception(BCP, "Validating time failed!", NULL);
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
    throw Spine::Exception(BCP, "Validating current time failed!", NULL);
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
    throw Spine::Exception(BCP, "Validating most current time!", NULL);
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
    throw Spine::Exception(BCP, "WMS info operation failed!", NULL);
  }
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
        << "crs: " << boost::algorithm::join(layer.crs, " ") << "\n"
        << "styles: ";
    for (auto style : layer.styles)
      ost << style.name << " ";
    if (layer.styles.size() > 0)
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
    throw Spine::Exception(BCP, "Printing the request failed!", NULL);
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
    layer_bbox["west_bound_longitude"] = geographicBoundingBox.xMin;
    layer_bbox["east_bound_longitude"] = geographicBoundingBox.xMax;
    layer_bbox["south_bound_latitude"] = geographicBoundingBox.yMin;
    layer_bbox["north_bound_latitude"] = geographicBoundingBox.yMax;
    layer["ex_geographic_bounding_box"] = layer_bbox;

    // Layer CRS list and their bounding boxes

    if (!crs.empty())
    {
      CTPP::CDT layer_crs_list(CTPP::CDT::ARRAY_VAL);
      CTPP::CDT layer_bbox_list(CTPP::CDT::ARRAY_VAL);

      for (const auto& crs_name : crs)
      {
        CTPP::CDT layer_bbox(CTPP::CDT::HASH_VAL);

        layer_bbox["crs"] = crs_name;

        if (crs_name == "EPSG:4326")
        {
          layer_crs_list.PushBack(crs_name);
          layer_bbox["minx"] = geographicBoundingBox.xMin;
          layer_bbox["miny"] = geographicBoundingBox.yMin;
          layer_bbox["maxx"] = geographicBoundingBox.xMax;
          layer_bbox["maxy"] = geographicBoundingBox.yMax;
          layer_bbox_list.PushBack(layer_bbox);
        }
        else
        {
          // Calculate CRS bbox from latlon bbox
          OGRSpatialReference srs;
          srs.importFromEPSGA(4326);

          int target_epsg_number = stoi(crs_name.substr(5));
          OGRSpatialReference target;
          auto err = target.importFromEPSGA(target_epsg_number);
          if (err != OGRERR_NONE)
            throw Spine::Exception(BCP, "Unknown CRS: ' " + crs_name + "'");

          boost::shared_ptr<OGRCoordinateTransformation> transformation(
              OGRCreateCoordinateTransformation(&srs, &target));

          if (transformation == nullptr)
            throw Spine::Exception(BCP, "OGRCreateCoordinateTransformation function call failed");

          // Intersect with target EPSG bounding box (latlon)

          Engine::Gis::BBox epsg_box = gisengine.getBBox(target_epsg_number);
          auto x1 = std::max(epsg_box.west, geographicBoundingBox.xMin);
          auto x2 = std::min(epsg_box.east, geographicBoundingBox.xMax);
          auto y1 = std::max(epsg_box.south, geographicBoundingBox.yMin);
          auto y2 = std::min(epsg_box.north, geographicBoundingBox.yMax);

          // Produce bbox only if there is overlap

          if (x1 < x2 && y1 < y2)
          {
            bool ok =
                (transformation->Transform(1, &x1, &y1) && transformation->Transform(1, &x2, &y2));

            // Produce bbox only if projection succeeds

            if (ok)
            {
              // Acceptable CRS
              layer_crs_list.PushBack(crs_name);

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

      // TODO: Do we need these?
      // layer_dimension["unit_symbol"] = ???
      // layer_dimension["default"] = ???

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
    if (!styles.empty())
    {
      CTPP::CDT layer_style_list(CTPP::CDT::ARRAY_VAL);
      for (const auto& style : styles)
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
    throw Spine::Exception(BCP, "Failed to generate capabilities for the layer!", NULL);
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
