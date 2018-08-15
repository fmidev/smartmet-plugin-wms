// ======================================================================
/*!
 * \brief A Meb Maps Service (WMS) Request
 */
// ======================================================================

#pragma once

#include <boost/shared_ptr.hpp>
#include <spine/HTTP.h>
#include <spine/Json.h>

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

std::string enclose_with_quotes(const std::string& param);
rgb_color hex_string_to_rgb(const std::string& hex_string);
unsigned int parse_resolution(const std::string& periodString, size_t designatorCharPos);
unsigned int resolution_in_minutes(const std::string& resolution);

std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest);

struct CaseInsensitiveComparator : std::binary_function<std::string, std::string, bool>
{
  char asciilower(char ch) const
  {
    char ret = ch;
    if (ch >= 'A' && ch <= 'Z')
      ret = static_cast<char>(ch + ('a' - 'A'));
    return ret;
  }

  bool operator()(const std::string& first, const std::string& second) const
  {
    std::size_t n = std::min(first.size(), second.size());
    for (std::size_t i = 0; i < n; i++)
    {
      char ch1 = asciilower(first[i]);
      char ch2 = asciilower(second[i]);
      if (ch1 != ch2)
        return false;
    }

    return (first.size() == second.size());
  }
};

std::string demimetype(const std::string& theMimeType);
void useStyle(Json::Value& root, const Json::Value& styles);
void useStyle(Json::Value& root, const std::string& styleName);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
