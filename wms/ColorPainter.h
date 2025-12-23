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

typedef std::map<std::string,std::string> Parameters;


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

  public:

    // Data opacity
    double opacity_land;
    double opacity_sea;
};


typedef std::shared_ptr<ColorPainter> ColorPainter_sptr;
typedef std::map<std::string,ColorPainter_sptr> ColorPainter_sptr_map;


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
