#pragma once

#include "ColorPainter.h"
#include "ColorMap.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


class ColorPainter_range : public ColorPainter
{
  public:

    class Range
    {
      public:
       Range() { color_high = 0x00000000; }

       double value_min{0};
       double value_max{0};
       uint color_min{0x00000000};
       uint color_max{0x00000000};
       uint color_low{0x00000000};
       uint color_high;
    };

                  ColorPainter_range();
                  ~ColorPainter_range() override;

    void          init(Json::Value &theJson,const State& theState) override;
    void          initRanges(Json::Value &theJson,const State& theState);
    void          initRange(Json::Value &theJson,const State& theState);

    void addRange(const Range& range);
    int           getRangeCount() {return ranges.size();}
    void          setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) override;
    std::size_t   hash_value(const State &theState) const override;

  protected:

    std::vector<Range> ranges;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
