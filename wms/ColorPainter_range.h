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
                  ColorPainter_range();
    virtual       ~ColorPainter_range();

    virtual void  setImageColors(uint width,uint height,uint *image,std::vector<float>& values,Parameters& parameters);
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
