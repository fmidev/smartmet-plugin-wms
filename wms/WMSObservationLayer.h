// ======================================================================
/*!
 * \brief A Meb Maps Service Layer data structure for observation data layer
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
class WMSObservationLayer : public WMSLayer
{
 private:
  const Engine::Observation::Engine* itsObsEngine;
  std::string itsProducer;

 protected:
  bool updateLayerMetaData() override;

 public:
  WMSObservationLayer(const WMSConfig& config, std::string producer)
      : WMSLayer(config),
        itsObsEngine(config.obsEngine()),
        itsProducer(std::move(producer))
  {
  }
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
