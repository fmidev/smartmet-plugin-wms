// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure for observation data layer
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include "LayerConfig.h"
#include <string>
#include <utility>

namespace SmartMet::Engine::Observation
{
class Engine;
}  // namespace SmartMet::Engine::Observation

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
class ObservationLayer : public Layer
{
 private:
  const Engine::Observation::Engine* itsObsEngine;
  std::string itsProducer;

 protected:
  bool updateLayerMetaData() override;

 public:
  ObservationLayer(const LayerConfig& config, std::string producer)
      : Layer(config),
        itsObsEngine(config.obsEngine()),
        itsProducer(std::move(producer))
  {
  }
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
