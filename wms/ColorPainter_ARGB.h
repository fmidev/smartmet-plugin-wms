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
                  ~ColorPainter_ARGB() override;

    void          init(Json::Value &theJson,const State& theState) override;
    void          setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) override;
    std::size_t   hash_value(const State &theState) const override;

  protected:

    ColorMap_sptr colormap;
    std::string   colormap_name;
    std::size_t   colormap_hash;
    bool          smooth_colors;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
