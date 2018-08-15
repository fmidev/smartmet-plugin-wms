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

rgb_color hex_string_to_rgb(const std::string& hex_string);
unsigned int parse_resolution(const std::string& periodString, size_t designatorCharPos);
unsigned int resolution_in_minutes(const std::string& resolution);

std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest);

std::string demimetype(const std::string& theMimeType);
void useStyle(Json::Value& root, const Json::Value& styles);
void useStyle(Json::Value& root, const std::string& styleName);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
