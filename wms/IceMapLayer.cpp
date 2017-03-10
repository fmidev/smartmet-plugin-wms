//======================================================================

#include "IceMapLayer.h"
#include "Config.h"
#include "Geometry.h"
#include "Hash.h"
#include "Layer.h"
#include "State.h"
#include <spine/Json.h>
#include <spine/Exception.h>
#include <macgyver/String.h>
#include <gis/Host.h>
#include <gis/PostGIS.h>
#include <gis/Box.h>
#include <gis/OGR.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
#include <boost/timer/timer.hpp>
#include <cairo.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct text_dimension_t
{
  unsigned int width;
  unsigned int height;
};

struct text_style_t
{
  std::string fontname;
  std::string fontsize;
  std::string fontstyle;
  std::string fontweight;
  std::string textanchor;

  text_style_t()
      : fontname("Arial"),
        fontsize("10"),
        fontstyle("normal"),
        fontweight("normal"),
        textanchor("start")
  {
  }
  text_style_t(const text_style_t& s)
      : fontname(s.fontname),
        fontsize(s.fontsize),
        fontstyle(s.fontstyle),
        fontweight(s.fontweight),
        textanchor(s.textanchor)
  {
  }
};

typedef boost::shared_ptr<OGRSpatialReference> OGRSpatialReferencePtr;
typedef std::unique_ptr<OGRCoordinateTransformation> OGRCoordinateTransformationPtr;

