#include "ColorPainter_stream.h"
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


namespace
{

// Throw if the number of values does not match the image size
void check_value_count(uint sz,const std::vector<float>& values,const std::string& name)
{
  if (sz != values.size())
  {
    Fmi::Exception exception(BCP, "Invalid number of " + name + "!");
    exception.addParameter(name.c_str(),std::to_string(values.size()));
    throw exception;
  }
}

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

// Scale the alpha of the color by the opacity unless the opacity is 1
inline uint with_opacity(uint valcol,double opacity)
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

}  // namespace


ColorPainter_stream::ColorPainter_stream()
{
  try
  {
    colormap_hash = 0;
    trace_step_x = 10;
    trace_step_y = 10;
    trace_length_min = 10;
    trace_length_max = 128;
    line_length = 16;
    line_color_land = 0xFF000000;
    line_color_sea = 0xFF000000;
    smooth_colors = true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Constructor failed!");
  }
}

ColorPainter_stream::~ColorPainter_stream() = default;

void ColorPainter_stream::init(Json::Value &theJson,const State& theState)
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





void ColorPainter_stream::initStream(Json::Value &theJson,const State& theState)
{
  try
  {
    if (theJson.isNull())
      return;

    JsonTools::remove_int(trace_step_x, theJson, "trace_step_x");
    JsonTools::remove_int(trace_step_y, theJson, "trace_step_y");
    JsonTools::remove_int(trace_length_min, theJson, "trace_length_min");
    JsonTools::remove_int(trace_length_max, theJson, "trace_length_max");

    JsonTools::remove_int(line_length, theJson, "line_length");

    auto json = JsonTools::remove(theJson, "speeds");
    if (!json.isNull())
      initStreamSpeeds(json,theState);

    if (streamSpeeds.empty())
    {
      StreamSpeed speed;
      addStreamSpeed(speed);
    }

    std::string col = "FF000000";
    JsonTools::remove_string(col, theJson, "line_color_land");
    line_color_land = argb(col.c_str());

    col = "FF000000";
    JsonTools::remove_string(col, theJson, "line_color_sea");
    line_color_sea = argb(col.c_str());

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





void ColorPainter_stream::initStreamSpeeds(Json::Value &theJson, const State &theState)
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





void ColorPainter_stream::initStreamSpeed(Json::Value &theJson, const State & /* theState */)
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

void ColorPainter_stream::addStreamSpeed(const StreamSpeed& speed)
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




void ColorPainter_stream::setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& /* parameters */)
{
  try
  {
    uint sz = width*height;
    if (sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    check_value_count(sz,values,"values");

    // Rotate

    auto* direction = new float[sz];
    flip_rows(values,direction,width,height);

    uint *streamImage = new uint[sz];
    getStreamlineImage(direction,nullptr,streamImage,width,height,trace_step_x,trace_step_y,trace_length_min,trace_length_max);

    uint alphamax = 0xFF;
    uint color_land[line_length];
    uint color_sea[line_length];

    double mp = (double)alphamax / (double)line_length;
    double vm = (double)line_length / (double)loop_steps;
    int step = (int)(vm * loop_step);

    for (int t=0; t<line_length; t++)
    {
      uint cc = (alphamax - (uint)(t*mp))*opacity_land;
      cc = std::min(cc, alphamax);

      color_land[t] = (cc << 24) + line_color_land;

      cc = (alphamax - (uint)(t*mp))*opacity_sea;
      cc = std::min(cc, alphamax);

      color_sea[t] = (cc << 24) + line_color_sea;
    }


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
        if (val != 0)
        {
          int stp = (step+val) % line_length;
          if (landval > 0.9)
            image[c] = merge_ARGB(with_opacity(color_land[stp],opacity_land),oldcol);
          else
            image[c] = merge_ARGB(with_opacity(color_sea[stp],opacity_sea),oldcol);
        }
        c++;
      }
    }

    delete [] direction;
    delete [] streamImage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



void ColorPainter_stream::setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values1,std::vector<float>& values2,Parameters& /* parameters */)
{
  try
  {
    uint sz = width*height;
    if (sz == 0)
    {
      Fmi::Exception exception(BCP, "The image size is zero!");
      throw exception;
    }

    check_value_count(sz,values1,"values1");
    check_value_count(sz,values2,"values2");

    // Rotate

    auto* direction = new float[sz];
    auto* speed = new float[sz];
    flip_rows(values1,direction,width,height);
    flip_rows(values2,speed,width,height);

    uint *streamImage = new uint[sz];
    getStreamlineImage(direction,nullptr,streamImage,width,height,trace_step_x,trace_step_y,trace_length_min,trace_length_max);

    if (!colormap)
    {
      //printf("No colormap\n");
      delete [] speed;
      delete [] direction;
      delete [] streamImage;
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
    delete [] direction;
    delete [] streamImage;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}



std::size_t ColorPainter_stream::hash_value(const State &theState) const
{
  try
  {
    std::size_t hash = ColorPainter::hash_value(theState);

    Fmi::hash_combine(hash,trace_step_x);
    Fmi::hash_combine(hash,trace_step_y);
    Fmi::hash_combine(hash,trace_length_min);
    Fmi::hash_combine(hash,trace_length_max);

    Fmi::hash_combine(hash,line_length);
    Fmi::hash_combine(hash,line_color_land);
    Fmi::hash_combine(hash,line_color_sea);

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
