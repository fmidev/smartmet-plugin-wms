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
                   const std::string& producer,
                   const std::string& parameter,
                   uint geometryId);

 protected:
  virtual void updateLayerMetaData();

 private:
  const Engine::Grid::Engine* itsGridEngine;
  const std::string itsProducer;
  const std::string itsParameter;
  uint itsGeometryId;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet