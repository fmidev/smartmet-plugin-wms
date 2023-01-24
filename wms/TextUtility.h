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
  unsigned int width = 0;
  unsigned int height = 0;
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
                  State& state);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
