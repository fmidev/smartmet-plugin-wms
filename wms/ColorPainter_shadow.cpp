#include "ColorPainter_shadow.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


ColorPainter_shadow::ColorPainter_shadow()
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}




ColorPainter_shadow::~ColorPainter_shadow()
{
}



void ColorPainter_shadow::init(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    ColorPainter::init(theJson,theState);

    auto json = JsonTools::remove(theJson, "shadows");
    if (!json.isNull())
      initShadows(json,theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_shadow::initShadows(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isArray())
      throw Fmi::Exception(BCP, "Shadows JSON is not a JSON array");

    for (auto& json : theJson)
    {
      if (!json.isNull())
        initShadow(json,theState);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_shadow::initShadow(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    Shadow shadow;

    JsonTools::remove_double(shadow.value_min, theJson, "value_min");
    JsonTools::remove_double(shadow.value_max, theJson, "value_max");

    std::string col = "00000000";
    JsonTools::remove_string(col, theJson, "color_min");
    shadow.color_min = argb(col);

    col = "00000000";
    JsonTools::remove_string(col, theJson, "color_max");
    shadow.color_max = argb(col);

    JsonTools::remove_int(shadow.dx, theJson, "dx");
    JsonTools::remove_int(shadow.dy, theJson, "dy");

    addShadow(shadow);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_shadow::addShadow(Shadow& shadow)
{
  try
  {
    shadows.push_back(shadow);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_shadow::setImageColors(uint width,uint height,uint /* loop_step */,uint /* loop_steps */,uint *image,std::vector<float>& /* land */,std::vector<float>& values,Parameters& /* parameters */)
{
  try
  {
    int sz = width*height;
    if (sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    if (sz != (int)values.size())
    {
      Fmi::Exception exception(BCP, "Invalid number of values!");
      exception.addParameter("values",std::to_string(values.size()));
      throw exception;
    }

    for (auto shadow = shadows.begin(); shadow != shadows.end(); ++shadow)
    {
      double minVal = shadow->value_min;
      double maxVal = shadow->value_max;
      uint minCol = shadow->color_min;
      uint maxCol = shadow->color_max;
      int dx = shadow->dx;
      int dy = shadow->dy;

      double dv = maxVal - minVal;

      if (dx == 0 && dy == 0)
        return;

      uchar *a = (uchar*)&minCol;
      uchar *b = (uchar*)&maxCol;

      uint newCol = 0;
      uchar *n = (uchar*)&newCol;

      uint c = 0;
      for (uint y = 0; y < height; y++)
      {
        int dyy = y + dy;
        uint yy = height - y -1;
        int py = yy * width;
        int yp = (yy+dy) * width;

        for (uint x = 0; x < width; x++)
        {
          int dxx = x - dx;
          int px = py + x;
          int xp = yp + x - dx;

          if (xp >= 0  &&  xp < sz  &&  dxx >= 0  &&  dxx < (int)width  &&  dyy >=0  &&  dyy < (int)height)
          {
            uint oldcol = image[c];
            uint valcol = 0;
            float val = values[xp];
            float valx = values[px];

            if (val != ParamValueMissing)
            {
              if (val >= minVal && val <= maxVal  &&  !(valx >= minVal &&  valx <= maxVal))
              {
                double vv = (val - minVal) / dv;
                for (uint t=0; t<4; t++)
                {
                  int d = ((int)b[t] - (int)a[t]) * vv;
                  n[t] = a[t] + d;
                }
                valcol = newCol;
              }
              image[c] = merge_ARGB(valcol,oldcol);
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




std::size_t ColorPainter_shadow::hash_value(const State &theState) const
{
  try
  {
    std::size_t hash = ColorPainter::hash_value(theState);

    for (auto shadow = shadows.begin(); shadow != shadows.end(); ++shadow)
    {
      Fmi::hash_combine(hash,shadow->value_min);
      Fmi::hash_combine(hash,shadow->value_max);
      Fmi::hash_combine(hash,Fmi::hash(shadow->color_min));
      Fmi::hash_combine(hash,Fmi::hash(shadow->color_max));
      Fmi::hash_combine(hash,Fmi::hash(shadow->dx));
      Fmi::hash_combine(hash,Fmi::hash(shadow->dy));
    }

    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet




