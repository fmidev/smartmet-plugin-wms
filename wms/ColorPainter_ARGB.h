#pragma once

#include "ColorPainter.h"
#include "ColorMap.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


class ColorPainter_ARGB : public ColorPainter
{
  public:
                  ColorPainter_ARGB();
    virtual       ~ColorPainter_ARGB();

    virtual void  addColorMap(std::string name,ColorMap_sptr colorMap);
    virtual void  addColorMap(std::string name,std::string& colorMap);
    virtual void  setImageColors(uint width,uint height,uint *image,std::vector<float>& values,Parameters& parameters);

  protected:

    ColorMap_sptr_map colorMaps;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
