#pragma once

#include <map>
#include <vector>
#include <memory>
#include <sys/types.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

typedef std::map<float, unsigned int> Colors;

class ColorMap
{
  public:
    ColorMap();
    ColorMap(const ColorMap &colorMap);
    ColorMap(std::string &colorMap);
    virtual ~ColorMap();

    virtual void addColor(float value, unsigned int color);
    virtual void setColors(Colors &colors);

    virtual void getColors(std::vector<float> &values, std::vector<unsigned int> &colors);
    virtual void getSmoothColors(std::vector<float> &values, std::vector<unsigned int> &colors);

    void getValuesAndColors(std::vector<float> &values, std::vector<unsigned int> &colors);

    void print(std::ostream &stream, uint level, uint optionFlags);

  protected:

    Colors mColors;

  public:

    uint getColor(double value, bool smooth)
    {
      if (smooth)
        return getSmoothColor(value);

      return getColor(value);
    }

    uint getColor(double value)
    {
      auto it = mColors.find(value);
      if (it != mColors.end())
        return it->second;

      it = mColors.lower_bound(value);
      if (it != mColors.end())
      {
        if (it->first > value && it != mColors.begin())
          it--;

        return it->second;
      }

      if (value > mColors.rbegin()->first)
        return mColors.rbegin()->second;

      if (value < mColors.begin()->first)
        return mColors.begin()->second;

      return 0x00000000;
    }

    uint getSmoothColor(double value)
    {
      if (mColors.empty())
        return 0;

      auto it = mColors.find(value);
      if (it != mColors.end())
        return it->second;

      double lowerValue = 0;
      double upperValue = 0;
      uint lowerColor = 0;
      uint upperColor = 0;

      unsigned char *a = (unsigned char*) &lowerColor;
      unsigned char *b = (unsigned char*) &upperColor;

      uint newColor = 0;
      unsigned char *n = (unsigned char*) &newColor;

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

          for (uint t = 0; t < 4; t++)
          {
            int d = ((int) b[t] - (int) a[t]) * p;
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
};

typedef std::shared_ptr<ColorMap> ColorMap_sptr;
typedef std::map<std::string, ColorMap_sptr> ColorMap_sptr_map;

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
