#pragma once

#include "ColorPainter.h"
#include "ColorMap.h"


namespace SmartMet
{
namespace Plugin
{
namespace Dali
{


class ColorPainter_rain : public ColorPainter
{
  public:

    class StreamSpeed
    {
      public:

        StreamSpeed()
        {
          speed = 1.0;
          value_min = -1000000000;
          value_max = 1000000000;
        }

        double speed;
        double value_min;
        double value_max;
    };

                  ColorPainter_rain();
                  ~ColorPainter_rain() override;

    void          init(Json::Value &theJson,const State& theState) override;
    void          initStream(Json::Value &theJson,const State& theState);
    void          initStreamSpeeds(Json::Value &theJson,const State& theState);
    void          initStreamSpeed(Json::Value &theJson,const State& theState);

    void          addStreamSpeed(StreamSpeed& speed);
    bool          isAnimator() override {return true;}
    void          setImageColors(uint width,uint height,uint loop_step,uint loop_steps,uint *image,std::vector<float>& land,std::vector<float>& values,Parameters& parameters) override;
    std::size_t   hash_value(const State &theState) const override;

  protected:

    int           line_length;
    std::size_t   colormap_hash;
    std::string   colormap_name;
    ColorMap_sptr colormap;
    bool          smooth_colors;
    uint          *streamImage;
    uint          streamImage_width;
    uint          streamImage_height;
    int           step_x;
    std::vector<StreamSpeed> streamSpeeds;
};


}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
