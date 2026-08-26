// ======================================================================
/*!
 * \brief A Web Map Service layer data structure for satellite layers
 *
 * The bounding box and the time dimension are asked from the satellite
 * engine, which knows the images available for the producer.
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include "LayerConfig.h"

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
class SatelliteLayer : public Layer
{
 private:
  const Engine::Satellite::Engine* itsSatelliteEngine;
  const std::string itsProducer;
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);

 protected:
  bool updateLayerMetaData() override;

 public:
  SatelliteLayer(const LayerConfig& config, std::string producer)
      : Layer(config),
        itsSatelliteEngine(config.satelliteEngine()),
        itsProducer(std::move(producer))
  {
  }

  const Fmi::DateTime& modificationTime() const override;
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
