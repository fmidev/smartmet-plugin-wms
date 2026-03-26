#include "TextUtility.h"
#include <boost/functional/hash.hpp>
#include <gis/Box.h>
#include <macgyver/LRUCache.h>
#include <macgyver/StringConversion.h>
#include <algorithm>
#include <cairo.h>
#include <mutex>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{

std::size_t makeCacheKey(const std::string& text, const text_style_t& s)
{
  std::size_t seed = 0;
  boost::hash_combine(seed, text);
  boost::hash_combine(seed, s.fontfamily);
  boost::hash_combine(seed, s.fontsize);
  boost::hash_combine(seed, s.fontstyle);
  boost::hash_combine(seed, s.fontweight);
  // textanchor does not affect glyph metrics, intentionally excluded
  return seed;
}

// Cache capacity is deliberately generous: each entry is two uints (~8 bytes),
// so 10 000 entries costs ~80 kB plus map/list overhead — negligible.
Fmi::LRUCache<text_dimension_t> g_dimCache(10000);

// Serialises cairo calls on cache miss.  The critical section is short
// (one 1×1 surface + text_extents call) and will almost never be reached
// once the cache is warm.
std::mutex g_cairoMutex;

text_dimension_t measureWithCairo(const std::string& text, const text_style_t& textStyle)
{
  cairo_font_slant_t slant = (textStyle.fontstyle == "italic"    ? CAIRO_FONT_SLANT_ITALIC
                              : textStyle.fontstyle == "oblique" ? CAIRO_FONT_SLANT_OBLIQUE
                                                                 : CAIRO_FONT_SLANT_NORMAL);

  cairo_font_weight_t weight =
      (textStyle.fontweight == "bold" ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);

  cairo_surface_t* cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
  cairo_t* cr = cairo_create(cs);

  cairo_select_font_face(cr, textStyle.fontfamily.c_str(), slant, weight);
  cairo_set_font_size(cr, Fmi::stoi(textStyle.fontsize));

  cairo_text_extents_t extents;
  cairo_text_extents(cr, text.c_str(), &extents);

  cairo_destroy(cr);
  cairo_surface_destroy(cs);

  text_dimension_t dim;
  dim.width = static_cast<unsigned int>(ceil(std::max(extents.width, extents.x_advance)));
  dim.height = static_cast<unsigned int>(ceil(extents.height));
  return dim;
}

}  // anonymous namespace

// ------------------------------------------------------------------
text_dimension_t getTextDimension(const std::string& text, const text_style_t& textStyle)
{
  const std::size_t key = makeCacheKey(text, textStyle);

  // Fast path: cache hit (shared-read lock inside get())
  if (auto hit = g_dimCache.get(key))
    return **hit;

  // Slow path: measure and populate.
  // Lock cairo, then re-check the cache in case another thread already
  // inserted while we were waiting — avoids a redundant cairo call and
  // a harmless but wasteful duplicate put().
  std::lock_guard<std::mutex> lock(g_cairoMutex);

  if (auto hit = g_dimCache.get(key))
    return **hit;

  text_dimension_t dim = measureWithCairo(text, textStyle);
  g_dimCache.put(key, std::make_shared<text_dimension_t>(dim));
  return dim;
}

// ------------------------------------------------------------------
text_dimension_t getTextDimension(const std::vector<std::string>& rows,
                                  const text_style_t& textStyle)
{
  // Use "W" as the baseline height probe (unchanged from original)
  text_dimension_t ret = getTextDimension(std::string("W"), textStyle);

  for (const auto& row : rows)
  {
    text_dimension_t rowDim = getTextDimension(row, textStyle);
    ret.width = std::max(rowDim.width, ret.width);
  }
  ret.height = ((ret.height + 5) * rows.size());
  return ret;
}

// ------------------------------------------------------------------
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

// ------------------------------------------------------------------
void addTextField(double xPos,
                  double yPos,
                  const std::vector<std::string>& rows,
                  const Attributes& attributes,
                  CTPP::CDT& globals,
                  CTPP::CDT& layersCdt,
                  const State& state)
{
  if (rows.empty())
    return;

  text_style_t textStyle = getTextStyle(attributes, text_style_t());
  text_dimension_t textDimension = getTextDimension(rows, textStyle);

  const double rowHeight = textDimension.height / static_cast<double>(rows.size());

  for (unsigned int i = 0; i < rows.size(); i++)
  {
    CTPP::CDT textCdt(CTPP::CDT::HASH_VAL);
    textCdt["start"] = "<text";
    textCdt["end"] = "</text>";
    textCdt["cdata"] = rows[i];
    textCdt["attributes"]["x"] = Fmi::to_string(lround(xPos));
    textCdt["attributes"]["y"] = Fmi::to_string(lround(yPos + i * rowHeight));
    textCdt["attributes"]["xml:space"] = "preserve";
    state.addAttributes(globals, textCdt, attributes);
    layersCdt.PushBack(textCdt);
  }
}

// ------------------------------------------------------------------
void addTextField(double xPos,
                  double yPos,
                  double fieldWidth,
                  const std::vector<std::string>& rows,
                  const Attributes& attributes,
                  CTPP::CDT& globals,
                  CTPP::CDT& layersCdt,
                  const State& state)
{
  if (rows.empty())
    return;

  text_style_t textStyle = getTextStyle(attributes, text_style_t());
  text_dimension_t textDimension = getTextDimension(rows, textStyle);

  const double rowHeight = textDimension.height / static_cast<double>(rows.size());
  const std::string& textAnchor = attributes.value("text-anchor");

  double xCoord = xPos;
  if (textAnchor == "middle")
    xCoord += fieldWidth / 2.0;
  else if (textAnchor == "end")
    xCoord += fieldWidth - 3.0;
  else
    xCoord += 3.0;

  for (unsigned int i = 0; i < rows.size(); i++)
  {
    CTPP::CDT textCdt(CTPP::CDT::HASH_VAL);
    textCdt["start"] = "<text";
    textCdt["end"] = "</text>";
    textCdt["cdata"] = rows[i];
    textCdt["attributes"]["x"] = Fmi::to_string(lround(xCoord));
    textCdt["attributes"]["y"] = Fmi::to_string(lround(yPos + i * rowHeight));
    textCdt["attributes"]["xml:space"] = "preserve";
    state.addAttributes(globals, textCdt, attributes);
    layersCdt.PushBack(textCdt);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
