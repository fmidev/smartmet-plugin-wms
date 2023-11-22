#include "Colour.h"
#include "WMSException.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
rgb_color hex_string_to_rgb(const std::string& hex_string)
{
  try
  {
    // valid fromat is 0xFFFFFF
    if (hex_string.size() != 8)
    {
      throw Fmi::Exception(BCP, "Invalid BGCOLOR parameter '" + hex_string + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    unsigned int red = 255;
    unsigned int green = 255;
    unsigned int blue = 255;

    try
    {
      std::size_t pos = 0;
      red = stoi(hex_string.substr(2, 2), &pos, 16);
      green = stoi(hex_string.substr(4, 2), &pos, 16);
      blue = stoi(hex_string.substr(6, 2), &pos, 16);
    }
    catch (...)
    {
      throw Fmi::Exception::Trace(BCP, "Invalid BGCOLOR parameter '" + hex_string + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    }

    return {red, green, blue};
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Converting hex string to RGB failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
