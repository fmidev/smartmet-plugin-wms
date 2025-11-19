#pragma once

#include "ColorPainter.h"
#include "ColorMap.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


class ColorPainter_stream : public ColorPainter
{
  public:
                  ColorPainter_stream();
    virtual       ~ColorPainter_stream();

    virtual void  addColorMap(std::string name,ColorMap_sptr colorMap);
    virtual void  addColorMap(std::string name,std::string& colorMap);
    virtual void  setImageColors(uint width,uint height,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters);
    virtual void  setImageColors(uint width,uint height,uint *image,std::vector<float>& land,std::vector<float>& values1,std::vector<float>& values2,Parameters& parameters);

  protected:

    ColorMap_sptr_map colorMaps;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
