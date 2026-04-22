#pragma once

#include "ComputedFields.h"
#include "Intersections.h"
#include "Isoline.h"
#include "IsolineFilter.h"
#include "Layer.h"
#include "Map.h"
#include "ParameterInfo.h"
#include "Sampling.h"
#include "Smoother.h"
#include <engines/querydata/Q.h>
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

class IsolineLayer : public Layer
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

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::vector<Isoline> isolines;
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

  // If set, the layer's "parameter" is expected to be the TFP metaparameter
  // name ("TFP"); the underlying scalar is fetched from tfp->field and the
  // TFP field is computed on the fly before contouring.
  std::optional<ComputedFields::TfpOptions> tfp;

  bool strict = false;
  bool validate = false;
  bool desliver = false;

  // Bilinear cell subdivision count (0..10); see IsobandLayer for details.
  int subdivide = 0;

  // See IsobandLayer::subdivide_min_cell_pixels.
  double subdivide_min_cell_pixels = 2.0;

 protected:
  std::vector<OGRGeometryPtr> getIsolines(const std::vector<double>& isovalues, State& theState);
  Engine::Querydata::Q q;  // Make used data available to derived IsolabelLayer
  T::FileId fileId = 0;
  T::MessageIndex messageIndex = 0;

 private:
  std::vector<OGRGeometryPtr> getIsolinesGrid(const std::vector<double>& isovalues,
                                              State& theState);
  std::vector<OGRGeometryPtr> getIsolinesQuerydata(const std::vector<double>& isovalues,
                                                   const State& theState);

};  // class IsolineLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
