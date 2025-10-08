#include "ColorMap.h"
#include <grid-files/common/GeneralFunctions.h>


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{



ColorMap::ColorMap()
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}





ColorMap::ColorMap(const ColorMap& colorMap)
{
  try
  {
    mColors = colorMap.mColors;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}





ColorMap::~ColorMap()
{
  try
  {
  }
  catch (...)
  {
    Fmi::Exception exception(BCP,"Destructor failed",nullptr);
    exception.printError();
  }
}




void ColorMap::setColors(Colors& colors)
{
  try
  {
    mColors = colors;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}





void ColorMap::getValuesAndColors(std::vector<float>& values,std::vector<unsigned int>& colors)
{
  try
  {
    for (auto it = mColors.begin(); it != mColors.end(); ++it)
    {
      values.emplace_back(it->first);
      colors.emplace_back(it->second);
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}





void ColorMap::getColors(std::vector<float>& values,std::vector<unsigned int>& colors)
{
  try
  {
    colors.reserve(values.size());
    for (auto it = values.begin(); it != values.end(); ++it)
    {
      colors.emplace_back(getColor(*it));
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}




void ColorMap::getSmoothColors(std::vector<float>& values,std::vector<unsigned int>& colors)
{
  try
  {
    colors.reserve(values.size());
    for (auto it = values.begin(); it != values.end(); ++it)
    {
      colors.emplace_back(getSmoothColor(*it));
    }
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}




unsigned int ColorMap::getColor(double value)
{
  try
  {
    auto it = mColors.find(value);
    if (it != mColors.end())
      return it->second;

    it = mColors.lower_bound(value);
    if (it != mColors.end())
    {
      if (it->first > value  && it != mColors.begin())
        it--;

      return it->second;
    }

    if (value > mColors.rbegin()->first)
      return mColors.rbegin()->second;

    if (value < mColors.begin()->first)
      return mColors.begin()->second;

    return 0x00000000;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}




unsigned int ColorMap::getSmoothColor(double value)
{
  try
  {
    auto it = mColors.find(value);
    if (it != mColors.end())
      return it->second;

    double lowerValue = 0;
    double upperValue = 0;
    uint lowerColor = 0;
    uint upperColor = 0;

    uchar *a = (uchar*)&lowerColor;
    uchar *b = (uchar*)&upperColor;

    uint newColor = 0;
    uchar *n = (uchar*)&newColor;

    it = mColors.lower_bound(value);
    if (it != mColors.end())
    {
      if (it->first > value)
      {
        upperValue = it->first;
        upperColor = it->second;
        if (it != mColors.begin())
          it--;

        lowerValue = it->first;
        lowerColor = it->second;
      }

      if (it->first < value)
      {
        lowerValue = it->first;
        lowerColor = it->second;
        if (it != mColors.end())
          it++;

        upperValue = it->first;
        upperColor = it->second;
      }

      double dv = upperValue - lowerValue;
      if (dv != 0)
      {
        double vv = value - lowerValue;
        double p = vv / dv;

        for (uint t=0; t<4; t++)
        {
          int d = ((int)b[t] - (int)a[t]) * p;
          n[t] = a[t] + d;
        }
      }

      return newColor;
    }

    if (value > mColors.rbegin()->first)
      return mColors.rbegin()->second;

    if (value < mColors.begin()->first)
      return mColors.begin()->second;

    return 0xFFFFFF00;
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Operation failed!", nullptr);
  }
}





void ColorMap::print(std::ostream& stream,unsigned int level,unsigned int optionFlags)
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception(BCP, "Constructor failed!", nullptr);
  }
}





}
}
}
