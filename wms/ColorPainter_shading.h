#pragma once

#include "ColorPainter.h"
#include "ColorMap.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


class ColorPainter_shading : public ColorPainter
{
  public:
                  ColorPainter_shading();
                  ~ColorPainter_shading() override;

            void  setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) override;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
