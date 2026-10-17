// ======================================================================
/*!
 * \brief Symbol layer
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Layer.h"
#include "ObservationSources.h"
#include "ParameterInfo.h"
#include "PointValueOptions.h"
#include "Positions.h"
#include <timeseries/ParameterFactory.h>
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

class SymbolLayer : public Layer
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

 private:
  bool symbolTag(CTPP::CDT& theGlobals,
                 CTPP::CDT& tag_cdt,
                 State& theState,
                 const Positions::Point& point,
                 double value) const;
#ifndef WITHOUT_OBSERVATION
  std::vector<PointData> readObservations(const State& theState,
                                          const Fmi::SpatialReference& crs,
                                          const Fmi::Box& box,
                                          const Fmi::DateTime& valid_time,
                                          const Fmi::TimePeriod& valid_time_period);
#endif
  void selectLevel(const Engine::Querydata::Q& q) const;
  void addGridParameter(const Engine::Grid::Engine& gridEngine,
                        const std::string& producerName,
                        T::AttributeList& attributeList,
                        QueryServer::Query& query);
  int addGridSymbols(CTPP::CDT& theGlobals,
                     CTPP::CDT& group_cdt,
                     State& theState,
                     const std::vector<PointData>& pointvalues,
                     double xmultiplier,
                     double xoffset) const;
  int addSymbols(CTPP::CDT& theGlobals,
                 CTPP::CDT& group_cdt,
                 State& theState,
                 const std::vector<PointData>& pointvalues,
                 double xmultiplier,
                 double xoffset,
                 const Fmi::TimePeriod& valid_time_period) const;
#ifndef WITHOUT_OBSERVATION
  std::optional<std::uint64_t> flashGeneration(const State& theState) const;
#endif

 public:

  virtual void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  std::optional<TS::ParameterAndFunctions> param_funcs;

  std::optional<Positions> positions;

  // Require at least this many valid valies
  int minvalues = 0;

  // the name of the default symbol
  std::optional<std::string> symbol;
  std::optional<double> scale;

  // optional adjustment for all symbols
  std::optional<int> dx;
  std::optional<int> dy;

  // The parameter values or ranges to be styled separately
  std::vector<AttributeSelection> symbols;

  // Minimum distance, priorities of symbols
  PointValueOptions point_value_options;

  // Several observation parameters combined, used instead of paraminfo.parameter if set
  ObservationSources sources;

 private:
  virtual void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  virtual void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

};  // class SymbolLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
