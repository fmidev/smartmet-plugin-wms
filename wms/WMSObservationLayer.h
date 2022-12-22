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
  int itsTimestep;

 protected:
  bool updateLayerMetaData() override;

 public:
  WMSObservationLayer(const WMSConfig& config, const std::string& producer, int timestep)
      : WMSLayer(config),
        itsObsEngine(config.obsEngine()),
        itsProducer(producer),
        itsTimestep(timestep)
  {
  }
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
