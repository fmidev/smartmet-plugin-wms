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
#include "ColorPainter_shading.h"
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
    std::vector<Isoband> isobands;
    std::string interpolation{"linear"};

    std::string unit_conversion;
    int compression;
    std::string painter;
    Parameters painterParameters;
    ColorPainter_shading shadingPainter;

    std::string land_color;
    std::string land_position;

    std::string sea_color;
    std::string sea_position;

    std::string landShading_position;
    Parameters landShading_parameters;

    std::string seaShading_position;
    Parameters seaShading_parameters;

    std::optional<double> multiplier;
    std::optional<double> offset;
    ColorPainter_sptr_map colorPainters;

  private:
    virtual void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
    virtual void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

    //void setColors_1(uint width,uint height,uint *image,std::vector<float>& values,uint alfa);
    //void setColors(uint width,uint height,uint *image,std::vector<float>& values,ColorMap& colorMap,uint alfa);

};  // class RasterLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
