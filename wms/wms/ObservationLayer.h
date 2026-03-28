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

#include "Config.h"

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class ObservationLayer : public Layer
{
 private:
  const Engine::Observation::Engine* itsObsEngine;
  std::string itsProducer;

 protected:
  bool updateLayerMetaData() override;

 public:
  ObservationLayer(const Config& config, std::string producer)
      : Layer(config),
        itsObsEngine(config.obsEngine()),
        itsProducer(std::move(producer))
  {
  }
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
