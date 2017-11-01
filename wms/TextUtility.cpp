#include "TextUtility.h"
#include <gis/Box.h>
#include <macgyver/StringConversion.h>
#include <cairo.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
text_dimension_t getTextDimension(const std::string& text, const text_style_t& textStyle)
{
  cairo_font_slant_t cairo_fontstyle =
      (textStyle.fontstyle == "normal"
           ? CAIRO_FONT_SLANT_NORMAL
           : (textStyle.fontstyle == "italic" ? CAIRO_FONT_SLANT_ITALIC
                                              : CAIRO_FONT_SLANT_OBLIQUE));
  cairo_font_weight_t cairo_fontsweight =
      (textStyle.fontweight == "normal" ? CAIRO_FONT_WEIGHT_NORMAL : CAIRO_FONT_WEIGHT_BOLD);

  // cairo surface to get font mectrics
  cairo_surface_t* cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t* cr = cairo_create(cs);
  cairo_surface_destroy(cs);

  cairo_select_font_face(cr, textStyle.fontfamily.c_str(), cairo_fontstyle, cairo_fontsweight);
  cairo_set_font_size(cr, Fmi::stoi(textStyle.fontsize));

  cairo_text_extents_t extents;
  cairo_text_extents(cr, text.c_str(), &extents);

  text_dimension_t ret;

  ret.width = static_cast<unsigned int>(ceil(std::max(extents.width, extents.x_advance)));
  ret.height = static_cast<unsigned int>(ceil(extents.height));

  return ret;
}

text_dimension_t getTextDimension(const std::vector<std::string>& rows,
                                  const text_style_t& textStyle)
{
  text_dimension_t ret = getTextDimension(std::string("W"), textStyle);
  // if width is longer in subsequent rows, update width
  for (unsigned int i = 0; i < rows.size(); i++)
  {
    text_dimension_t textDim = getTextDimension(rows[i], textStyle);
    if (textDim.width > ret.width)
      ret.width = textDim.width;
  }
  ret.height = ((ret.height + 5) * rows.size());

  return ret;
}

text_style_t getTextStyle(const Attributes& attributes, const text_style_t& defaultValues)
{
  text_style_t ret(defaultValues);

  if (!attributes.value("font-family").empty())
    ret.fontfamily = attributes.value("font-family");

  if (!attributes.value("font-size").empty())
    ret.fontsize = attributes.value("font-size");

  if (!attributes.value("font-style").empty())
    ret.fontstyle = attributes.value("font-style");

  if (!attributes.value("font-weight").empty())
    ret.fontweight = attributes.value("font-weight");

  if (!attributes.value("text-anchor").empty())
    ret.textanchor = attributes.value("text-anchor");

  return ret;
}

void addTextField(double xPos,
                  double yPos,
                  const std::vector<std::string>& rows,
                  const Attributes& attributes,
                  CTPP::CDT& globals,
                  CTPP::CDT& layersCdt,
                  State& state)
{
  if (rows.empty())
    return;

  text_style_t textStyle = getTextStyle(attributes, text_style_t());
  // initialize dimension
  text_dimension_t textDimension = getTextDimension(rows, textStyle);

  double xCoord = xPos;
  double yCoord = yPos;
  double rowHeight = textDimension.height / rows.size();

  for (unsigned int i = 0; i < rows.size(); i++)
  {
    yCoord = (yPos + (i * rowHeight));

    const std::string& row = rows[i];
    CTPP::CDT textCdt(CTPP::CDT::HASH_VAL);
    textCdt["start"] = "<text";
    textCdt["end"] = "</text>";
    textCdt["cdata"] = row;
    textCdt["attributes"]["x"] = Fmi::to_string(std::round(xCoord));
    textCdt["attributes"]["y"] = Fmi::to_string(std::round(yCoord));
    textCdt["attributes"]["xml:space"] = "preserve";
    state.addAttributes(globals, textCdt, attributes);
    layersCdt.PushBack(textCdt);
  }
}

void addTextField(double xPos,
                  double yPos,
                  double fieldWidth,
                  const std::vector<std::string>& rows,
                  const Attributes& attributes,
                  CTPP::CDT& globals,
                  CTPP::CDT& layersCdt,
                  State& state)
{
  if (rows.empty())
    return;

  text_style_t textStyle = getTextStyle(attributes, text_style_t());
  // initialize dimension
  text_dimension_t textDimension = getTextDimension(rows, textStyle);

  double xCoord = xPos;
  double yCoord = yPos;
  double rowHeight = textDimension.height / rows.size();

  //"text-anchor": "start"
  std::string textAnchor = attributes.value("text-anchor");
  if (textAnchor.compare("middle") == 0)
    xCoord += (fieldWidth / 2.0);
  else if (textAnchor.compare("end") == 0)
    xCoord += (fieldWidth - 3.0);
  else
    xCoord += 3.0;

  for (unsigned int i = 0; i < rows.size(); i++)
  {
    yCoord = (yPos + (i * rowHeight));

    const std::string& row = rows[i];
    CTPP::CDT textCdt(CTPP::CDT::HASH_VAL);
    textCdt["start"] = "<text";
    textCdt["end"] = "</text>";
    textCdt["cdata"] = row;
    textCdt["attributes"]["x"] = Fmi::to_string(std::round(xCoord));
    textCdt["attributes"]["y"] = Fmi::to_string(std::round(yCoord));
    textCdt["attributes"]["xml:space"] = "preserve";
    state.addAttributes(globals, textCdt, attributes);
    layersCdt.PushBack(textCdt);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
