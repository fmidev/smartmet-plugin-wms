#include "ColorPainter_range.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


ColorPainter_range::ColorPainter_range()
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}


ColorPainter_range::~ColorPainter_range()
{
}


void ColorPainter_range::setImageColors(uint width,uint height,uint *image,std::vector<float>& values,Parameters& parameters)
{
  try
  {
    uint sz = width*height;
    if (sz != values.size() || sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    auto min_value = parameters.find("min_value");
    if (min_value == parameters.end())
    {
      Fmi::Exception exception(BCP, "Cannot find the 'min_value' parameter!");
      throw exception;
    }

    auto max_value = parameters.find("max_value");
    if (max_value == parameters.end())
    {
      Fmi::Exception exception(BCP, "Cannot find the 'max_value' parameter!");
      throw exception;
    }

    auto min_color = parameters.find("min_color");
    if (min_color == parameters.end())
    {
      Fmi::Exception exception(BCP, "Cannot find the 'min_color' parameter!");
      throw exception;
    }

    auto max_color = parameters.find("max_color");
    if (max_color == parameters.end())
    {
      Fmi::Exception exception(BCP, "Cannot find the 'max_color' parameter!");
      throw exception;
    }

    uint lowCol = 0x00000000;
    auto low_color = parameters.find("low_color");
    if (low_color != parameters.end())
      lowCol = strtoul(low_color->second.c_str(),nullptr,16);

    uint highCol = 0x00000000;
    auto high_color = parameters.find("high_color");
    if (high_color != parameters.end())
      highCol = strtoul(high_color->second.c_str(),nullptr,16);

    double minVal = toDouble(min_value->second);
    double maxVal = toDouble(max_value->second);
    double dv = maxVal - minVal;

    uint minCol = strtoul(min_color->second.c_str(),nullptr,16);
    uint maxCol = strtoul(max_color->second.c_str(),nullptr,16);

    uchar *a = (uchar*)&minCol;
    uchar *b = (uchar*)&maxCol;

    uint newCol = 0;
    uchar *n = (uchar*)&newCol;

    uint c = 0;
    for (uint y = 0; y < height; y++)
    {
      uint yy = height - y -1;
      uint p = yy * width;
      for (uint x = 0; x < width; x++)
      {
        uint cc = p + x;
        float val = values[cc];
        if (val != ParamValueMissing)
        {
          if (val < minVal)
            image[c] = lowCol;
          else
          if (val > maxVal)
            image[c] = highCol;
          else
          {
            double vv = (val - minVal) / dv;
            for (uint t=0; t<4; t++)
            {
              int d = ((int)b[t] - (int)a[t]) * vv;
              n[t] = a[t] + d;
            }
            image[c] = newCol;
          }
        }
        else
          image[c] = 0x00000000;
        c++;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