namespace
{
const char* const attribute_columns[] = {"firstname_column",
                                         "secondname_column",
                                         "nameposition_column",
                                         "angle_column",
                                         "labeltext_column",
                                         "fontname_column",
                                         "fontsize_column",
                                         "time_column"};

class PostGISAttributeToString : public boost::static_visitor<std::string>
{
 public:
  std::string operator()(int i) const { return Fmi::to_string(i); }
  std::string operator()(double d) const { return Fmi::to_string(d); }
  std::string operator()(const std::string& str) const
  {
    std::string ret(str);
    boost::algorithm::trim(ret);
    return ret;
  }
};

std::string getMeanTemperatureColumnName(boost::posix_time::ptime t)
{
  boost::posix_time::time_period summer_period(
      boost::posix_time::ptime(boost::gregorian::date(t.date().year(), boost::gregorian::Mar, 20),
                               boost::posix_time::time_duration(0, 0, 0)),
      boost::posix_time::ptime(boost::gregorian::date(t.date().year(), boost::gregorian::Oct, 20),
                               boost::posix_time::time_duration(23, 59, 59)));

  // date must be between 21.10-21.3
  if (summer_period.contains(t))
    return "";

  // replace _DD_ and _MM_ with current day and month
  std::string month(Fmi::to_string(t.date().month()));
  // 29.2. does not exist in db -> use 28.2 instead
  std::string day((t.date().month() == 2 && t.date().day() == 29) ? "28"
                                                                  : Fmi::to_string(t.date().day()));

  if (month.size() == 1)
    month.insert(0, "0");
  if (day.size() == 1)
    day.insert(0, "0");

  return ("o" + day + "o" + month + "o");
}

std::vector<std::string> getAttributeColumns(const std::map<std::string, std::string>& parameters,
                                             boost::optional<boost::posix_time::ptime> time)
{
  std::vector<std::string> column_name_vector;
  for (auto col : attribute_columns)
  {
    if (parameters.find(col) == parameters.end())
      continue;

    std::string column_name = parameters.at(col);
    boost::algorithm::trim(column_name);

    // mean temperature column name is determined by given date
    if (column_name == std::string("o_DD_o_MM_o") && time)
    {
      std::string mean_temperature_column(getMeanTemperatureColumnName(*time));

      if (mean_temperature_column.empty())
      {
        column_name_vector.clear();
        return column_name_vector;
      }
      column_name_vector.push_back(getMeanTemperatureColumnName(*time));
    }
    else
      column_name_vector.push_back(column_name);
  }

  return column_name_vector;
}

OGRCoordinateTransformationPtr getTransformation(OGRSpatialReferencePtr sr)
{
  // Get the geonames projection

  std::unique_ptr<OGRSpatialReference> geocrs(new OGRSpatialReference);
  OGRErr err = geocrs->SetFromUserInput("WGS84");
  if (err != OGRERR_NONE)
    throw std::runtime_error("GDAL does not understand this crs 'WGS84'");

  // Create the coordinate transformation from geonames coordinates to image coordinates

  OGRCoordinateTransformationPtr transformation(
      OGRCreateCoordinateTransformation(geocrs.get(), sr.get()));
  if (!transformation)
    throw std::runtime_error(
        "Failed to create the needed coordinate transformation when drawing locations");

  return transformation;
}

text_dimension_t getTextDimension(const std::string& text,  // Text
                                  const text_style_t& text_style)
{
  cairo_font_slant_t cairo_fontstyle =
      (text_style.fontstyle == "normal"
           ? CAIRO_FONT_SLANT_NORMAL
           : (text_style.fontstyle == "italic" ? CAIRO_FONT_SLANT_ITALIC
                                               : CAIRO_FONT_SLANT_OBLIQUE));
  cairo_font_weight_t cairo_fontsweight =
      (text_style.fontweight == "normal" ? CAIRO_FONT_WEIGHT_NORMAL : CAIRO_FONT_WEIGHT_BOLD);

  // cairo surface to get font mectrics
  cairo_surface_t* cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t* cr = cairo_create(cs);
  cairo_surface_destroy(cs);

  cairo_select_font_face(cr, text_style.fontname.c_str(), cairo_fontstyle, cairo_fontsweight);
  cairo_set_font_size(cr, Fmi::stoi(text_style.fontsize));

  cairo_text_extents_t extents;
  cairo_text_extents(cr, text.c_str(), &extents);

  text_dimension_t ret;

  ret.width = static_cast<unsigned int>(ceil(std::max(extents.width, extents.x_advance)));
  ret.height = static_cast<unsigned int>(ceil(extents.height));

  return ret;
}

text_style_t getTextStyle(const Attributes& theAttributes, const text_style_t& default_values)
{
  text_style_t ret(default_values);

  if (!theAttributes.value("font-family").empty())
    ret.fontname = theAttributes.value("font-family");

  if (!theAttributes.value("font-size").empty())
    ret.fontsize = theAttributes.value("font-size");

  if (!theAttributes.value("font-style").empty())
    ret.fontstyle = theAttributes.value("font-style");

  if (!theAttributes.value("font-weight").empty())
    ret.fontweight = theAttributes.value("font-weight");

  if (!theAttributes.value("text-anchor").empty())
    ret.textanchor = theAttributes.value("text-anchor");

  return ret;
}

std::string convertText(const std::string& theText)
{
  std::string ret(theText);  // original text is returned by default

  if (theText == "Slight Pressure" || theText == "Slight to Moderate Pressure")
    ret = "1";
  else if (theText == "Moderate Pressure" || theText == "Moderate to Strong Pressure")
    ret = "2";
  else if (theText == "Strong Pressure")
    ret = "3";
  else if (theText == "No Pressure")
    ret = "0";

  return ret;
}

bool operator==(const Fmi::Feature& f1, const Fmi::Feature& f2)
{
  if ((!f1.geom && f2.geom) || (f1.geom && !f2.geom))
    return false;

  if (f1.geom && f2.geom && !f1.geom->Equals(f2.geom.get()))
    return false;

  return (f1.attributes == f2.attributes);
}

bool operator==(const Fmi::FeaturePtr& f1, const Fmi::FeaturePtr& f2)
{
  if (!f1 && !f2)
    return true;

  if ((!f1 && f2) || (f1 && !f2))
    return false;

  return (*f1 == *f2);
}
}  // anonymous namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void IceMapLayer::init(const Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "PostGIS JSON is not a JSON object");

    PostGISLayerBase::init(theJson, theState, theConfig, theProperties);

    // Extract all members

    Json::Value nulljson, json;

    // layer_subtype specifies the final layer type, e.g. label, symbol, named_location, ...
    json = theJson.get("layer_subtype", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("layer_subtype", json.asString()));

    // name of symbol file
    json = theJson.get("symbol", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("symbol", json.asString()));

    // column name of first name (e.g. 'Helsinki')
    json = theJson.get("firstname_column", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("firstname_column", json.asString()));

    // column name of second name (e.g. 'Helsingfors')
    json = theJson.get("secondname_column", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("secondname_column", json.asString()));

    // name position
    json = theJson.get("nameposition_column", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("nameposition_column", json.asString()));

    // angle column
    json = theJson.get("angle_column", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("angle_column", json.asString()));

    // name of pattern file
    json = theJson.get("pattern", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("pattern", json.asString()));

    // labeltext_column is one of attribute colmuns
    json = theJson.get("labeltext_column", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("labeltext_column", json.asString()));

    // list of all attribute columns
    json = theJson.get("attribute_columns", nulljson);

    if (!json.isNull())
      itsParameters.insert(make_pair("attribute_columns", json.asString()));

    // fontname column of the label
    json = theJson.get("fontname_column", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("fontname_column", json.asString()));

    // fontsize column of the label
    json = theJson.get("fontsize_column", nulljson);
    if (!json.isNull())
      itsParameters.insert(make_pair("fontsize_column", json.asString()));

    // if time missing set current time
    if (!time)
      time = boost::posix_time::second_clock::universal_time();
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void IceMapLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    std::string report = "IceMapLayer::generate finished in %t sec CPU, %w sec real\n";
    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer.reset(new boost::timer::auto_cpu_timer(2, report));

    // Get projection details
    if (!projection.crs)
      throw std::runtime_error("IceMapLayer projection not set");

    if (theState.addId("spearhead"))
      theGlobals["includes"]["marker"] = theState.getMarker("spearhead");
    if (theState.addId("jmdbrr"))
      theGlobals["includes"]["jmdbrr"] = theState.getSymbol("jmdbrr");
    if (theState.addId("icelea"))
      theGlobals["includes"]["icelea"] = theState.getSymbol("icelea");
    if (theState.addId("icefra"))
      theGlobals["includes"]["icefra"] = theState.getSymbol("icefra");
    if (theState.addId("icefra_zone"))
      theGlobals["includes"]["icefra_zone"] = theState.getSymbol("icefra_zone");
    if (theState.addId("flobrg"))
      theGlobals["includes"]["flobrg"] = theState.getSymbol("flobrg");
    if (theState.addId("ice_edge"))
      theGlobals["includes"]["ice_edge"] = theState.getSymbol("ice_edge");
    if (theState.addId("estimated_ice_edge"))
      theGlobals["includes"]["estimated_ice_edge"] = theState.getSymbol("estimated_ice_edge");
    if (theState.addId("icedft"))
      theGlobals["includes"]["icedft"] = theState.getSymbol("icedft");
    if (theState.addId("icerdg"))
      theGlobals["includes"]["icerdg"] = theState.getSymbol("icerdg");
    if (theState.addId("icerft"))
      theGlobals["includes"]["icerft"] = theState.getSymbol("icerft");
    if (theState.addId("sea_surface_mean_temperature"))
      theGlobals["includes"]["sea_surface_mean_temperature"] =
          theState.getSymbol("sea_surface_mean_temperature");
    if (theState.addId("icecom"))
      theGlobals["includes"]["icecom"] = theState.getSymbol("icecom");
    if (theState.addId("watertemperature_isotherm"))
      theGlobals["includes"]["watertemperature_isotherm"] =
          theState.getSymbol("watertemperature_isotherm");
    if (theState.addId("ice_thickness"))
      theGlobals["includes"]["ice_thickness"] = theState.getSymbol("ice_thickness");
    if (theState.addId("lighthouse"))
      theGlobals["includes"]["lighthouse"] = theState.getSymbol("lighthouse");

    auto crs = projection.getCRS();

    // Update the globals
    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *(css);
      theGlobals["css"][name] = theState.getStyle(*(css));
    }

    // Clip if necessary

    addClipRect(theLayersCdt, theGlobals, projection.getBox(), theState);

    Engine::Gis::MapOptions mapOptions;
    mapOptions.pgname = pgname;
    mapOptions.schema = schema;
    mapOptions.table = table;

    CTPP::CDT theGroupCdt(CTPP::CDT::HASH_VAL);
    if (!theState.inDefs())
    {
      theGroupCdt["start"] = "<g";
      theGroupCdt["end"] = "</g>";
      // Add attributes to the group, not the areas
      theState.addAttributes(theGlobals, theGroupCdt, attributes);
    }

    unsigned int s = theLayersCdt.Size();
    theLayersCdt.PushBack(theGroupCdt);

    OGRSpatialReferencePtr projectionSR = projection.getCRS();
    OGRSpatialReference defaultSR;
    defaultSR.importFromEPSG(3395);  // if sr is missing use this one

    unsigned int mapid(1);  // id to concatenate to iri to make it unique
    // Get the polygons and store them into the template engine
    BOOST_FOREACH (const PostGISLayerFilter& filter, filters)
    {
      if (time_condition && filter.where)
        mapOptions.where = (*filter.where + " AND " + *time_condition);
      else if (time_condition)
        mapOptions.where = time_condition;
      else if (filter.where)
        mapOptions.where = filter.where;

      std::vector<std::string> attribute_column_names = getAttributeColumns(itsParameters, time);

      std::string layer_subtype(getParameterValue("layer_subtype"));

      // mean temperature needs a column name (requested time must be between 21.10-21.3)
      if (attribute_column_names.empty() && layer_subtype == "mean_temperature")
        return;

      mapOptions.fieldnames.insert(attribute_column_names.begin(), attribute_column_names.end());

      Fmi::Features result_set = getFeatures(theState, projectionSR.get(), mapOptions);

      for (auto result_item : result_set)
      {
        if (result_item->geom && !result_item->geom->IsEmpty())
        {
          OGRSpatialReference* sr = result_item->geom->getSpatialReference();

          if (!sr)
          {
            result_item->geom->assignSpatialReference(&defaultSR);
            result_item->geom->transformTo(projectionSR.get());
          }

          handleResultItem(
              *result_item, filter, mapid, theGlobals, theLayersCdt, theGroupCdt, theState);
        }
      }
    }

    theLayersCdt.At(s) = theGroupCdt;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Parameter value
 */
