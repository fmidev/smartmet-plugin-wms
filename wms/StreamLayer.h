#pragma once

#include "Layer.h"
#include "ParameterInfo.h"
#include <engines/grid/Engine.h>
#include <engines/querydata/Q.h>
#include <gis/Types.h>
#include <grid-files/grid/Typedefs.h>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet::QueryServer
{
class Query;
}  // namespace SmartMet::QueryServer
namespace SmartMet::T
{
class AttributeList;
}  // namespace SmartMet::T

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class State;

class StreamLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  // std::optional<std::string> parameter;
  std::optional<std::string> u_parameter;
  std::optional<std::string> v_parameter;
  int minStreamLen = 5;
  int maxStreamLen = 2048;
  int lineLen = 32;
  int xStep = 20;
  int yStep = 20;
  double precision = 1.0;

 protected:
  std::vector<OGRGeometryPtr> getStreams(const State& theState);
  Engine::Querydata::Q q;  // Make used data available to derived IsolabelLayer
  T::FileId fileId = 0;
  T::MessageIndex messageIndex = 0;

 private:
  void addStreamParameters(const Engine::Grid::Engine& gridEngine,
                           const std::string& producerName,
                           T::AttributeList& attributeList,
                           QueryServer::Query& query);
  std::vector<OGRGeometryPtr> getStreamsGrid(const State& theState);
  std::vector<OGRGeometryPtr> getStreamsQuerydata(const State& theState);

};  // class StreamLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
