#pragma once

#include "ColorPainter.h"
#include "ColorMap.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


class ColorPainter_shadow : public ColorPainter
{
  public:

    class Shadow
    {
      public:

        Shadow()
        {
          value_min = 0;
          value_max = 0;
          color_min = 0x00000000;
          color_max = 0x40000000;
          dx = 10;
          dy = 10;
        }

        double value_min;
        double value_max;
        uint color_min;
        uint color_max;
        int dx;
        int dy;
    };

  public:
                  ColorPainter_shadow();
                  ~ColorPainter_shadow() override;

    void          init(Json::Value &theJson,const State& theState) override;
    void          initShadows(Json::Value &theJson,const State& theState);
    void          initShadow(Json::Value &theJson,const State& theState);

    void          addShadow(Shadow& shadow);
    int           getShadowCount() {return shadows.size();}
    void          setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) override;
    std::size_t   hash_value(const State &theState) const override;

  protected:

    std::vector<Shadow> shadows;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
