#pragma once

#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct rgb_color
{
  unsigned int red = 0xFF;
  unsigned int green = 0xFF;
  unsigned int blue = 0xFF;

  rgb_color() = default;
  rgb_color(unsigned int r, unsigned int g, unsigned int b) : red(r), green(g), blue(b) {}
};

rgb_color hex_string_to_rgb(const std::string& hex_string);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
