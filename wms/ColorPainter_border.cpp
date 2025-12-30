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




ColorPainter_border::~ColorPainter_border()
{
}



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




void ColorPainter_border::initBorder(Json::Value &theJson, const State &theState)
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




void ColorPainter_border::addBorder(Border& border)
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




void ColorPainter_border::setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters)
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

    for (auto border = borders.begin(); border != borders.end(); ++border)
    {
      double minVal = border->inside_value_min;
      double stepIncrease = border->step_increase;
      uint insideCol = border->inside_color;
      uint outsideCol = border->outside_color;
      uint steps = border->steps;

      uint insideCol_land = insideCol;
      uint insideCol_sea = insideCol;
      uint outsideCol_land = outsideCol;
      uint outsideCol_sea = outsideCol;

      uint o = (uint)((double)((outsideCol & 0xFF000000) >> 24));
      uint i = (uint)((double)((insideCol & 0xFF000000) >> 24));

      if (opacity_land != 1.0)
      {
        uint op = (uint) o * opacity_land;
        if (op > 255)
          op = 0xFF000000;
        else
          op = (op & 0xFF) << 24;

        outsideCol_land = op + (outsideCol & 0x00FFFFFF);

        op = (uint) i * opacity_land;
        if (op > 255)
          op = 0xFF000000;
        else
          op = (op & 0xFF) << 24;

        insideCol_land = op + (insideCol & 0x00FFFFFF);
      }

      if (opacity_sea != 1.0)
      {
        uint op = (uint) o * opacity_sea;
        if (op > 255)
          op = 0xFF000000;
        else
          op = (op & 0xFF) << 24;

        outsideCol_sea = op + (outsideCol & 0x00FFFFFF);

        op = (uint) i * opacity_sea;
        if (op > 255)
          op = 0xFF000000;
        else
          op = (op & 0xFF) << 24;

        insideCol_sea = op + (insideCol & 0x00FFFFFF);
      }


      for (uint t=0; t<steps; t++)
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
            {
              if (!prevInside  &&  inside)
              {
                if (prevLand > 0.9)
                  image[c-1] = merge_ARGB(outsideCol_land,image[c-1]);
                else
                  image[c-1] = merge_ARGB(outsideCol_sea,image[c-1]);

                if (landval > 0.9)
                  image[c] = merge_ARGB(insideCol_land,image[c]);
                else
                  image[c] = merge_ARGB(insideCol_sea,image[c]);
              }
              else
              if (prevInside  &&  !inside)
              {
                if (prevLand > 0.9)
                  image[c-1] = merge_ARGB(insideCol_land,image[c-1]);
                else
                  image[c-1] = merge_ARGB(insideCol_sea,image[c-1]);

                if (landval > 0.9)
                  image[c] = merge_ARGB(outsideCol_land,image[c]);
                else
                  image[c] = merge_ARGB(outsideCol_sea,image[c]);
              }
            }

            if (y > 0)
            {
              uint pp = prevInsidePos[x];
              bool prevIn = prevInsideY[x];
              double prevL = prevLandVal[x];
              if (!prevIn  &&  inside)
              {
                if (prevL > 0.9)
                  image[pp] = merge_ARGB(outsideCol_land,image[pp]);
                else
                  image[pp] = merge_ARGB(outsideCol_sea,image[pp]);

                if (landval > 0.9)
                  image[c] = merge_ARGB(insideCol_land,image[c]);
                else
                  image[c] = merge_ARGB(insideCol_sea,image[c]);
              }
              else
              if (prevIn  &&  !inside)
              {
                if (prevL > 0.9)
                  image[pp] = merge_ARGB(insideCol_land,image[pp]);
                else
                  image[pp] = merge_ARGB(insideCol_sea,image[pp]);

                if (landval > 0.9)
                  image[c] = merge_ARGB(outsideCol_land,image[c]);
                else
                  image[c] = merge_ARGB(outsideCol_sea,image[c]);
              }
            }

            prevInside = inside;
            prevLand = landval;
            prevLandVal[x] = landval;
            prevInsideY[x] = inside;
            prevInsidePos[x] = c;
            c++;
          }
        }
        minVal = minVal + stepIncrease;
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

    for (auto border = borders.begin(); border != borders.end(); ++border)
    {
      Fmi::hash_combine(hash,border->inside_value_min);
      Fmi::hash_combine(hash,Fmi::hash(border->inside_color));
      Fmi::hash_combine(hash,Fmi::hash(border->outside_color));
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
