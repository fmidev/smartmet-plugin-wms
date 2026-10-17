#include "ColorPainter_border.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


ColorPainter_border::ColorPainter_border()
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}

ColorPainter_border::~ColorPainter_border() = default;

void ColorPainter_border::init(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    ColorPainter::init(theJson,theState);

    auto json = JsonTools::remove(theJson, "borders");
    if (!json.isNull())
      initBorders(json,theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_border::initBorders(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isArray())
      throw Fmi::Exception(BCP, "Borders JSON is not a JSON array");

    for (auto& json : theJson)
    {
      if (!json.isNull())
        initBorder(json,theState);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void ColorPainter_border::initBorder(Json::Value &theJson, const State & /* theState */)
{
  try
  {
    if (theJson.isNull())
      return;

    Border border;

    JsonTools::remove_double(border.inside_value_min, theJson, "inside_value_min");
    JsonTools::remove_double(border.step_increase, theJson, "step_increase");
    JsonTools::remove_int(border.steps, theJson, "steps");

    std::string col = "00000000";
    JsonTools::remove_string(col, theJson, "inside_color");
    border.inside_color = argb(col);

    col = "00000000";
    JsonTools::remove_string(col, theJson, "outside_color");
    border.outside_color = argb(col);

    addBorder(border);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ColorPainter_border::addBorder(const Border& border)
{
  try
  {
    borders.push_back(border);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




namespace
{

// Border colors on land and on sea
struct BorderColors
{
  uint inside_land = 0;
  uint inside_sea = 0;
  uint outside_land = 0;
  uint outside_sea = 0;
};

// Paint the colors of the previous and the current pixel if they are on different sides of the border
inline void paint_transition(uint *image,uint prevpos,double prevland,bool previnside,uint pos,double landval,bool inside,const BorderColors& colors)
{
  if (!previnside  &&  inside)
  {
    if (prevland > 0.9)
      image[prevpos] = merge_ARGB(colors.outside_land,image[prevpos]);
    else
      image[prevpos] = merge_ARGB(colors.outside_sea,image[prevpos]);

    if (landval > 0.9)
      image[pos] = merge_ARGB(colors.inside_land,image[pos]);
    else
      image[pos] = merge_ARGB(colors.inside_sea,image[pos]);
  }
  else
  if (previnside  &&  !inside)
  {
    if (prevland > 0.9)
      image[prevpos] = merge_ARGB(colors.inside_land,image[prevpos]);
    else
      image[prevpos] = merge_ARGB(colors.inside_sea,image[prevpos]);

    if (landval > 0.9)
      image[pos] = merge_ARGB(colors.outside_land,image[pos]);
    else
      image[pos] = merge_ARGB(colors.outside_sea,image[pos]);
  }
}

// Paint the border of the area where values are at least minVal
void paint_border(uint width,uint height,uint *image,const std::vector<float>& land,const std::vector<float>& values,double minVal,const BorderColors& colors)
{
  std::vector<bool> prevInsideY(width, false);
  std::vector<uint> prevInsidePos(width, 0);
  std::vector<double> prevLandVal(width, 0.0);

  uint c = 0;
  for (uint y = 0; y < height; y++)
  {
    uint yy = height - y -1;
    uint p = yy * width;
    bool prevInside = false;
    double prevLand = 0.0;
    for (uint x = 0; x < width; x++)
    {
      uint cc = p + x;
      float val = values[cc];
      float landval = land[cc];
      bool inside = true;
      if (val < minVal)
        inside = false;

      if (x > 0)
        paint_transition(image,c-1,prevLand,prevInside,c,landval,inside,colors);

      if (y > 0)
        paint_transition(image,prevInsidePos[x],prevLandVal[x],prevInsideY[x],c,landval,inside,colors);

      prevInside = inside;
      prevLand = landval;
      prevLandVal[x] = landval;
      prevInsideY[x] = inside;
      prevInsidePos[x] = c;
      c++;
    }
  }
}

}  // namespace



void ColorPainter_border::setImageColors(uint width,uint height,uint /* loop_step */,uint /* loop_steps */,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& /* parameters */)
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

    for (auto& border : borders)
    {
      // Border colors on land and on sea
      BorderColors colors;
      colors.inside_land = color_with_opacity(border.inside_color,opacity_land);
      colors.inside_sea = color_with_opacity(border.inside_color,opacity_sea);
      colors.outside_land = color_with_opacity(border.outside_color,opacity_land);
      colors.outside_sea = color_with_opacity(border.outside_color,opacity_sea);

      double minVal = border.inside_value_min;
      for (uint t=0; t<(uint)border.steps; t++)
      {
        paint_border(width,height,image,land,values,minVal,colors);
        minVal = minVal + border.step_increase;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




std::size_t ColorPainter_border::hash_value(const State &theState) const
{
  try
  {
    std::size_t hash = ColorPainter::hash_value(theState);

    for (const auto& border : borders)
    {
      Fmi::hash_combine(hash, border.inside_value_min);
      Fmi::hash_combine(hash, Fmi::hash(border.inside_color));
      Fmi::hash_combine(hash, Fmi::hash(border.outside_color));
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
