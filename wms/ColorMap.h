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

typedef std::map<float,unsigned int> Colors;


class ColorMap
{
  public:
                      ColorMap();
                      ColorMap(const ColorMap& colorMap);
    virtual           ~ColorMap();

    virtual void      setColors(Colors& colors);

    virtual uint      getColor(double value);
    virtual void      getColors(std::vector<float>& values,std::vector<unsigned int>& colors);
    virtual uint      getSmoothColor(double value);
    virtual void      getSmoothColors(std::vector<float>& values,std::vector<unsigned int>& colors);

    void              getValuesAndColors(std::vector<float>& values,std::vector<unsigned int>& colors);

    void              print(std::ostream& stream,uint level,uint optionFlags);

  protected:

    Colors            mColors;
};


typedef std::shared_ptr<ColorMap> ColorMap_sptr;
typedef std::map<std::string,ColorMap_sptr> ColorMap_sptr_map;


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
