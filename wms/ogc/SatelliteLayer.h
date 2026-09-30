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
  const std::string itsParameter;
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);
  std::size_t itsImageCount = 0;

 protected:
  bool updateLayerMetaData() override;

 public:
  // The layer is created again from its product file only when the
  // engine has a newer image or a different number of them
  bool mustUpdateLayerMetaData() override;

 protected:
 public:
  SatelliteLayer(const LayerConfig& config, std::string producer, std::string parameter)
      : Layer(config),
        itsSatelliteEngine(config.satelliteEngine()),
        itsProducer(std::move(producer)),
        itsParameter(std::move(parameter))
  {
  }

  const Fmi::DateTime& modificationTime() const override;
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
