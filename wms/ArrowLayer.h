// ======================================================================
/*!
 * \brief Arrow layer with possible speed range styling
 */
// ======================================================================

#pragma once

#include "AttributeSelection.h"
#include "Layer.h"
#include "ParameterInfo.h"
#include "PointValueOptions.h"
#include "Positions.h"
#include "Projection.h"
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

class ArrowLayer : public Layer
{
 public:
  ArrowLayer() = default;
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;
  std::string generateGeoTiff(State& theState) override;
  void addMVTLayer(class MVTTileBuilder& builder, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  // Direction and speed parameters
  std::optional<std::string> direction;
  std::optional<std::string> speed;
  // U- and V-components, mutually exclusive with above settings
  std::optional<std::string> u;
  std::optional<std::string> v;

  std::optional<double> fixedspeed;
  std::optional<double> fixeddirection;

  std::optional<double> minrotationspeed;
  std::optional<std::string> symbol;
  std::optional<double> scale;
  bool southflop = false;
  bool northflop = false;
  bool flip = false;

  // Arrow position generator
  std::optional<Positions> positions;
  std::optional<int> dx;
  std::optional<int> dy;

  // Require at least this many valid values
  int minvalues = 0;

  // The speed parameter and the ranges to be styled separately
  std::vector<AttributeSelection> arrows;

  // Minimum distance, priorities of arrows
  PointValueOptions point_value_options;

 private:
  void generate_gridEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);
  void generate_qEngine(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  void getObservationValue(CTPP::CDT& theInfo, const State& theState);
  void getQuerydataValue(CTPP::CDT& theInfo, const State& theState);
  void getGridValue(CTPP::CDT& theInfo, const State& theState);

  // Shared helpers
  void render_arrows(CTPP::CDT& theGlobals,
                     CTPP::CDT& group_cdt,
                     const std::vector<PointData>& pointvalues,
                     const Fmi::CoordinateTransformation& transformation,
                     State& theState,
                     int& valid_count);

  std::shared_ptr<QueryServer::Query> build_grid_query(const std::vector<std::string>& paramNames,
                                                       const T::Coordinate_vec& coordinates,
                                                       const State& theState);

};  // class ArrowLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