// ----------------------------------------------------------------------

std::string IceMapLayer::getParameterValue(const std::string& theKey) const
{
  try
  {
    if (itsParameters.find(theKey) == itsParameters.end())
      return "";

    return itsParameters.at(theKey);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t IceMapLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, Dali::hash_value(itsParameters));
    boost::hash_combine(hash, Dali::hash_value(filters, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

void IceMapLayer::handleSymbol(const Fmi::Feature& theResultItem, CTPP::CDT& theGroupCdt) const
{
  const Fmi::Box& box = projection.getBox();
  OGRSpatialReferencePtr sr = projection.getCRS();
  OGRCoordinateTransformationPtr transformation(getTransformation(sr));

  std::string iri(getParameterValue("symbol"));

  if (theResultItem.geom && !theResultItem.geom->IsEmpty())
  {
    // envelope should work for point and areas
    OGREnvelope envelope;
    theResultItem.geom->getEnvelope(&envelope);

    double lon = ((envelope.MinX + envelope.MaxX) / 2);
    double lat = ((envelope.MinY + envelope.MaxY) / 2);

    transformation->Transform(1, &lon, &lat);

    box.transform(lon, lat);

    lon -= 15;
    lat -= 5;

    // Start generating the hash
    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";
    tag_cdt["attributes"]["xlink:href"] = "#" + iri;
    tag_cdt["attributes"]["x"] = Fmi::to_string(std::round(lon));
    tag_cdt["attributes"]["y"] = Fmi::to_string(std::round(lat));
    theGroupCdt["tags"].PushBack(tag_cdt);
  }
}

void IceMapLayer::handleNamedLocation(const Fmi::Feature& theResultItem,
                                      const PostGISLayerFilter& theFilter,
                                      CTPP::CDT& theGlobals,
                                      CTPP::CDT& theLayersCdt,
                                      CTPP::CDT& theGroupCdt,
                                      State& theState) const
{
  const Fmi::Box& box = projection.getBox();
  OGRSpatialReferencePtr sr = projection.getCRS();
  OGRCoordinateTransformationPtr transformation(getTransformation(sr));

  if (theResultItem.geom && !theResultItem.geom->IsEmpty())
  {
    const OGRPoint* point = static_cast<const OGRPoint*>(theResultItem.geom.get());
    double lon(point->getX());
    double lat(point->getY());

    transformation->Transform(1, &lon, &lat);

    box.transform(lon, lat);

    // Start generating the hash
    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";

    std::string iri(getParameterValue("symbol"));

    if (!iri.empty())
    {
      if (theState.addId(iri))
        theGlobals["includes"][iri] = theState.getSymbol(iri);
      tag_cdt["attributes"]["xlink:href"] = "#" + iri;
      tag_cdt["attributes"]["x"] = Fmi::to_string(std::round(lon - 6));
      tag_cdt["attributes"]["y"] = Fmi::to_string(std::round(lat - 6));
      theGroupCdt["tags"].PushBack(tag_cdt);
    }

    // first name second name
    std::string first_name;
    std::string second_name;
    std::string name_position("label_location");
    if (itsParameters.find("firstname_column") != itsParameters.end())
      first_name =
          boost::apply_visitor(PostGISAttributeToString(),
                               theResultItem.attributes.at(itsParameters.at("firstname_column")));
    if (itsParameters.find("secondname_column") != itsParameters.end())
      second_name =
          boost::apply_visitor(PostGISAttributeToString(),
                               theResultItem.attributes.at(itsParameters.at("secondname_column")));
    if (itsParameters.find("nameposition_column") != itsParameters.end())
      name_position = boost::apply_visitor(
          PostGISAttributeToString(),
          theResultItem.attributes.at(itsParameters.at("nameposition_column")));

    if (name_position.empty() ||
        !(std::all_of(name_position.begin(), name_position.end(), ::isdigit)))
      name_position = "2";  // by default name is positioned on the right side of the symbol

    // we may add arrow beside the name
    std::string arrow_angle;
    if (itsParameters.find("angle_column") != itsParameters.end())
      arrow_angle =
          boost::apply_visitor(PostGISAttributeToString(),
                               theResultItem.attributes.at(itsParameters.at("angle_column")));

    boost::algorithm::trim(arrow_angle);

    if (arrow_angle.empty())
      arrow_angle = "-1";

    addLocationName(lon,
                    lat,
                    first_name,
                    second_name,
                    Fmi::stoi(name_position),
                    Fmi::stoi(arrow_angle),
                    theFilter,
                    theGlobals,
                    theLayersCdt,
                    theGroupCdt,
                    theState);
  }
}

void IceMapLayer::handleLabel(const Fmi::Feature& theResultItem,
                              const PostGISLayerFilter& theFilter,
                              CTPP::CDT& theGlobals,
                              CTPP::CDT& theLayersCdt,
                              State& theState) const
{
  std::vector<std::string> attribute_columns = getAttributeColumns(itsParameters, time);

  if (attribute_columns.empty())
    return;

  OGREnvelope envelope;
  theResultItem.geom->getEnvelope(&envelope);

  double xpos = (envelope.MinX);
  double ypos = (envelope.MinY);

  std::string label_text("");
  text_style_t text_style;

  // default name of text column for a label is 'textstring'
  std::string labeltext_column("textstring");
  std::string fontname_column("fontname");
  std::string fontsize_column("fontsize");
  if (itsParameters.find("labeltext_column") != itsParameters.end())
    labeltext_column = itsParameters.at("labeltext_column");
  if (theResultItem.attributes.find(labeltext_column) != theResultItem.attributes.end())
    label_text = boost::apply_visitor(PostGISAttributeToString(),
                                      theResultItem.attributes.at(labeltext_column));

  if (itsParameters.find("fontname_column") != itsParameters.end())
    fontname_column = itsParameters.at("fontname_column");
  if (theResultItem.attributes.find(labeltext_column) != theResultItem.attributes.end())
    text_style.fontname = boost::apply_visitor(PostGISAttributeToString(),
                                               theResultItem.attributes.at(fontname_column));

  if (itsParameters.find("fontsize_column") != itsParameters.end())
    fontname_column = itsParameters.at("fontsize_column");
  if (theResultItem.attributes.find(labeltext_column) != theResultItem.attributes.end())
    text_style.fontsize = boost::apply_visitor(PostGISAttributeToString(),
                                               theResultItem.attributes.at(fontsize_column));

  // erase decimal part from fontsize
  if (text_style.fontsize.find(".") != std::string::npos)
    text_style.fontsize.erase(text_style.fontsize.find("."));

  text_style = getTextStyle(theFilter.text_attributes, text_style);

  OGRSpatialReferencePtr sr = projection.getCRS();
  OGRCoordinateTransformationPtr transformation(getTransformation(sr));
  const Fmi::Box& box = projection.getBox();

  transformation->Transform(1, &xpos, &ypos);

  box.transform(xpos, ypos);

  text_dimension_t text_dimension = getTextDimension(label_text, text_style);

  boost::replace_all(label_text, "<", "&#60;");
  boost::replace_all(label_text, ">", "&#62;");

  // background rectangle
  if (!theFilter.attributes.empty())
  {
    CTPP::CDT background_rect_cdt(CTPP::CDT::HASH_VAL);
    background_rect_cdt["start"] = "<rect";
    background_rect_cdt["end"] = "</rect>";
    background_rect_cdt["attributes"]["width"] = Fmi::to_string(text_dimension.width + 5);
    background_rect_cdt["attributes"]["height"] = Fmi::to_string(text_dimension.height + 5);
    background_rect_cdt["attributes"]["x"] = Fmi::to_string(xpos);
    background_rect_cdt["attributes"]["y"] = Fmi::to_string(ypos - (text_dimension.height * 2.0));
    theState.addAttributes(theGlobals, background_rect_cdt, theFilter.attributes);
    theLayersCdt.PushBack(background_rect_cdt);
  }

  // label text
  CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
  text_cdt["start"] = "<text";
  text_cdt["end"] = "</text>";
  text_cdt["cdata"] =
      convertText(label_text);  // original text may be converted to some other at this point
  text_cdt["attributes"]["x"] = Fmi::to_string(xpos + 2);
  text_cdt["attributes"]["y"] = Fmi::to_string(ypos - (text_dimension.height) + 2);

  if (theFilter.text_attributes.empty())
  {
    text_cdt["attributes"]["font-family"] = text_style.fontname;
    text_cdt["attributes"]["font-size"] = text_style.fontsize;
    text_cdt["attributes"]["font-style"] = text_style.fontstyle;
    text_cdt["attributes"]["font-weight"] = text_style.fontweight;
    text_cdt["attributes"]["text-anchor"] = text_style.textanchor;
  }
  else
  {
    theState.addAttributes(theGlobals, text_cdt, theFilter.text_attributes);
  }

  theLayersCdt.PushBack(text_cdt);
}

void IceMapLayer::handleMeanTemperature(const Fmi::Feature& theResultItem,
                                        const PostGISLayerFilter& theFilter,
                                        CTPP::CDT& theGlobals,
                                        CTPP::CDT& theLayersCdt,
                                        State& theState) const
{
  std::string col_name(getMeanTemperatureColumnName(*time));

  // mean temperature
  std::string mean_temperature =
      boost::apply_visitor(PostGISAttributeToString(), theResultItem.attributes.at(col_name));

  if (mean_temperature.empty() || !isdigit(mean_temperature.at(0)))
    return;

  if (mean_temperature.size() == 1)
    mean_temperature += ".0";
  mean_temperature.append("°");

  text_style_t text_style = getTextStyle(theFilter.text_attributes, text_style_t());
  text_dimension_t text_dimension = getTextDimension(mean_temperature, text_style);

  // position of the geometry mean temperature
  const OGRPoint* point = static_cast<const OGRPoint*>(theResultItem.geom.get());

  double xpos = point->getX();
  double ypos = point->getY();

  OGRSpatialReferencePtr sr = projection.getCRS();
  OGRCoordinateTransformationPtr transformation(getTransformation(sr));
  const Fmi::Box& box = projection.getBox();
  transformation->Transform(1, &xpos, &ypos);
  box.transform(xpos, ypos);

  // background ellipse
  CTPP::CDT background_ellipse_cdt(CTPP::CDT::HASH_VAL);
  background_ellipse_cdt["start"] = "<ellipse";
  background_ellipse_cdt["end"] = "</ellipse>";

  background_ellipse_cdt["attributes"]["cx"] = Fmi::to_string(xpos + (text_dimension.height * 0.5));
  background_ellipse_cdt["attributes"]["cy"] = Fmi::to_string(ypos - (text_dimension.height * 0.5));
  background_ellipse_cdt["attributes"]["rx"] = Fmi::to_string(text_dimension.width * 0.75);
  background_ellipse_cdt["attributes"]["ry"] = Fmi::to_string(text_dimension.height);
  theState.addAttributes(theGlobals, background_ellipse_cdt, theFilter.attributes);
  theLayersCdt.PushBack(background_ellipse_cdt);

  // mean temperature text
  CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
  text_cdt["start"] = "<text";
  text_cdt["end"] = "</text>";
  text_cdt["cdata"] = mean_temperature;

  theState.addAttributes(theGlobals, text_cdt, theFilter.text_attributes);

  text_cdt["attributes"]["x"] = Fmi::to_string(xpos);
  text_cdt["attributes"]["y"] = Fmi::to_string(ypos);

  theLayersCdt.PushBack(text_cdt);
}

// Pekko Ilvessalo 20151106: Labelin sijainti
// (vasen/ylävasen/ylä/yläoikea/oikea/alaoikea/ala/alavasen) (0,6,1,7,2,4,3,5)
void IceMapLayer::addLocationName(double theXPos,
                                  double theYPos,
                                  const std::string& theFirstName,
                                  const std::string& theSecondName,
                                  int thePosition,
                                  int theArrowAngle,
                                  const PostGISLayerFilter& theFilter,
                                  CTPP::CDT& theGlobals,
                                  CTPP::CDT& theLayersCdt,
                                  CTPP::CDT& theGroupCdt,
                                  State& theState) const
{
  std::string first_name(theFirstName);
  std::string second_name(theSecondName);

  boost::algorithm::trim(first_name);
  boost::algorithm::trim(second_name);

  text_style_t text_style = getTextStyle(theFilter.text_attributes, text_style_t());
  text_dimension_t text_dimension_first = getTextDimension(first_name, text_style);
  text_dimension_t text_dimension_second = getTextDimension(second_name, text_style);

  // in database there seems to be '<Null>'-strings
  boost::replace_all(second_name, "<Null>", "");
  boost::replace_all(first_name, "<", "&#60;");
  boost::replace_all(first_name, ">", "&#62;");
  boost::replace_all(second_name, "<", "&#60;");
  boost::replace_all(second_name, ">", "&#62;");

  double x_coord(-1.0);
  double y_coord_first(-1.0);
  double y_coord_second(-1.0);

  unsigned int x_offset(text_dimension_first.width >= text_dimension_second.width
                            ? text_dimension_first.width
                            : text_dimension_second.width);
  unsigned int y_offset(text_dimension_first.height >= text_dimension_second.height
                            ? text_dimension_first.height
                            : text_dimension_second.height);

  switch (thePosition)
  {
    case 0:  // vasen
    {
      x_coord = theXPos - x_offset - 5;
      y_coord_first = theYPos - (y_offset * 0.53);
      y_coord_second = theYPos + (y_offset * 0.53);

      if (first_name.empty())
        y_coord_second = theYPos + (text_dimension_first.height * 0.1);
      if (second_name.empty())
        y_coord_first = theYPos + (text_dimension_first.height * 0.1);
    }
    break;
    case 1:  // oikea
    {
      x_coord = theXPos + 5;
      y_coord_first = theYPos - (y_offset * 0.53);
      y_coord_second = theYPos + (y_offset * 0.53);

      if (first_name.empty())
        y_coord_second = theYPos + (text_dimension_first.height * 0.1);
      if (second_name.empty())
        y_coord_first = theYPos + (text_dimension_first.height * 0.1);
    }
    break;
    case 2:  // ylä
    {
      x_coord = theXPos - (x_offset * 0.5);
      y_coord_first = theYPos - (y_offset * 2.0);
      y_coord_second = theYPos - y_offset;

      if (second_name.empty())
        y_coord_first = y_coord_second;
    }
    break;
    case 3:  // ala
    {
      x_coord = theXPos - (x_offset * 0.5);
      y_coord_first = theYPos + 2.0 + y_offset;
      y_coord_second = theYPos + 2.0 + (y_offset * 2.0);

      if (first_name.empty())
        y_coord_second = y_coord_first;
    }
    break;
    case 4:  // alaoikea
    {
      x_coord = theXPos + 3.0;
      y_coord_first = theYPos + y_offset;
      y_coord_second = theYPos + (y_offset * 2.0);

      if (first_name.empty())
        y_coord_second = y_coord_first;
    }
    break;
    case 5:  // alavasen
    {
      x_coord = theXPos - x_offset;
      y_coord_first = theYPos + y_offset;
      y_coord_second = theYPos + (y_offset * 2.0);

      if (first_name.empty())
        y_coord_second = y_coord_first;
    }
    break;
    case 6:  // ylävasen
    {
      x_coord = theXPos - x_offset;
      y_coord_first = theYPos - (y_offset * 2.0);
      y_coord_second = theYPos - y_offset;

      if (second_name.empty())
        y_coord_first = y_coord_second;
    }
    break;
    case 7:  // yläoikea
    {
      x_coord = theXPos + 3;
      y_coord_first = theYPos - (y_offset * 2.0);
      y_coord_second = theYPos - y_offset;

      if (second_name.empty())
        y_coord_first = y_coord_second;
    }
    break;
    case 8:  // dont add name
      break;
  }

  if (x_coord < 0)
    return;

  if (!first_name.empty())
  {
    CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
    text_cdt["start"] = "<text";
    text_cdt["end"] = "</text>";
    text_cdt["cdata"] = first_name;
    text_cdt["attributes"]["x"] = Fmi::to_string(std::round(x_coord));
    text_cdt["attributes"]["y"] = Fmi::to_string(std::round(y_coord_first));
    theState.addAttributes(theGlobals, text_cdt, theFilter.attributes);
    theLayersCdt.PushBack(text_cdt);
  }

  // second name
  if (!second_name.empty())
  {
    CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
    text_cdt["start"] = "<text";
    text_cdt["end"] = "</text>";
    text_cdt["cdata"] = second_name;
    text_cdt["attributes"]["x"] = Fmi::to_string(std::round(x_coord));
    text_cdt["attributes"]["y"] = Fmi::to_string(std::round(y_coord_second));
    theState.addAttributes(theGlobals, text_cdt, theFilter.attributes);
    theLayersCdt.PushBack(text_cdt);
  }

  // in some cases arrow is added next to location name to indicate the actual location
  if (theArrowAngle == -1 || first_name.empty())
    return;

  int arrow_x_coord(
      theArrowAngle >= 90 && theArrowAngle <= 270 ? x_coord : x_coord + text_dimension_first.width);
  CTPP::CDT arrow_cdt(CTPP::CDT::HASH_VAL);
  arrow_cdt["start"] = "<line";
  arrow_cdt["end"] = "</line>";
  arrow_cdt["attributes"]["x1"] = Fmi::to_string(std::round(arrow_x_coord));
  arrow_cdt["attributes"]["y1"] = Fmi::to_string(std::round(y_coord_first - 2));
  arrow_cdt["attributes"]["x2"] = Fmi::to_string(
      std::round(arrow_x_coord + ((text_dimension_first.width / first_name.size()) * 2)));
  arrow_cdt["attributes"]["y2"] = Fmi::to_string(std::round(y_coord_first - 2));
  arrow_cdt["attributes"]["stroke"] = "black";
  arrow_cdt["attributes"]["stroke-width"] = "0.5";
  arrow_cdt["attributes"]["marker-end"] = "url(#spearhead)";
  // add arrow next to the location name
  arrow_cdt["attributes"]["transform"] =
      ("rotate(" + Fmi::to_string(theArrowAngle) + " " + Fmi::to_string(std::round(arrow_x_coord)) +
       " " + Fmi::to_string(std::round(y_coord_first - 2)) + ") ");

  theState.addAttributes(theGlobals, arrow_cdt, theFilter.attributes);
  theLayersCdt.PushBack(arrow_cdt);
}

void IceMapLayer::handleGeometry(const Fmi::Feature& theResultItem,
                                 const PostGISLayerFilter& theFilter,
                                 unsigned int& theMapId,
                                 CTPP::CDT& theGlobals,
                                 CTPP::CDT& theGroupCdt,
                                 State& theState) const
{
  if (!theResultItem.geom || theResultItem.geom->IsEmpty())
    return;

  const auto box = projection.getBox();
  const auto crs = projection.getCRS();

  // Store the path with unique ID
  std::string iri = (qid + Fmi::to_string(theMapId++));

  CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
  map_cdt["iri"] = iri;
  map_cdt["type"] = Geometry::name(*theResultItem.geom, theState.getType());
  map_cdt["layertype"] = "icemap";
  map_cdt["data"] = Geometry::toString(*theResultItem.geom, theState.getType(), box, crs);
  theGlobals["paths"][iri] = map_cdt;

  // add pattern on geometry
  if (itsParameters.find("pattern") != itsParameters.end())
  {
    std::string pattern_iri(itsParameters.at("pattern"));
    if (theState.addId(pattern_iri))
      theGlobals["includes"][pattern_iri] = theState.getPattern(pattern_iri);
  }

  if (!theState.inDefs())
  {
    // Add the SVG use element
    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";

    theState.addAttributes(theGlobals, tag_cdt, theFilter.attributes);
    tag_cdt["attributes"]["xlink:href"] = "#" + iri;
    theGroupCdt["tags"].PushBack(tag_cdt);
  }
}

void IceMapLayer::handleResultItem(const Fmi::Feature& theResultItem,
                                   const PostGISLayerFilter& theFilter,
                                   unsigned int& theMapId,
                                   CTPP::CDT& theGlobals,
                                   CTPP::CDT& theLayersCdt,
                                   CTPP::CDT& theGroupCdt,
                                   State& theState) const
{
  std::string layer_subtype(getParameterValue("layer_subtype"));

  if (layer_subtype.find("label") == layer_subtype.size() - 5)
  {
    handleLabel(theResultItem, theFilter, theGlobals, theLayersCdt, theState);
  }
  else if (layer_subtype == "symbol")
  {
    handleSymbol(theResultItem, theGroupCdt);
  }
  else if (layer_subtype == "named_location")
  {
    handleNamedLocation(theResultItem, theFilter, theGlobals, theLayersCdt, theGroupCdt, theState);
  }
  else if (layer_subtype == "mean_temperature")
  {
    handleMeanTemperature(theResultItem, theFilter, theGlobals, theLayersCdt, theState);
  }
  else if (layer_subtype == "degree_of_pressure")
  {
    handleSymbol(theResultItem, theGroupCdt);
    handleLabel(theResultItem, theFilter, theGlobals, theLayersCdt, theState);
  }
  else
  {
    handleGeometry(theResultItem, theFilter, theMapId, theGlobals, theGroupCdt, theState);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
