#pragma once

#include "ColorPainter.h"
#include "ColorMap.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


class ColorPainter_border : public ColorPainter
{
  public:

    class Border
    {
      public:
       Border() {}

       double inside_value_min{0};
       double step_increase{0};
       uint inside_color{0x00000000};
       uint outside_color{0x00000000};
       int steps{1};
    };

                  ColorPainter_border();
                  ~ColorPainter_border() override;

    void          init(Json::Value &theJson,const State& theState) override;
    void          initBorders(Json::Value &theJson,const State& theState);
    void          initBorder(Json::Value &theJson,const State& theState);

    void addBorder(const Border& border);
    int           getBorderCount() {return borders.size();}
    void          setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) override;
    std::size_t   hash_value(const State &theState) const override;

  protected:

    std::vector<Border> borders;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
