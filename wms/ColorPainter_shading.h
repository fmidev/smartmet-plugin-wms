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
    virtual       ~ColorPainter_shading();

    virtual void  setImageColors(uint width,uint height,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters);
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
