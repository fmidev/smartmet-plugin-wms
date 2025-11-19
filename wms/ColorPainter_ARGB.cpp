#include "ColorPainter_ARGB.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


ColorPainter_ARGB::ColorPainter_ARGB()
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}




ColorPainter_ARGB::~ColorPainter_ARGB()
{
}




void ColorPainter_ARGB::addColorMap(std::string name,ColorMap_sptr colorMap)
{
  try
  {
    colorMaps.insert(std::pair<std::string,ColorMap_sptr>(name,colorMap));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_ARGB::addColorMap(std::string name,std::string& colorMap)
{
  try
  {
    ColorMap_sptr cm = std::shared_ptr<ColorMap>(new ColorMap(colorMap));
    colorMaps.insert(std::pair<std::string,ColorMap_sptr>(name,cm));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_ARGB::setImageColors(uint width,uint height,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters)
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

    auto cmap = parameters.find("colormap");
    if (cmap == parameters.end())
    {
      Fmi::Exception exception(BCP, "Cannot find the 'colormap' parameter!");
      throw exception;
    }

    auto cmRec = colorMaps.find(cmap->second);
    if (cmRec == colorMaps.end())
    {
      Fmi::Exception exception(BCP, "Cannot find the colormap!");
      exception.addParameter("colormap",cmap->second);
      throw exception;
    }

    double opacity_land = 1.0;
    auto opacity_value_land = parameters.find("opacity_land");
    if (opacity_value_land != parameters.end())
      opacity_land = toDouble(opacity_value_land->second);

    double opacity_sea = 1.0;
    auto opacity_value_sea = parameters.find("opacity_sea");
    if (opacity_value_sea != parameters.end())
      opacity_sea = toDouble(opacity_value_sea->second);

    ColorMap_sptr cm = cmRec->second;
    uint c = 0;

    auto smooth = parameters.find("smooth");
    if (smooth != parameters.end()  &&  smooth->second == "false")
    {
      for (uint y = 0; y < height; y++)
      {
        uint yy = height - y -1;
        uint p = yy * width;
        for (uint x = 0; x < width; x++)
        {
          uint oldcol = image[c];
          uint cc = p + x;
          float val = values[cc];
          float landval = land[cc];
          if (val != ParamValueMissing)
          {
            uint valcol = cm->getColor(val);
            if (landval > 0.9)
            {
              if (opacity_land != 1.0)
              {
                uint op = (uint)((double)((valcol & 0xFF000000) >> 24) * opacity_land);
                if (op > 255)
                  op = 0xFF000000;
                else
                  op = (op & 0xFF) << 24;

                uint nv = op + (valcol & 0x00FFFFFF);
                image[c] = merge_ARGB(nv,oldcol);
              }
              else
              {
                image[c] = merge_ARGB(valcol,oldcol);
              }
            }
            else
            {
              if (opacity_sea != 1.0)
              {
                uint op = (uint)((double)((valcol & 0xFF000000) >> 24) * opacity_sea);
                if (op > 255)
                  op = 0xFF000000;
                else
                  op = (op & 0xFF) << 24;

                uint nv = op + (valcol & 0x00FFFFFF);
                image[c] = merge_ARGB(nv,oldcol);
              }
              else
              {
                image[c] = merge_ARGB(valcol,oldcol);
              }
            }
          }
          //else
          //  image[c] = 0xFF00FF00;

          c++;
        }
      }
    }
    else
    {
      for (uint y = 0; y < height; y++)
      {
        uint yy = height - y -1;
        uint p = yy * width;
        for (uint x = 0; x < width; x++)
        {
          uint oldcol = image[c];
          uint cc = p + x;
          float val = values[cc];
          float landval = land[cc];
          if (val != ParamValueMissing)
          {
            uint valcol = cm->getSmoothColor(val);
            if (landval > 0.9)
            {
              if (opacity_land != 1.0)
              {
                uint op = (uint)((double)((valcol & 0xFF000000) >> 24) * opacity_land);
                if (op > 255)
                  op = 0xFF000000;
                else
                  op = (op & 0xFF) << 24;

                uint nv = op + (valcol & 0x00FFFFFF);
                image[c] = merge_ARGB(nv,oldcol);
              }
              else
              {
                image[c] = merge_ARGB(valcol,oldcol);
              }
            }
            else
            {
              if (opacity_sea != 1.0)
              {
                uint op = (uint)((double)((valcol & 0xFF000000) >> 24) * opacity_sea);
                if (op > 255)
                  op = 0xFF000000;
                else
                  op = (op & 0xFF) << 24;

                uint nv = op + (valcol & 0x00FFFFFF);
                image[c] = merge_ARGB(nv,oldcol);
              }
              else
              {
                image[c] = merge_ARGB(valcol,oldcol);
              }
            }
          }
          c++;
        }
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
