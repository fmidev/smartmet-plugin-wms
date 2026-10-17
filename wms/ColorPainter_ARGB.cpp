#include "ColorPainter_ARGB.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

ColorPainter_ARGB::ColorPainter_ARGB()
{
  try
  {
    smooth_colors = true;
    colormap_hash = 0;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}

ColorPainter_ARGB::~ColorPainter_ARGB() = default;

void ColorPainter_ARGB::init(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    ColorPainter::init(theJson,theState);

    std::string smooth = "true";
    JsonTools::remove_string(smooth, theJson, "smooth");
    if (smooth != "true")
      smooth_colors = false;

    JsonTools::remove_bool(smooth_colors, theJson, "smooth_colors");

    JsonTools::remove_string(colormap_name, theJson, "colormap");

    if (!colormap_name.empty())
    {
      std::string cmap = theState.getColorMap(colormap_name);
      if (!cmap.empty())
      {
        colormap_hash = Fmi::hash(cmap);
        colormap = std::make_shared<ColorMap>(cmap);
      }
      else
      {
        Fmi::Exception exception(BCP, "Cannot find the colormap!");
        exception.addParameter("colormap_name", colormap_name);
        throw exception;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



void ColorPainter_ARGB::setImageColors(uint width, uint height, uint /* loop_step */, uint /* loop_steps */, uint *image, std::vector<float> &land,
                                       std::vector<float> &values, Parameters & /* parameters */)
{
  try
  {
    uint sz = width * height;
    if (sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    if (sz != values.size())
    {
      Fmi::Exception exception(BCP, "Invalid number of values!");
      exception.addParameter("values", std::to_string(values.size()));
      throw exception;
    }

    if (!colormap)
      return;

    uint c = 0;
    for (uint y = 0; y < height; y++)
    {
      uint yy = height - y - 1;
      uint p = yy * width;
      for (uint x = 0; x < width; x++)
      {
        uint oldcol = image[c];
        uint cc = p + x;
        float val = values[cc];
        float landval = land[cc];
        if (val != ParamValueMissing)
        {
          uint valcol = colormap->getColor(val, smooth_colors);
          if (landval > 0.9)
            image[c] = merge_ARGB(color_with_opacity(valcol, opacity_land), oldcol);
          else
            image[c] = merge_ARGB(color_with_opacity(valcol, opacity_sea), oldcol);
        }
        c++;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



std::size_t ColorPainter_ARGB::hash_value(const State &theState) const
{
  try
  {
    std::size_t hash = ColorPainter::hash_value(theState);

    Fmi::hash_combine(hash,Fmi::hash(colormap_name));
    Fmi::hash_combine(hash,colormap_hash);
    Fmi::hash_combine(hash,smooth_colors);

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
