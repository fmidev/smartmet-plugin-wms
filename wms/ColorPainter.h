#pragma once

#include <map>
#include <vector>
#include <memory>
#include <sys/types.h>
#include <json/json.h>
#include "JsonTools.h"
#include "State.h"
#include "Hash.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

using Parameters = std::map<std::string, std::string>;

class ColorPainter
{
  public:
                  ColorPainter();
    virtual       ~ColorPainter();

    virtual void  init(Json::Value &theJson,const State& theState);

    virtual bool  isAnimator() {return false;}
    virtual void  setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) {}
    virtual void  setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values1,std::vector<float>& values2,Parameters& parameters) {}

    virtual std::size_t hash_value(const State &theState) const;

    // Data opacity
    double opacity_land;
    double opacity_sea;
};

// Scale the alpha of the color by the opacity unless the opacity is 1
inline uint color_with_opacity(uint valcol,double opacity)
{
  if (opacity == 1.0)
    return valcol;

  uint op = (uint)((double)((valcol & 0xFF000000) >> 24) * opacity);
  if (op > 255)
    op = 0xFF000000;
  else
    op = (op & 0xFF) << 24;

  return op + (valcol & 0x00FFFFFF);
}

using ColorPainter_sptr = std::shared_ptr<ColorPainter>;
using ColorPainter_sptr_map = std::map<std::string, ColorPainter_sptr>;

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
