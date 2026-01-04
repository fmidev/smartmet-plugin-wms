#include "ColorPainter.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

ColorPainter::ColorPainter()
{
  try
  {
    // Data opacity
    opacity_land = 1.0;
    opacity_sea = 1.0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}




ColorPainter::~ColorPainter()
{
}





void ColorPainter::init(Json::Value &theJson, const State &/* theState */)
{
  try
  {
    if (theJson.isNull())
      return;

    JsonTools::remove_double(opacity_land, theJson, "data_opacity_land");
    JsonTools::remove_double(opacity_sea, theJson, "data_opacity_sea");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}





std::size_t ColorPainter::hash_value(const State & /* theState */) const
{
  try
  {
    std::size_t hash = 0;
    Fmi::hash_combine(hash,opacity_land);
    Fmi::hash_combine(hash,opacity_sea);
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
