//======================================================================

#include "IceMapLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "LonLatToXYTransformation.h"
#include "State.h"
#include "TextTable.h"
#include "TextUtility.h"
#include "XYTransformation.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <gis/Box.h>
#include <gis/Host.h>
#include <gis/OGR.h>
#include <gis/PostGIS.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Json.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <ogr_geometry.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
Json::CharReaderBuilder charreaderbuilder;

const std::array<std::string, 10> attribute_columns = {"firstname_column",
                                                       "secondname_column",
                                                       "nameposition_column",
                                                       "text_column",
                                                       "textposition_column",
                                                       "angle_column",
                                                       "labeltext_column",
                                                       "fontname_column",
                                                       "fontsize_column",
                                                       "textstring"};

class PostGISAttributeToString : public boost::static_visitor<std::string>
{
 public:
  std::string operator()(int i) const { return Fmi::to_string(i); }
  std::string operator()(double d) const { return Fmi::to_string(d); }
  std::string operator()(const Fmi::DateTime& t) const { return Fmi::to_iso_string(t); }
  std::string operator()(const std::string& str) const
  {
    std::string ret(str);
    boost::algorithm::trim(ret);
    return ret;
  }
};

std::string getMeanTemperatureColumnName(const Fmi::DateTime& t)
{
  Fmi::TimePeriod summer_period(Fmi::DateTime(Fmi::Date(t.date().year(), boost::gregorian::Mar, 20),
                                              Fmi::TimeDuration(0, 0, 0)),
                                Fmi::DateTime(Fmi::Date(t.date().year(), boost::gregorian::Oct, 20),
                                              Fmi::TimeDuration(23, 59, 59)));

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

Json::Value getJsonValue(const std::string& param_name,
                         const std::map<std::string, std::string>& parameters)
{
  Json::Value ret;
  if (parameters.find(param_name) != parameters.end())
  {
    const auto& str = parameters.at(param_name);

    std::unique_ptr<Json::CharReader> reader(charreaderbuilder.newCharReader());
    std::string errors;
    if (!reader->parse(str.c_str(), str.c_str() + str.size(), &ret, &errors))
      throw Fmi::Exception(BCP, "Failed to parse JSON value").addParameter("Message", errors);
  }

  return ret;
}

}  // anonymous namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void IceMapLayer::init(Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "PostGIS JSON is not a JSON object");

    PostGISLayerBase::init(theJson, theState, theConfig, theProperties);

    // layer_subtype specifies the final layer type, e.g. label, symbol, named_location, ...
    std::string subtype;
    JsonTools::remove_string(subtype, theJson, "layer_subtype");
    if (!subtype.empty())
      itsParameters.insert({"layer_subtype", subtype});

    // name of symbol file
    std::string symbol;
    JsonTools::remove_string(symbol, theJson, "symbol");
    if (!symbol.empty())
      itsParameters.insert({"symbol", symbol});

    // column name of first name (e.g. 'Helsinki')
    std::string firstname_column;
    JsonTools::remove_string(firstname_column, theJson, "firstname_column");
    if (!firstname_column.empty())
      itsParameters.insert({"firstname_column", firstname_column});

    // column name of second name (e.g. 'Helsingfors')
    std::string secondname_column;
    JsonTools::remove_string(secondname_column, theJson, "secondname_column");
    if (!secondname_column.empty())
      itsParameters.insert({"secondname_column", secondname_column});

    // name position
    std::string nameposition_column;
    JsonTools::remove_string(nameposition_column, theJson, "nameposition_column");
    if (!nameposition_column.empty())
      itsParameters.insert({"nameposition_column", nameposition_column});

    // column name of text
    std::string text_column;
    JsonTools::remove_string(text_column, theJson, "text_column");
    if (!text_column.empty())
      itsParameters.insert({"text_column", text_column});

    // text position
    std::string textposition_column;
    JsonTools::remove_string(textposition_column, theJson, "textposition_column");
    if (!textposition_column.empty())
      itsParameters.insert({"textposition_column", textposition_column});

    // angle column
    std::string angle_column;
    JsonTools::remove_string(angle_column, theJson, "angle_column");
    if (!angle_column.empty())
      itsParameters.insert({"angle_column", angle_column});

    // name of pattern file
    std::string pattern;
    JsonTools::remove_string(pattern, theJson, "pattern");
    if (!pattern.empty())
      itsParameters.insert({"pattern", pattern});

    // labeltext_column is one of attribute colmuns
    std::string labeltext_column;
    JsonTools::remove_string(labeltext_column, theJson, "labeltext_column");
    if (!labeltext_column.empty())
      itsParameters.insert({"labeltext_column", labeltext_column});

    // list of all attribute columns
    std::string attribute_columns;
    JsonTools::remove_string(attribute_columns, theJson, "attribute_columns");
    if (!attribute_columns.empty())
      itsParameters.insert({"attribute_columns", attribute_columns});

    // fontname column of the label
    std::string fontname_column;
    JsonTools::remove_string(fontname_column, theJson, "fontname_column");
    if (!fontname_column.empty())
      itsParameters.insert({"fontname_column", fontname_column});

    // fontsize column of the label
    std::string fontsize_column;
    JsonTools::remove_string(fontsize_column, theJson, "fontsize_column");
    if (!fontsize_column.empty())
      itsParameters.insert({"fontsize_column", fontsize_column});

    // longitude, latitude
    std::string longitude;
    JsonTools::remove_string(longitude, theJson, "longitude");
    if (!longitude.empty())
      itsParameters.insert({"longitude", longitude});

    // longitude, latitude
    std::string latitude;
    JsonTools::remove_string(latitude, theJson, "latitude");
    if (!latitude.empty())
      itsParameters.insert({"latitude", latitude});

    // if time missing set current time
    if (!hasValidTime())
      setValidTime(Fmi::SecondClock::universal_time());

    // table attributes
    auto json = JsonTools::remove(theJson, "table_attributes");
    if (!json.isNull())
      itsParameters.insert({"table_attributes", json.toStyledString()});

    // table data
    json = JsonTools::remove(theJson, "table_data");
    if (!json.isNull())
      itsParameters.insert({"table_data", json.toStyledString()});

    // additional columns
    std::string additional_columns;
    JsonTools::remove_string(additional_columns, theJson, "additional_columns");
    if (!additional_columns.empty())
    {
      std::vector<std::string> columns;
      boost::algorithm::split(columns, additional_columns, boost::algorithm::is_any_of(","));
      for (const auto& col : columns)
        itsParameters.insert({col, col});
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "IceMapLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    // Get projection details
    if (!projection.crs)
      throw std::runtime_error("IceMapLayer projection not set");

    auto& includes = theGlobals["includes"];

    std::vector<std::string> markers{"spearhead"};

    std::vector<std::string> symbols{"strips_and_patches",
                                     "jmdbrr",
                                     "icelea",
                                     "icefra",
                                     "icefra_zone",
                                     "flobrg",
                                     "ice_edge",
                                     "estimated_ice_edge",
                                     "icedft",
                                     "icerdg",
                                     "icerft",
                                     "sea_surface_mean_temperature",
                                     "icecom",
                                     "watertemperature_isotherm",
                                     "ice_thickness",
                                     "lighthouse"};

    std::vector<std::string> patterns{"close_ice",
                                      "consolidated_ice",
                                      "fast_ice",
                                      "grey_ice",
                                      "new_ice",
                                      "open_ice",
                                      "open_water",
                                      "rotten_fast_ice",
                                      "very_close_ice",
                                      "very_open_ice"};

    for (const auto& marker : markers)
      if (theState.addId(marker))
        includes[marker] = theState.getMarker(marker);

    for (const auto& symbol : symbols)
      if (theState.addId(symbol))
        includes[symbol] = theState.getSymbol(symbol);

    for (const auto& pattern : patterns)
      if (theState.addId(pattern))
        includes[pattern] = theState.getPattern(pattern);

    // const auto& crs = projection.getCRS();

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

    const auto& projectionSR = projection.getCRS();

    // Default is World Mercator used in marine maps in case the spatial reference is missing from
    // the database
    Fmi::SpatialReference defaultSR("EPSG:3395");

    unsigned int mapid = 1;  // id to concatenate to iri to make it unique

    // Get the polygons and store them into the template engine
    for (const PostGISLayerFilter& filter : filters)
    {
      if (time_condition && filter.where)
        mapOptions.where = (*filter.where + " AND " + *time_condition);
      else if (time_condition)
        mapOptions.where = time_condition;
      else if (filter.where)
        mapOptions.where = filter.where;

      std::vector<std::string> attribute_column_names = getAttributeColumns();

      std::string layer_subtype = getParameterValue("layer_subtype");

      // mean temperature needs a column name (requested time must be between 21.10-21.3)
      if (attribute_column_names.empty() && layer_subtype == "mean_temperature")
        return;

      mapOptions.fieldnames.insert(attribute_column_names.begin(), attribute_column_names.end());

      Fmi::Features result_set = getFeatures(theState, projectionSR, mapOptions);

      for (const auto& result_item : result_set)
      {
        if (result_item->geom && result_item->geom->IsEmpty() == 0)
        {
          const OGRSpatialReference* sr = result_item->geom->getSpatialReference();

          if (sr == nullptr)
          {
            result_item->geom->assignSpatialReference(defaultSR.get());
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    auto hash = PostGISLayerBase::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(itsParameters));
    Fmi::hash_combine(hash, Dali::hash_value(filters, theState));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void IceMapLayer::handleSymbol(const Fmi::Feature& theResultItem,
                               CTPP::CDT& theGroupCdt,
                               const State& /* theState */) const
{
  auto transformation = XYTransformation(projection);

  std::string iri = getParameterValue("symbol");

  if (theResultItem.geom && theResultItem.geom->IsEmpty() == 0)
  {
    // envelope should work for point and areas
    OGREnvelope envelope;
    theResultItem.geom->getEnvelope(&envelope);

    double x = ((envelope.MinX + envelope.MaxX) / 2);
    double y = ((envelope.MinY + envelope.MaxY) / 2);

    transformation.transform(x, y);

    x -= 15;
    y -= 5;

    // Start generating the hash
    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";
    tag_cdt["attributes"]["xlink:href"] = "#" + iri;
    tag_cdt["attributes"]["x"] = Fmi::to_string(lround(x));
    tag_cdt["attributes"]["y"] = Fmi::to_string(lround(y));
    theGroupCdt["tags"].PushBack(tag_cdt);
  }
}

void IceMapLayer::handleTextField(const Fmi::Feature& theResultItem,
                                  const PostGISLayerFilter& theFilter,
                                  CTPP::CDT& theGlobals,
                                  CTPP::CDT& theLayersCdt,
                                  CTPP::CDT& /* theGroupCdt */,
                                  const State& theState) const
{
  // text field
  std::string text;

  if (itsParameters.find("text_column") != itsParameters.end())
  {
    std::string text_column = itsParameters.at("text_column");

    if (theResultItem.attributes.find(text_column) != theResultItem.attributes.end())
      text = std::visit(PostGISAttributeToString(), theResultItem.attributes.at(text_column));
  }
  else
  {
    throw Fmi::Exception(BCP, "Error: text_column must be defined for PostGIS-based text field!");
  }

  std::vector<std::string> rows;
  boost::algorithm::split(rows, text, boost::algorithm::is_any_of("#"));

  double xpos = Fmi::stod(itsParameters.at("longitude"));
  double ypos = Fmi::stod(itsParameters.at("latitude"));
  auto transformation = LonLatToXYTransformation(projection);
  if (!transformation.transform(xpos, ypos))
    return;

  addTextField(xpos, ypos, rows, theFilter.attributes, theGlobals, theLayersCdt, theState);
}

void IceMapLayer::handleNamedLocation(const Fmi::Feature& theResultItem,
                                      const PostGISLayerFilter& theFilter,
                                      CTPP::CDT& theGlobals,
                                      CTPP::CDT& theLayersCdt,
                                      CTPP::CDT& theGroupCdt,
                                      State& theState) const
{
  auto transformation = XYTransformation(projection);

  if (theResultItem.geom && theResultItem.geom->IsEmpty() == 0)
  {
    const auto* point = dynamic_cast<const OGRPoint*>(theResultItem.geom.get());

    double x = point->getX();
    double y = point->getY();

    transformation.transform(x, y);

    // Start generating the hash
    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<use";
    tag_cdt["end"] = "/>";

    std::string iri = getParameterValue("symbol");

    if (!iri.empty())
    {
      if (theState.addId(iri))
        theGlobals["includes"][iri] = theState.getSymbol(iri);
      tag_cdt["attributes"]["xlink:href"] = "#" + iri;
      tag_cdt["attributes"]["x"] = Fmi::to_string(x);
      tag_cdt["attributes"]["y"] = Fmi::to_string(y);
      theGroupCdt["tags"].PushBack(tag_cdt);
    }
    // first name second name
    std::string first_name;
    std::string second_name;
    std::string name_position = "label_location";
    if (itsParameters.find("firstname_column") != itsParameters.end())
      first_name = std::visit(PostGISAttributeToString(),
                              theResultItem.attributes.at(itsParameters.at("firstname_column")));

    if (itsParameters.find("secondname_column") != itsParameters.end())
      second_name = std::visit(PostGISAttributeToString(),
                               theResultItem.attributes.at(itsParameters.at("secondname_column")));

    if (itsParameters.find("nameposition_column") != itsParameters.end())
      name_position =
          std::visit(PostGISAttributeToString(),
                     theResultItem.attributes.at(itsParameters.at("nameposition_column")));

    if (name_position.empty() ||
        !(std::all_of(name_position.begin(), name_position.end(), ::isdigit)))
      name_position = "2";  // by default name is positioned on the right side of the symbol

    // we may add arrow beside the name
    std::string arrow_angle;
    if (itsParameters.find("angle_column") != itsParameters.end())
    {
      std::string angle_column = itsParameters.at("angle_column");
      if (theResultItem.attributes.find(angle_column) != theResultItem.attributes.end())
      {
        arrow_angle = std::visit(PostGISAttributeToString(),
                                 theResultItem.attributes.at(itsParameters.at("angle_column")));
      }
    }

    boost::algorithm::trim(arrow_angle);

    if (arrow_angle.empty())
      arrow_angle = "-1";

    addLocationName(x,
                    y,
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
                              const State& theState) const
{
  std::vector<std::string> attribute_columns = getAttributeColumns();

  if (attribute_columns.empty())
    return;

  std::string label_text;
  text_style_t text_style;

  // default name of text column for a label is 'textstring'
  std::string labeltext_column = "textstring";
  std::string fontname_column = "fontname";
  std::string fontsize_column = "fontsize";
  if (itsParameters.find("labeltext_column") != itsParameters.end())
    labeltext_column = itsParameters.at("labeltext_column");
  if (theResultItem.attributes.find(labeltext_column) != theResultItem.attributes.end())
    label_text =
        std::visit(PostGISAttributeToString(), theResultItem.attributes.at(labeltext_column));

  boost::replace_all(label_text, "<", "&#60;");
  boost::replace_all(label_text, ">", "&#62;");

  if (itsParameters.find("fontname_column") != itsParameters.end())
    fontname_column = itsParameters.at("fontname_column");
  if (theResultItem.attributes.find(labeltext_column) != theResultItem.attributes.end())
    text_style.fontfamily =
        std::visit(PostGISAttributeToString(), theResultItem.attributes.at(fontname_column));

  if (itsParameters.find("fontsize_column") != itsParameters.end())
    fontsize_column = itsParameters.at("fontsize_column");
  if (theResultItem.attributes.find(labeltext_column) != theResultItem.attributes.end())
    text_style.fontsize =
        std::visit(PostGISAttributeToString(), theResultItem.attributes.at(fontsize_column));

  // erase decimal part from fontsize
  if (text_style.fontsize.find('.') != std::string::npos)
    text_style.fontsize.erase(text_style.fontsize.find('.'));

  text_style = getTextStyle(theFilter.text_attributes, text_style);

  std::vector<std::string> rows;
  boost::algorithm::split(rows, label_text, boost::algorithm::is_any_of("\n"));

  text_dimension_t text_dimension = getTextDimension(label_text, text_style);

  OGREnvelope envelope;
  theResultItem.geom->getEnvelope(&envelope);

  double xpos = envelope.MinX;
  double ypos = envelope.MaxY;

  auto transformation = XYTransformation(projection);
  transformation.transform(xpos, ypos);

  ypos += (text_dimension.height + 5);

  Attributes text_attributes;
  if (!theFilter.text_attributes.empty())
    text_attributes = theFilter.text_attributes;
  else
  {
    text_attributes.add("font-family", text_style.fontfamily);
    text_attributes.add("font-size", text_style.fontsize);
    text_attributes.add("font-style", text_style.fontweight);
    text_attributes.add("font-weight", text_style.fontweight);
    text_attributes.add("text-anchor", text_style.textanchor);
  }

  // background rectangle
  if (!theFilter.attributes.empty())
  {
    std::string dxValue = text_attributes.value("dx");
    std::string dyValue = text_attributes.value("dy");
    int dx = (!dxValue.empty() ? Fmi::stoi(dxValue) : 0);
    int dy = (!dyValue.empty() ? Fmi::stoi(dyValue) : 0);

    text_dimension_t rectangleDimension = getTextDimension(rows, text_style);

    CTPP::CDT background_rect_cdt(CTPP::CDT::HASH_VAL);
    background_rect_cdt["start"] = "<rect";
    background_rect_cdt["end"] = "</rect>";
    background_rect_cdt["attributes"]["width"] = Fmi::to_string(rectangleDimension.width);
    background_rect_cdt["attributes"]["height"] = Fmi::to_string(rectangleDimension.height);
    background_rect_cdt["attributes"]["x"] = Fmi::to_string(xpos + dx);
    background_rect_cdt["attributes"]["y"] = Fmi::to_string(ypos - text_dimension.height + dy);

    theState.addAttributes(theGlobals, background_rect_cdt, theFilter.attributes);
    theLayersCdt.PushBack(background_rect_cdt);
  }

  addTextField(xpos, ypos, rows, text_attributes, theGlobals, theLayersCdt, theState);
}

void IceMapLayer::handleMeanTemperature(const Fmi::Feature& theResultItem,
                                        const PostGISLayerFilter& theFilter,
                                        CTPP::CDT& theGlobals,
                                        CTPP::CDT& theLayersCdt,
                                        State& theState) const
{
  std::string col_name = getMeanTemperatureColumnName(getValidTime());

  // mean temperature
  std::string mean_temperature =
      std::visit(PostGISAttributeToString(), theResultItem.attributes.at(col_name));

  if (mean_temperature.empty())
    return;

  if (isdigit(mean_temperature.at(0)))
  {
    if (mean_temperature.size() == 1)
      mean_temperature += ".0";
    mean_temperature.append("°");
  }

  text_style_t text_style = getTextStyle(theFilter.text_attributes, text_style_t());
  text_dimension_t text_dimension = getTextDimension(mean_temperature, text_style);

  // position of the geometry mean temperature
  const auto* point = dynamic_cast<const OGRPoint*>(theResultItem.geom.get());

  double xpos = point->getX();
  double ypos = point->getY();

  auto transformation = XYTransformation(projection);
  transformation.transform(xpos, ypos);

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

void IceMapLayer::handleTrafficRestrictions(const Fmi::Feature& /* theResultItem */,
                                            const PostGISLayerFilter& theFilter,
                                            CTPP::CDT& theGlobals,
                                            CTPP::CDT& theLayersCdt,
                                            const State& theState) const
{
  double xpos = Fmi::stod(itsParameters.at("longitude"));
  double ypos = Fmi::stod(itsParameters.at("latitude"));
  auto transformation = LonLatToXYTransformation(projection);

  if (!transformation.transform(xpos, ypos))
    return;

  auto jsonTableAttributes = getJsonValue("table_attributes", itsParameters);

  TextTable mapTable(jsonTableAttributes, lround(xpos), lround(ypos));

  auto tableDataJson = getJsonValue("table_data", itsParameters);

  // add content from configuration file
  mapTable.addContent(tableDataJson);

  mapTable.makeTable(
      theFilter.attributes, theFilter.text_attributes, theGlobals, theLayersCdt, theState);
}

void IceMapLayer::handleIceEgg(const Fmi::Feature& theResultItem,
                               const PostGISLayerFilter& theFilter,
                               unsigned int& theMapId,
                               CTPP::CDT& theGlobals,
                               CTPP::CDT& theGroupCdt,
                               CTPP::CDT& theLayersCdt,
                               const State& theState) const
{
  if (!theResultItem.geom || theResultItem.geom->IsEmpty() != 0)
    return;

  // We get rectangle from database, but need to show egg-shaped ellipse
  OGREnvelope envelope;
  theResultItem.geom->getEnvelope(&envelope);
  double xLeft = envelope.MinX;
  double yBottom = envelope.MinY;
  double xRight = envelope.MaxX;
  double yTop = envelope.MaxY;

  double xMiddle = fabs(xLeft + xRight) * 0.5;
  double yMiddle = fabs(yBottom + yTop) * 0.5;

  // We draw a bounding ellipse for the original rectangle,
  // so we need a new bounding box limits for ellipse
  double width = xRight - xLeft;
  double height = yTop - yBottom;
  xRight = xMiddle + (width / sqrt(2.0));
  xLeft = xMiddle - (width / sqrt(2.0));
  yTop = yMiddle + (height / sqrt(2.0));
  yBottom = yMiddle - (height / sqrt(2.0));

  double rx = fabs(xRight - xLeft) * 0.4;
  double ry = fabs(yTop - yBottom) * 0.5;

  // Ellipse points
  Fmi::OGR::CoordinatePoints points;
  double angle = 0;
  while (angle < 360.0)
  {
    double rad = angle / 57.2957795;
    double x = xMiddle + rx * cos(rad);
    double y = yMiddle + ry * sin(rad);
    points.push_back(std::pair<double, double>(x, y));
    angle += 2.0;
  }

  // In polygon last point is same as first one
  points.push_back(points.front());

  std::string wktString = "POLYGON((";
  for (const auto& p : points)
  {
    if (wktString.length() > 9)
      wktString += ", ";
    wktString += Fmi::to_string(p.first) + " " + Fmi::to_string(p.second);
  }
  wktString += "))";

  std::unique_ptr<OGRGeometry> ellipse;
  ellipse.reset(Fmi::OGR::createFromWkt(wktString));

  // Horizontal lines of iceegg
  double firstHorizontalY = yBottom + (fabs(yTop - yBottom) * 0.34);
  double secondHorizontalY = yBottom + (fabs(yTop - yBottom) * 0.55);
  double thirdHorizontalY = yBottom + (fabs(yTop - yBottom) * 0.76);
  double zeroHorizontalY = yBottom + (fabs(yTop - yBottom) * 0.13);

  wktString = "MULTILINESTRING((" + Fmi::to_string(xLeft) + " " + Fmi::to_string(firstHorizontalY) +
              ", " + Fmi::to_string(xRight) + " " + Fmi::to_string(firstHorizontalY) + "),";
  wktString += "(" + Fmi::to_string(xLeft) + " " + Fmi::to_string(secondHorizontalY) + ", " +
               Fmi::to_string(xRight) + " " + Fmi::to_string(secondHorizontalY) + "),";
  wktString += "(" + Fmi::to_string(xLeft) + " " + Fmi::to_string(thirdHorizontalY) + ", " +
               Fmi::to_string(xRight) + " " + Fmi::to_string(thirdHorizontalY) + "))";

  std::unique_ptr<OGRGeometry> horizontalLines;

  horizontalLines.reset(Fmi::OGR::createFromWkt(wktString));
  horizontalLines.reset(horizontalLines->Intersection(ellipse.get()));

  OGRGeometryCollection collection;
  collection.addGeometry(ellipse.get());
  collection.addGeometry(horizontalLines.get());

  Fmi::Feature feat;
  feat.geom.reset(collection.clone());
  handleGeometry(feat, theFilter, theMapId, theGlobals, theGroupCdt, theState);

  // Then add the content to the egg
  std::string egg_text;
  if (theResultItem.attributes.find("textstring") != theResultItem.attributes.end())
    egg_text = std::visit(PostGISAttributeToString(), theResultItem.attributes.at("textstring"));

  std::vector<std::string> rows;
  boost::algorithm::split(rows, egg_text, boost::algorithm::is_any_of("\n"));
  // Remove leading and trailing spaces
  std::for_each(rows.begin(), rows.end(), [](std::string& str) { boost::trim(str); });

  for (std::string& r : rows)
    if (!r.empty() && r.back() == '+')
      r.insert(0, " ");

  text_style_t text_style;
  text_style = getTextStyle(theFilter.text_attributes, text_style);
  text_dimension_t text_dimension = getTextDimension("1234567890", text_style);
  auto transformation = LonLatToXYTransformation(projection);

  // Add row by row to get more precise y-position inside the egg
  for (unsigned int i = 0; i < rows.size(); i++)
  {
    const std::string& r = rows[i];
    if (r.empty())
      continue;
    std::vector<std::string> rows2;
    rows2.push_back(r);
    double ypos = 0.0;
    double xpos = xMiddle;
    if (i == 0)
      ypos = thirdHorizontalY;
    else if (i == 1)
      ypos = secondHorizontalY;
    else if (i == 2)
      ypos = firstHorizontalY;
    else
      ypos = zeroHorizontalY;
    if (!transformation.transform(xpos, ypos))
      continue;
    ypos += (text_dimension.height / 10.0);
    addTextField(xpos, ypos, rows2, theFilter.text_attributes, theGlobals, theLayersCdt, theState);
  }
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
                                  CTPP::CDT& /* theGroupCdt */,
                                  const State& theState)
{
  std::string first_name = theFirstName;
  std::string second_name = theSecondName;

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

  double x_coord = -1.0;
  double y_coord_first = -1.0;
  double y_coord_second = -1.0;

  unsigned int x_offset =
      (text_dimension_first.width >= text_dimension_second.width ? text_dimension_first.width
                                                                 : text_dimension_second.width);
  unsigned int y_offset =
      (text_dimension_first.height >= text_dimension_second.height ? text_dimension_first.height
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
      y_coord_first = theYPos - (y_offset * 0.53) + 2;
      y_coord_second = theYPos + (y_offset * 0.53) + 2;

      if (first_name.empty())
        y_coord_second = theYPos + (text_dimension_first.height * 0.3);
      if (second_name.empty())
        y_coord_first = theYPos + (text_dimension_first.height * 0.3);
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
      y_coord_first = theYPos + (y_offset * 1.6);
      y_coord_second = y_coord_first + y_offset;

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
    text_cdt["attributes"]["x"] = Fmi::to_string(lround(x_coord));
    text_cdt["attributes"]["y"] = Fmi::to_string(lround(y_coord_first));
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
    text_cdt["attributes"]["x"] = Fmi::to_string(lround(x_coord));
    text_cdt["attributes"]["y"] = Fmi::to_string(lround(y_coord_second));
    theState.addAttributes(theGlobals, text_cdt, theFilter.attributes);
    theLayersCdt.PushBack(text_cdt);
  }

  // in some cases arrow is added next to location name to indicate the actual location
  if (theArrowAngle == -1 || first_name.empty())
    return;

  int arrow_x_coord = lround(
      theArrowAngle >= 90 && theArrowAngle <= 270 ? x_coord : x_coord + text_dimension_first.width);

  CTPP::CDT arrow_cdt(CTPP::CDT::HASH_VAL);
  arrow_cdt["start"] = "<line";
  arrow_cdt["end"] = "</line>";
  arrow_cdt["attributes"]["x1"] = Fmi::to_string(lround(arrow_x_coord));
  arrow_cdt["attributes"]["y1"] = Fmi::to_string(lround(y_coord_first - 2));
  arrow_cdt["attributes"]["x2"] = Fmi::to_string(
      lround(arrow_x_coord + ((1.0 * text_dimension_first.width / first_name.size()) * 2)));
  arrow_cdt["attributes"]["y2"] = Fmi::to_string(lround(y_coord_first - 2));
  arrow_cdt["attributes"]["stroke"] = "black";
  arrow_cdt["attributes"]["stroke-width"] = "0.5";
  arrow_cdt["attributes"]["marker-end"] = "url(#spearhead)";
  // add arrow next to the location name
  arrow_cdt["attributes"]["transform"] =
      ("rotate(" + Fmi::to_string(theArrowAngle) + " " + Fmi::to_string(lround(arrow_x_coord)) +
       " " + Fmi::to_string(lround(y_coord_first - 2)) + ") ");

  theState.addAttributes(theGlobals, arrow_cdt, theFilter.attributes);
  theLayersCdt.PushBack(arrow_cdt);
}

void IceMapLayer::handleGeometry(const Fmi::Feature& theResultItem,
                                 const PostGISLayerFilter& theFilter,
                                 unsigned int& theMapId,
                                 CTPP::CDT& theGlobals,
                                 CTPP::CDT& theGroupCdt,
                                 const State& theState) const
{
  if (!theResultItem.geom || theResultItem.geom->IsEmpty() != 0)
    return;

  const auto& box = projection.getBox();
  const auto& crs = projection.getCRS();
  const auto clipbox = getClipBox(box);

  // Clip the data or on high zoom levels the coordinates may overflow in Cairo
  OGRGeometryPtr geom(Fmi::OGR::polyclip(*theResultItem.geom, clipbox));

  if (!geom || geom->IsEmpty() != 0)
    return;

  // Store the path with unique ID
  std::string iri = (qid + Fmi::to_string(theMapId++));

  CTPP::CDT map_cdt(CTPP::CDT::HASH_VAL);
  map_cdt["iri"] = iri;
  map_cdt["type"] = Geometry::name(*theResultItem.geom, theState.getType());
  map_cdt["layertype"] = "icemap";
  map_cdt["data"] = Geometry::toString(*geom, theState.getType(), box, crs, precision);
  theState.addPresentationAttributes(map_cdt, css, attributes, theFilter.attributes);  // NEW
  theGlobals["paths"][iri] = map_cdt;

  // add pattern on geometry
  if (itsParameters.find("pattern") != itsParameters.end())
  {
    std::string pattern_iri = itsParameters.at("pattern");
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
  std::string layer_subtype = getParameterValue("layer_subtype");

  if (layer_subtype.find("label") == layer_subtype.size() - 5)
  {
    handleLabel(theResultItem, theFilter, theGlobals, theLayersCdt, theState);
  }
  else if (layer_subtype == "symbol")
  {
    handleSymbol(theResultItem, theGroupCdt, theState);
  }
  else if (layer_subtype == "named_location")
  {
    handleNamedLocation(theResultItem, theFilter, theGlobals, theLayersCdt, theGroupCdt, theState);
  }
  else if (layer_subtype == "text_field")
  {
    handleTextField(theResultItem, theFilter, theGlobals, theLayersCdt, theGroupCdt, theState);
  }
  else if (layer_subtype == "mean_temperature")
  {
    handleMeanTemperature(theResultItem, theFilter, theGlobals, theLayersCdt, theState);
  }
  else if (layer_subtype == "traffic_restrictions")
  {
    handleTrafficRestrictions(theResultItem, theFilter, theGlobals, theLayersCdt, theState);
  }
  else if (layer_subtype == "degree_of_pressure")
  {
    handleSymbol(theResultItem, theGroupCdt, theState);
    handleLabel(theResultItem, theFilter, theGlobals, theLayersCdt, theState);
  }
  else if (layer_subtype == "ice_egg")
  {
    handleIceEgg(
        theResultItem, theFilter, theMapId, theGlobals, theGroupCdt, theLayersCdt, theState);
  }
  else
  {
    handleGeometry(theResultItem, theFilter, theMapId, theGlobals, theGroupCdt, theState);
  }
}

std::vector<std::string> IceMapLayer::getAttributeColumns() const
{
  std::vector<std::string> column_name_vector;
  for (const auto& col : attribute_columns)
  {
    if (itsParameters.find(col) == itsParameters.end())
      continue;

    std::string column_name = itsParameters.at(col);
    boost::algorithm::trim(column_name);

    // mean temperature column name is determined by given date
    if (column_name == std::string("o_DD_o_MM_o") && hasValidTime())
    {
      std::string mean_temperature_column = getMeanTemperatureColumnName(getValidTime());

      if (mean_temperature_column.empty())
      {
        column_name_vector.clear();
        return column_name_vector;
      }
      column_name_vector.push_back(getMeanTemperatureColumnName(getValidTime()));
    }
    else
      column_name_vector.push_back(column_name);
  }

  return column_name_vector;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
