// ======================================================================
/*!
 * \brief A Meb Maps Service (WMS) Request
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include <boost/shared_ptr.hpp>
#include <macgyver/TemplateFormatterMT.h>
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
enum class WMSRequestType
{
  GET_CAPABILITIES,
  GET_MAP,
  GET_LEGEND_GRAPHIC,
  GET_FEATURE_INFO,
  NOT_A_WMS_REQUEST
};

typedef boost::shared_ptr<Fmi::TemplateFormatterMT> WMSExceptionFormatter;

struct rgb_color
{
  unsigned int red;
  unsigned int green;
  unsigned int blue;

  rgb_color() : red(0xFF), green(0xFF), blue(0xFF) {}
  rgb_color(unsigned int r, unsigned int g, unsigned int b) : red(r), green(g), blue(b) {}
};

WMSRequestType wmsRequestType(const Spine::HTTP::Request& theHTTPRequest);

std::string enclose_with_quotes(const std::string& param);
rgb_color hex_string_to_rgb(const std::string& hex_string);
unsigned int parse_resolution(const std::string& periodString, size_t designatorCharPos);
unsigned int resolution_in_minutes(const std::string resolution);

std::ostream& operator<<(std::ostream& ost, const Spine::HTTP::Request& theHTTPRequest);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
