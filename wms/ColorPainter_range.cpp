#include "ColorPainter_range.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>

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

ColorPainter_range::~ColorPainter_range() = default;

void ColorPainter_range::init(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    ColorPainter::init(theJson,theState);

    auto json = JsonTools::remove(theJson, "ranges");
    if (!json.isNull())
      initRanges(json,theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_range::initRanges(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isArray())
      throw Fmi::Exception(BCP, "Ranges JSON is not a JSON array");

    for (auto& json : theJson)
    {
      if (!json.isNull())
        initRange(json,theState);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_range::initRange(Json::Value &theJson, const State & /* theState */)
{
  try
  {
    if (theJson.isNull())
      return;

    Range range;

    JsonTools::remove_double(range.value_min, theJson, "value_min");
    JsonTools::remove_double(range.value_max, theJson, "value_max");

    std::string col = "00000000";
    JsonTools::remove_string(col, theJson, "color_min");
    range.color_min = argb(col);

    col = "00000000";
    JsonTools::remove_string(col, theJson, "color_max");
    range.color_max = argb(col);

    col = "00000000";
    JsonTools::remove_string(col, theJson, "color_low");
    range.color_low = argb(col);

    col = "00000000";
    JsonTools::remove_string(col, theJson, "color_high");
    range.color_high = argb(col);

    addRange(range);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ColorPainter_range::addRange(const Range& range)
{
  try
  {
    ranges.push_back(range);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




namespace
{

// Interpolate the four color channels between a and b
inline void interpolate_channels(const uchar *a,const uchar *b,uchar *n,double vv)
{
  for (uint t=0; t<4; t++)
  {
    int d = ((int)b[t] - (int)a[t]) * vv;
    n[t] = a[t] + d;
  }
}

// Paint the values of one range unless all its colors are fully transparent
void paint_range(const ColorPainter_range::Range& range,uint width,uint height,uint *image,const std::vector<float>& land,const std::vector<float>& values,double opacity_land,double opacity_sea)
{
  double dv = range.value_max - range.value_min;

  uint minCol = range.color_min;
  uint maxCol = range.color_max;
  uint lowCol = range.color_low;
  uint highCol = range.color_high;

  if (!((minCol & 0xFF000000) || (maxCol & 0xFF000000) || (lowCol & 0xFF000000) || (highCol & 0xFF000000)))
    return;

  auto* a = (uchar*)&minCol;
  auto* b = (uchar*)&maxCol;

  uint newCol = 0;
  auto* n = (uchar*)&newCol;

  uint c = 0;
  for (uint y = 0; y < height; y++)
  {
    uint yy = height - y -1;
    uint p = yy * width;
    for (uint x = 0; x < width; x++)
    {
      uint cc = p + x;
      uint oldcol = image[c];
      uint valcol = 0;
      float val = values[cc];
      float landval = land[cc];
      if (val != ParamValueMissing)
      {
        if (val < range.value_min)
          valcol = lowCol;
        else if (val > range.value_max)
          valcol = highCol;
        else
        {
          double vv = (val - range.value_min) / dv;
          interpolate_channels(a,b,n,vv);
          valcol = newCol;
        }
        if (landval > 0.9)
          image[c] = merge_ARGB(color_with_opacity(valcol,opacity_land),oldcol);
        else
          image[c] = merge_ARGB(color_with_opacity(valcol,opacity_sea),oldcol);
      }
      c++;
    }
  }
}

}  // namespace



void ColorPainter_range::setImageColors(uint width,uint height,uint /* loop_step */,uint /* loop_steps */ ,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& /*parameters */)
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

    for (auto& range : ranges)
      paint_range(range,width,height,image,land,values,opacity_land,opacity_sea);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




std::size_t ColorPainter_range::hash_value(const State &theState) const
{
  try
  {
    std::size_t hash = ColorPainter::hash_value(theState);

    for (const auto& range : ranges)
    {
      Fmi::hash_combine(hash, range.value_min);
      Fmi::hash_combine(hash, range.value_max);
      Fmi::hash_combine(hash, Fmi::hash(range.color_min));
      Fmi::hash_combine(hash, Fmi::hash(range.color_max));
      Fmi::hash_combine(hash, Fmi::hash(range.color_low));
      Fmi::hash_combine(hash, Fmi::hash(range.color_high));
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
