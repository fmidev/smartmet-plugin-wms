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
  unsigned int red;
  unsigned int green;
  unsigned int blue;

  rgb_color() : red(0xFF), green(0xFF), blue(0xFF) {}
  rgb_color(unsigned int r, unsigned int g, unsigned int b) : red(r), green(g), blue(b) {}
};

rgb_color hex_string_to_rgb(const std::string& hex_string);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
