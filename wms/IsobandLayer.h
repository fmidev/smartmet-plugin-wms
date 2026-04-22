// ======================================================================
/*!
 * \brief Isoband layer
 */
// ======================================================================

#pragma once

#include "ComputedFields.h"
#include "Heatmap.h"
#include "Intersections.h"
#include "Isoband.h"
#include "IsolineFilter.h"
#include "Layer.h"
#include "Map.h"
#include "ParameterInfo.h"
#include "Sampling.h"
#include "Smoother.h"
#include <optional>
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

class IsobandLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;
  std::string generateGeoTiff(State& theState) override;
  std::string generateDataTile(State& theState) override;
  void addMVTLayer(class MVTTileBuilder& builder, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::vector<Isoband> isobands;
  std::string interpolation{"linear"};
  Smoother smoother;

  int extrapolation = 0;

  double precision = 1.0;

  std::optional<double> minarea;
  std::string areaunit = "km^2";  // km^2|px^2

  std::optional<Map> outside;
  std::optional<Map> inside;

  Sampling sampling;
  Intersections intersections;
  IsolineFilter filter;

  Heatmap heatmap;

  // If set, the layer's "parameter" is the TFP metaparameter name
  // ("TFP"); the underlying scalar is fetched from tfp->field and the
  // TFP field is computed on the fly before contouring.
  std::optional<ComputedFields::TfpOptions> tfp;

  bool closed_range = true;
  bool strict = false;
  bool validate = false;
  bool desliver = false;

  // Bilinear cell subdivision count (0..4). When >0, inserts up to N-1 interior
  // samples on the bilinear level curve between consecutive edge intersections,
  // smoothing diamond-like artifacts caused by lone high pixels.
  int subdivide = 0;

 private:
  virtual void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  virtual void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  std::shared_ptr<Engine::Querydata::QImpl> buildHeatmap(const Spine::Parameter& theParameter,
                                                         const Fmi::DateTime& theTime,
                                                         const State& theState);

};  // class IsobandLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
