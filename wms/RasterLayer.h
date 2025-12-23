// ======================================================================
/*!
 * \brief Isoband layer
 */
// ======================================================================

#pragma once

#include "Heatmap.h"
#include "Intersections.h"
#include "Isoband.h"
#include "IsolineFilter.h"
#include "Layer.h"
#include "Map.h"
#include "ColorPainter.h"
#include "ParameterInfo.h"
#include "Sampling.h"
#include "Smoother.h"
#include "ColorPainter_border.h"
#include "ColorPainter_range.h"
#include "ColorPainter_shadow.h"
#include "ColorPainter_shading.h"
#include <grid-files/common/ImageFunctions.h>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class RasterLayer : public Layer
{
  public:
    void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

    void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

    std::size_t hash_value(const State& theState) const override;

    virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

    std::optional<std::string> parameter;
    std::optional<std::string> direction;
    std::optional<std::string> speed;

    std::string           interpolation{"linear"};

    std::string           unit_conversion;
    int                   compression;
    std::string           painter;
    Parameters            painterParameters;

    ColorPainter_range    landPainter;
    ColorPainter_border   landBorderPainter;
    ColorPainter_range    seaPainter;
    ColorPainter_sptr     dataPainter;
    ColorPainter_border   dataBorderPainter;
    ColorPainter_shadow   dataShadowPainter;
    ColorPainter_shading  shadingPainter;

    std::string           land_position;
    std::string           sea_position;
    std::string           land_border_position;
    std::string           land_shading_position;
    std::string           sea_shading_position;

    Parameters            land_shading_parameters;
    Parameters            sea_shading_parameters;

    std::optional<double> multiplier;
    std::optional<double> offset;

    std::string           svg_image;

  private:

    void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
    void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
    void generate_output(CTPP::CDT &theGlobals, CTPP::CDT &theLayersCdt, State &theState,const T::Coordinate_vec *coordinates,std::vector<float>& values1,std::vector<float>& values2);
    void generate_image(int loopstep,int loopsteps,const T::Coordinate_vec *coordinates,std::vector<float>& values1,std::vector<float>& values2,CImage& cimage);
    bool isNewImageRequired(State &theState);


};  // class RasterLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
