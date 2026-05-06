#pragma once

#include "Attributes.h"
#include "Geometry.h"
#include "Projection.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <macgyver/CacheStats.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct text_dimension_t
{
  unsigned int width = 0;
  unsigned int height = 0;
  // y_bearing from cairo_text_extents: distance from the reference point
  // (text baseline) to the top of the visual glyph bbox.  Typically
  // negative for Latin text since the bbox top sits above the baseline.
  // Used to render text such that its visual extent fills a placed
  // label bbox exactly: SVG y = bbox.y1 - y_bearing places the visual
  // glyph top at bbox.y1.
  int y_bearing = 0;
};

struct text_style_t
{
  std::string fontfamily = "Roboto";
  std::string fontsize = "10";
  std::string fontstyle = "normal";
  std::string fontweight = "normal";
  std::string textanchor = "start";
};

text_dimension_t getTextDimension(const std::string& text, const text_style_t& textStyle);
text_dimension_t getTextDimension(const std::vector<std::string>& rows,
                                  const text_style_t& textStyle);
text_style_t getTextStyle(const Attributes& attributes, const text_style_t& defaultValues);
void addTextField(double xPos,
                  double yPos,
                  const std::vector<std::string>& rows,
                  const Attributes& attributes,
                  CTPP::CDT& globals,
                  CTPP::CDT& layersCdt,
                  const State& state);
void addTextField(double xPos,
                  double yPos,
                  double fieldWidth,
                  const std::vector<std::string>& rows,
                  const Attributes& attributes,
                  CTPP::CDT& globals,
                  CTPP::CDT& layersCdt,
                  const State& state);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
