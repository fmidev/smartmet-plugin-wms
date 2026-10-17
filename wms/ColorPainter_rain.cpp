#include "ColorPainter_rain.h"
#include <macgyver/Exception.h>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/GraphFunctions.h>

#include <algorithm>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


ColorPainter_rain::ColorPainter_rain()
{
  try
  {
    colormap_hash = 0;
    line_length = 16;
    streamImage = nullptr;
    streamImage_width = 0;
    streamImage_height = 0;
    smooth_colors = true;
    step_x = 2;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}




ColorPainter_rain::~ColorPainter_rain()
{
    delete [] streamImage;
}



void ColorPainter_rain::init(Json::Value &theJson,const State& theState)
{
  try
  {
    if (theJson.isNull())
      return;

    ColorPainter::init(theJson,theState);

    auto json = JsonTools::remove(theJson, "stream");
    if (!json.isNull())
      initStream(json,theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}





void ColorPainter_rain::initStream(Json::Value &theJson,const State& theState)
{
  try
  {
    if (theJson.isNull())
      return;

    JsonTools::remove_int(line_length, theJson, "line_length");

    JsonTools::remove_int(step_x, theJson, "step_x");

    auto json = JsonTools::remove(theJson, "speeds");
    if (!json.isNull())
      initStreamSpeeds(json,theState);

    if (streamSpeeds.empty())
    {
      StreamSpeed speed;
      addStreamSpeed(speed);
    }

    JsonTools::remove_string(colormap_name, theJson, "colormap");

    std::string smooth = "true";
    JsonTools::remove_string(smooth, theJson, "smooth");
    if (smooth != "true")
      smooth_colors = false;

    JsonTools::remove_bool(smooth_colors, theJson, "smooth_colors");

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
        exception.addParameter("colormap_name",colormap_name);
        throw exception;
      }
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}





void ColorPainter_rain::initStreamSpeeds(Json::Value &theJson, const State &theState)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isArray())
      throw Fmi::Exception(BCP, "StreamSpeed JSON is not a JSON array");

    for (auto& json : theJson)
    {
      if (!json.isNull())
        initStreamSpeed(json,theState);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}





void ColorPainter_rain::initStreamSpeed(Json::Value &theJson, const State & /* theState */)
{
  try
  {
    if (theJson.isNull())
      return;

    StreamSpeed streamSpeed;

    JsonTools::remove_double(streamSpeed.speed, theJson, "speed");
    JsonTools::remove_double(streamSpeed.value_min, theJson, "value_min");
    JsonTools::remove_double(streamSpeed.value_max, theJson, "value_max");


    addStreamSpeed(streamSpeed);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ColorPainter_rain::addStreamSpeed(const StreamSpeed& speed)
{
  try
  {
    streamSpeeds.push_back(speed);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




namespace
{

// Copy the values to the output array with the rows in reverse order
void flip_rows(const std::vector<float>& values,float *out,uint width,uint height)
{
  uint idx = 0;
  for (uint y=0; y<height; y++)
  {
    uint idx2 = (height-y-1) * width;
    for (uint x=0; x<width; x++)
    {
      out[idx] = values[idx2];
      idx++;
      idx2++;
    }
  }
}

}  // namespace

// Create the random stream image of the given size

void ColorPainter_rain::createStreamImage(uint width,uint height)
{
  uint sz = width*height;

  delete [] streamImage;

  streamImage = new uint[sz];
  memset(streamImage,0,sz*4);
  streamImage_width = width;
  streamImage_height = height;
  step_x = std::max(step_x, 1);

  for (uint x=0; x<width; x=x+step_x)
  {
    // NOLINTNEXTLINE(cert-msc50-cpp): rand() is not used for security here
    uint v = abs(rand());
    for (uint y=0; y<height; y++)
    {
      uint idx = (height-y-1) * width + x;
      //uint idx = y * width + x;
      streamImage[idx] = v;
      v++;
    }
  }
}




void ColorPainter_rain::setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& /* parameters */)
{
  try
  {
    uint sz = width*height;
    if (sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    if (sz != values.size())
    {
      Fmi::Exception exception(BCP, "Invalid number of values!");
      exception.addParameter("values1",std::to_string(values.size()));
      throw exception;
    }

    // Rotate

    auto* speed = new float[sz];

    if (!streamImage || streamImage_width != width || streamImage_height != height)
      createStreamImage(width,height);

    flip_rows(values,speed,width,height);

    if (!colormap)
    {
      //printf("No colormap\n");
      delete [] speed;
      return;
    }

    double alphamax = 1.0;
    double alpha_land[line_length];
    double alpha_sea[line_length];

    double mp = alphamax / line_length;
    double vm = (double)line_length / (double)loop_steps;

    for (int t=0; t<line_length; t++)
    {
      double cc = (alphamax - (t*mp))*opacity_land;
      cc = std::min(cc, alphamax);

      alpha_land[t] = cc;

      cc = (alphamax - (t*mp))*opacity_sea;
      cc = std::min(cc, alphamax);

      alpha_sea[t] = cc;
    }

    for (auto& streamSpeed : streamSpeeds)
    {
      int step = (int)(vm * loop_step * streamSpeed.speed);

      uint c = 0;
      for (uint y = 0; y < height; y++)
      {
        uint yy = height - y -1;
        uint p = yy * width;
        for (uint x = 0; x < width; x++)
        {
          uint oldcol = image[c];
          uint cc = p + x;
          uint val = streamImage[c];
          float landval = land[cc];
          if (val != 0 && speed[c] >= streamSpeed.value_min && speed[c] <= streamSpeed.value_max)
          {
            uint speedcol = colormap->getColor(speed[c],smooth_colors);
            uint speedAlpha = (speedcol >> 24);
            uint newAlpha = 0xFF;

            int stp = (step+val) % line_length;
            if (landval > 0.9)
            {
              newAlpha = (uint)(alpha_land[stp] * speedAlpha);
            }
            else
            {
              newAlpha = (uint)(alpha_sea[stp] * speedAlpha);
            }

            newAlpha = std::min<SmartMet::uint>(newAlpha, 0xFF);

            uint valcol = (newAlpha << 24) + (speedcol & 0x00FFFFFF);
            image[c] = merge_ARGB(valcol,oldcol);
          }

          c++;
        }
      }
    }

    delete [] speed;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



std::size_t ColorPainter_rain::hash_value(const State &theState) const
{
  try
  {
    std::size_t hash = ColorPainter::hash_value(theState);

    Fmi::hash_combine(hash,line_length);

    Fmi::hash_combine(hash,Fmi::hash(colormap_name));
    Fmi::hash_combine(hash,colormap_hash);
    Fmi::hash_combine(hash,smooth_colors);

    for (const auto& streamSpeed : streamSpeeds)
    {
      Fmi::hash_combine(hash, streamSpeed.speed);
      Fmi::hash_combine(hash, streamSpeed.value_min);
      Fmi::hash_combine(hash, streamSpeed.value_max);
    }
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
