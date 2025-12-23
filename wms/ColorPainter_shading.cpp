#include "ColorPainter_shading.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/Typedefs.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


ColorPainter_shading::ColorPainter_shading()
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}


ColorPainter_shading::~ColorPainter_shading()
{
}




void ColorPainter_shading::setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters)
{
  try
  {
    uint sz = width*height;
    if (sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    if (sz != values.size())
    {
      Fmi::Exception exception(BCP, "Invalid number of values!");
      exception.addParameter("values",std::to_string(values.size()));
      throw exception;
    }

    bool rotate = true;

    auto rotate_val = parameters.find("rotate");
    if (rotate_val != parameters.end())
      rotate = (bool)toInt64(rotate_val->second);

    double light = 0;
    auto shading_light = parameters.find("shading_light");
    if (shading_light != parameters.end())
      light = toDouble(shading_light->second.c_str());

    double shadow = 0;
    auto shading_shadow = parameters.find("shading_shadow");
    if (shading_shadow != parameters.end())
      shadow = toDouble(shading_shadow->second.c_str());

    uint c = 0;
    for (uint y=0; y<height; y++)
    {
      int yy = y;
      if (rotate)
        yy = height-y-1;

      for (uint x=0; x<width; x++)
      {
        uint pixpos = yy*width + x;

        uint oldcol = image[c];
        float m = values[pixpos];

        if (m != ParamValueMissing)
        {
          if (m < 0)
          {
            uint pp = (uint)(-(double)shadow*m);
            if (pp > 255)
              pp = 255;

            uint scol = pp << 24;
            uint newcol = merge_ARGB(scol,oldcol);
            image[c] = newcol;
          }
          else
          {
            uint pp = (uint)((double)light*m);
            if (pp > 255)
              pp = 255;

            uint scol = (pp << 24) + 0xFFFFFF;
            uint newcol = merge_ARGB(scol,oldcol);
            image[c] = newcol;
          }
        }
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
