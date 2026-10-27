// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure for grid data layer
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "LayerConfig.h"
#include "Layer.h"

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
struct GridQuery;

class GridDataLayer : public Layer
{
 public:
  GridDataLayer(const LayerConfig& config,
                std::string producer,
                std::string parameter,
                int forecastType,
                int forecastNumber,
                uint geometryId,
                int levelId,
                std::string elevation_unit);

 protected:
  bool updateLayerMetaData() override;

 private:
  const Engine::Grid::Engine* itsGridEngine;
  const std::string itsProducer;
  const std::string itsParameter;
  int itsForecastType = 0;
  int itsForecastNumber = -1;
  int itsGeometryId = 0;
  int itsLevelId = 0;
  std::string itsElevationUnit;

  bool getContentTimes(ContentServer::ServiceInterface& contentServer,
                       const T::GenerationInfo& generationInfo,
                       GridQuery& query,
                       const std::string& producerName,
                       int tmpGeometryId,
                       const std::set<int>& validGeometries,
                       std::set<std::string>& contentTimeList,
                       std::set<int>& members);
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
