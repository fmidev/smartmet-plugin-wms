// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure for non-temporal layer
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

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
class NonTemporalLayer : public Layer
{
 private:
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);

 protected:
  bool updateLayerMetaData() override { return true; }

 public:
  explicit NonTemporalLayer(const LayerConfig& config) : Layer(config) {}

  const Fmi::DateTime& modificationTime() const override { return itsModificationTime; }
};

class MapLayer : public NonTemporalLayer
{
 public:
  MapLayer(const LayerConfig& config, const Json::Value& json);
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
