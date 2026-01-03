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

        Border()
        {
          inside_value_min = 0;
          inside_color = 0x00000000;
          outside_color = 0x00000000;
          step_increase = 0;
          steps = 1;
        }

        double inside_value_min;
        double step_increase;
        uint inside_color;
        uint outside_color;
        int steps;
    };

  public:
                  ColorPainter_border();
                  ~ColorPainter_border() override;

    void          init(Json::Value &theJson,const State& theState) override;
    void          initBorders(Json::Value &theJson,const State& theState);
    void          initBorder(Json::Value &theJson,const State& theState);

    void          addBorder(Border& border);
    int           getBorderCount() {return borders.size();}
    void          setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) override;
    std::size_t   hash_value(const State &theState) const override;

  protected:

    std::vector<Border> borders;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
