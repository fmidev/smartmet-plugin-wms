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

#include "WMSLayer.h"

#include <engines/observation/Engine.h>

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
  virtual void updateLayerMetaData();

 public:
  WMSObservationLayer(const Engine::Observation::Engine* obs,
                      const std::string& producer,
                      int timestep)
      : itsObsEngine(obs), itsProducer(producer), itsTimestep(timestep)
  {
  }
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
