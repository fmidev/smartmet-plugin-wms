// ======================================================================
/*!
 * \brief A Meb Maps Service Layer data structure for grid data layer
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "WMSConfig.h"

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSGridDataLayer : public WMSLayer
{
 public:
  WMSGridDataLayer(const WMSConfig& config,
                   std::string producer,
                   std::string parameter,
                   int forecastType,
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
  uint itsGeometryId = 0;
  int itsLevelId = 0;
  std::string itsElevationUnit;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
