#include "TimeResolution.h"
#include "WMSException.h"
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
unsigned int parse_resolution(const std::string& periodString, size_t designatorCharPos)
{
  try
  {
    if (periodString.empty() || periodString.at(0) != 'P' || designatorCharPos == 0)
    {
      Spine::Exception exception(BCP, "Cannot parse resolution string '" + periodString + "'!");
      exception.addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
      throw exception;
    }

    if (designatorCharPos == std::string::npos)
      return 0;

    size_t pos(designatorCharPos - 1);
    // count digits before previous character
    while (isdigit(periodString.at(pos)) != 0)
      pos--;

    if ((designatorCharPos - pos) == 1)
    {
      throw Spine::Exception(BCP, "Invalid dimension value '" + periodString + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
    }

    std::string numberStr(periodString.substr(pos + 1, designatorCharPos - pos - 1));

    unsigned int retval(0);

    try
    {
      retval = boost::lexical_cast<unsigned int>(numberStr);
    }
    catch (const boost::bad_lexical_cast&)
    {
      throw Spine::Exception::Trace(BCP, "Invalid dimension value '" + periodString + "'!")
          .addParameter(WMS_EXCEPTION_CODE, WMS_INVALID_DIMENSION_VALUE);
    }

    return retval;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Parsing resolution failed!");
  }
}

unsigned int resolution_in_minutes(const std::string& resolution)
{
  try
  {
    bool timeResolutionExists(resolution.find('T') != std::string::npos);
    bool minutesDefined(timeResolutionExists && (resolution.rfind('M') > resolution.find('T')));

    // note! years, months are not relevant in FMI timesteps, so they are not parsed
    unsigned int days(parse_resolution(resolution, resolution.find('D')));
    unsigned int hours(parse_resolution(resolution, resolution.find('H')));
    unsigned int minutes(minutesDefined ? parse_resolution(resolution, resolution.rfind('M')) : 0);
    unsigned int seconds(parse_resolution(resolution, resolution.find('S')));

    return ((days * 1440) + (hours * 60) + (minutes + seconds / 60));
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Extracting resolution in minutes failed!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
