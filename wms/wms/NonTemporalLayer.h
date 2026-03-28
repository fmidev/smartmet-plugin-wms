// ======================================================================
/*!
 * \brief A Meb Maps Service Layer data structure for noneteporal layer
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "Config.h"

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class NonTemporalLayer : public Layer
{
 private:
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);

 protected:
  bool updateLayerMetaData() override { return true; }

 public:
  explicit NonTemporalLayer(const Config& config) : Layer(config) {}

  const Fmi::DateTime& modificationTime() const override { return itsModificationTime; }
};

class MapLayer : public NonTemporalLayer
{
 public:
  MapLayer(const Config& config, const Json::Value& json);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
