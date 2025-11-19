#include "ColorPainter_stream.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/GraphFunctions.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


ColorPainter_stream::ColorPainter_stream()
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}




ColorPainter_stream::~ColorPainter_stream()
{
}




void ColorPainter_stream::addColorMap(std::string name,ColorMap_sptr colorMap)
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




void ColorPainter_stream::addColorMap(std::string name,std::string& colorMap)
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




void ColorPainter_stream::setImageColors(uint width,uint height,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters)
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

    uint stream_color = 0x00FFFFFF;
    auto scolor = parameters.find("stream_color");
    if (scolor != parameters.end())
      stream_color = strtoul(scolor->second.c_str(),nullptr,16);

    uint stream_step_x = 10;
    auto stepx = parameters.find("stream_step_x");
    if (stepx != parameters.end())
      stream_step_x = strtoul(stepx->second.c_str(),nullptr,16);

    uint stream_step_y = 10;
    auto stepy = parameters.find("stream_step_y");
    if (stepy != parameters.end())
      stream_step_y = strtoul(stepy->second.c_str(),nullptr,16);

    uint stream_minLength = 10;
    auto min = parameters.find("stream_min_length");
    if (min != parameters.end())
      stream_minLength = strtoul(min->second.c_str(),nullptr,16);

    uint stream_maxLength = 64;
    auto max = parameters.find("stream_max_length");
    if (max != parameters.end())
      stream_maxLength = strtoul(max->second.c_str(),nullptr,16);

    double opacity_land = 1.0;
    auto opacity_value_land = parameters.find("opacity_land");
    if (opacity_value_land != parameters.end())
      opacity_land = toDouble(opacity_value_land->second);

    double opacity_sea = 1.0;
    auto opacity_value_sea = parameters.find("opacity_sea");
    if (opacity_value_sea != parameters.end())
      opacity_sea = toDouble(opacity_value_sea->second);

    // Rotate

    float *direction = new float[sz];
    uint idx = 0;
    for (uint y=0; y<height; y++)
    {
      uint idx2 = (height-y-1) * width;
      for (uint x=0; x<width; x++)
      {
        direction[idx] = values[idx2];
        idx++;
        idx2++;
      }
    }

    uint *streamImage = new uint[sz];
    getStreamlineImage(direction,nullptr,streamImage,width,height,stream_step_x,stream_step_y,stream_minLength,stream_maxLength);

    uint alphamax = 0xFF;
    uint lcolors = 16;
    uint color_land[lcolors];
    uint color_sea[lcolors];

    double mp = (double)alphamax / (double)lcolors;

    for (uint t=0; t<lcolors; t++)
    {
      uint cc = (alphamax - (uint)(t*mp))*opacity_land;
      if (cc > alphamax)
        cc = alphamax;

      color_land[t] = (cc << 24) + stream_color;

      cc = (alphamax - (uint)(t*mp))*opacity_sea;
      if (cc > alphamax)
        cc = alphamax;

      color_sea[t] = (cc << 24) + stream_color;
    }


    uint c = 0;
    for (uint y = 0; y < height; y++)
    {
      uint yy = height - y -1;
      uint p = yy * width;
      for (uint x = 0; x < width; x++)
      {
        uint oldcol = image[c];
        uint cc = p + x;
        uint val = streamImage[c];
        float landval = land[cc];
        if (val != 0)
        {
          if (landval > 0.9)
          {
            uint valcol = color_land[(val-1) % lcolors];
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
            uint valcol = color_sea[(val-1) % lcolors];
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


/*
    for (uint t=0; t<sz; t++)
    {
      uint sval = streamImage[t];
      if (sval != 0)
      {
        image[t] = merge_ARGB(color[(sval-1) % lcolors],image[t]);
      }
    }
*/
    delete [] direction;
    delete [] streamImage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



void ColorPainter_stream::setImageColors(uint width,uint height,uint *image,std::vector<float>& land,std::vector<float>& values1,std::vector<float>& values2,Parameters& parameters)
{
  try
  {
    uint sz = width*height;
    if (sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    if (sz != values1.size())
    {
      Fmi::Exception exception(BCP, "Invalid number of values1!");
      exception.addParameter("values1",std::to_string(values1.size()));
      throw exception;
    }

    if (sz != values2.size())
    {
      Fmi::Exception exception(BCP, "Invalid number of values2!");
      exception.addParameter("values2",std::to_string(values2.size()));
      throw exception;
    }

    uint stream_step_x = 10;
    auto stepx = parameters.find("stream_step_x");
    if (stepx != parameters.end())
      stream_step_x = strtoul(stepx->second.c_str(),nullptr,16);

    uint stream_step_y = 10;
    auto stepy = parameters.find("stream_step_y");
    if (stepy != parameters.end())
      stream_step_y = strtoul(stepy->second.c_str(),nullptr,16);

    uint stream_minLength = 10;
    auto min = parameters.find("stream_min_length");
    if (min != parameters.end())
      stream_minLength = strtoul(min->second.c_str(),nullptr,16);

    uint stream_maxLength = 64;
    auto max = parameters.find("stream_max_length");
    if (max != parameters.end())
      stream_maxLength = strtoul(max->second.c_str(),nullptr,16);

    double opacity_land = 1.0;
    auto opacity_value_land = parameters.find("opacity_land");
    if (opacity_value_land != parameters.end())
      opacity_land = toDouble(opacity_value_land->second);

    double opacity_sea = 1.0;
    auto opacity_value_sea = parameters.find("opacity_sea");
    if (opacity_value_sea != parameters.end())
      opacity_sea = toDouble(opacity_value_sea->second);

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

    ColorMap_sptr cm = cmRec->second;

    // Rotate

    float *direction = new float[sz];
    float *speed = new float[sz];
    uint idx = 0;
    for (uint y=0; y<height; y++)
    {
      uint idx2 = (height-y-1) * width;
      for (uint x=0; x<width; x++)
      {
        direction[idx] = values1[idx2];
        speed[idx] = values2[idx2];
        idx++;
        idx2++;
      }
    }

    uint *streamImage = new uint[sz];
    getStreamlineImage(direction,nullptr,streamImage,width,height,stream_step_x,stream_step_y,stream_minLength,stream_maxLength);

    uint alphamax = 0xFF;
    uint lcolors = 16;
    uint alpha_land[lcolors];
    uint alpha_sea[lcolors];

    double mp = (double)alphamax / (double)lcolors;

    for (uint t=0; t<lcolors; t++)
    {
      uint cc = (alphamax - (uint)(t*mp))*opacity_land;
      if (cc > alphamax)
        cc = alphamax;

      alpha_land[t] = (cc << 24);

      cc = (alphamax - (uint)(t*mp))*opacity_sea;
      if (cc > alphamax)
        cc = alphamax;

      alpha_sea[t] = (cc << 24);
    }

    uint c = 0;
    for (uint y = 0; y < height; y++)
    {
      uint yy = height - y -1;
      uint p = yy * width;
      for (uint x = 0; x < width; x++)
      {
        uint oldcol = image[c];
        uint cc = p + x;
        uint val = streamImage[c];
        float landval = land[cc];
        if (val != 0)
        {
          if (landval > 0.9)
          {
            uint speedcol = (cm->getSmoothColor(speed[c]) & 0x00FFFFFF);
            uint valcol = alpha_land[(val-1) % lcolors] + speedcol;

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
            uint speedcol = (cm->getSmoothColor(speed[c]) & 0x00FFFFFF);
            uint valcol = alpha_sea[(val-1) % lcolors] + speedcol;

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


    /*
    for (uint t=0; t<sz; t++)
    {
      uint sval = streamImage[t];
      if (sval != 0)
      {
        uint speedcol = (cm->getSmoothColor(speed[t]) & 0x00FFFFFF);
        uint newcol = alpha[(sval-1) % lcolors] + speedcol;
        image[t] = merge_ARGB(newcol,image[t]);
      }
    }
    */
    delete [] speed;
    delete [] direction;
    delete [] streamImage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
