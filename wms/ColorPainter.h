#pragma once

#include <map>
#include <vector>
#include <memory>
#include <sys/types.h>


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
                  ColorPainter() {}
    virtual       ~ColorPainter() {}

    virtual void  setImageColors(uint width,uint height,uint *image,std::vector<float>& values,Parameters& parameters) {}
};


typedef std::shared_ptr<ColorPainter> ColorPainter_sptr;
typedef std::map<std::string,ColorPainter_sptr> ColorPainter_sptr_map;


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
