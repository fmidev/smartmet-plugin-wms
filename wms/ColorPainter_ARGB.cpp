#include "ColorPainter_ARGB.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>

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
/*
    Colors colors;
    colors.insert(std::pair<float,unsigned int>(0.0,0x00000000));
    colors.insert(std::pair<float,unsigned int>(1.0,0x80FFFFFF));
    ColorMap_sptr colorMap(new ColorMap());
    colorMap->setColors(colors);
    colorMaps.insert(std::pair<std::string,ColorMap_sptr>("percent",colorMap));


    colors.clear();
    colors.insert(std::pair<float,unsigned int>(0.0,0xFF739EC9));
    colors.insert(std::pair<float,unsigned int>(0.1,0x00739EC9));
    colors.insert(std::pair<float,unsigned int>(1.0,0x00000000));
    ColorMap_sptr colorMap2(new ColorMap());
    colorMap2->setColors(colors);
    colorMaps.insert(std::pair<std::string,ColorMap_sptr>("sea",colorMap2));


    colors.clear();
    colors.insert(std::pair<float,unsigned int>(0.0,0x00000000));
    colors.insert(std::pair<float,unsigned int>(0.099,0x00A8BBA3));
    colors.insert(std::pair<float,unsigned int>(0.1,0xFFA8BBA3));
    colors.insert(std::pair<float,unsigned int>(1.0,0xFFA8BBA3));
    ColorMap_sptr colorMap3(new ColorMap());
    colorMap3->setColors(colors);
    colorMaps.insert(std::pair<std::string,ColorMap_sptr>("land",colorMap3));

    colors.clear();
    colors.insert(std::pair<float,unsigned int>(0.0,0x20000000));
    colors.insert(std::pair<float,unsigned int>(0.1,0x00000000));
    colors.insert(std::pair<float,unsigned int>(1.0,0x00000000));
    ColorMap_sptr colorMap4(new ColorMap());
    colorMap4->setColors(colors);
    colorMaps.insert(std::pair<std::string,ColorMap_sptr>("sea2",colorMap4));
*/
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
    Colors colors;
    ColorMap_sptr cm = std::shared_ptr<ColorMap>(new ColorMap());

    uint c = 0;
    char buf[1000];
    const char *p = colorMap.c_str();
    uint s = 0;
    uint cc = 0;
    while (*p != '\0')
    {
      if (*p == ';')
      {
        buf[c] = '\0';
        c++;
        s = c;
        cc = 0;
      }
      else
      if (*p == '\n' || *p == '#')
      {
        buf[c] = '\0';
        c = 0;
        if (s != 0)
        {
          float val = toDouble(buf);
          uint col = 0;
          if (cc == 3)
          {
            std::vector<uint> v;
            splitString(buf + s,',',v);
            if (v.size() == 4)
            {
              // ARGB (a,r,g,b)
              col = (v[0] <<  24) + (v[1] <<  16) + (v[2] <<  8) + v[3];
            }
          }
          else
          {
            // ARGB (Hex)
            col = strtoul(buf + s,nullptr,16);
          }
          colors.insert(std::pair<float,unsigned int>(val,col));
        }
        s = 0;
        cc = 0;
      }
      else
      {
        if (*p == ',')
          cc++;

        buf[c] = *p;
        c++;
      }
      p++;
    }

    cm->setColors(colors);
    colorMaps.insert(std::pair<std::string,ColorMap_sptr>(name,cm));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_ARGB::setImageColors(uint width,uint height,uint *image,std::vector<float>& values,Parameters& parameters)
{
  try
  {
    uint sz = width*height;
    if (sz != values.size() || sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
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
          uint cc = p + x;
          float val = values[cc];
          if (val != ParamValueMissing)
            image[c] = cm->getColor(val);
          else
            image[c] = 0x00000000;
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
          uint cc = p + x;
          float val = values[cc];
          if (val != ParamValueMissing)
            image[c] = cm->getSmoothColor(val);
          else
            image[c] = 0x00000000;
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
