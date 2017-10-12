#pragma once

#include "Attributes.h"
#include "Geometry.h"
#include "Projection.h"
#include "State.h"
#include <ctpp2/CDT.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
struct text_dimension_t
{
  text_dimension_t() : width(0), height(0) {}
  text_dimension_t(const text_dimension_t& dim) : width(dim.width), height(dim.height) {}
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
                  State& state);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
