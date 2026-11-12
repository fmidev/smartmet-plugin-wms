// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure for query data layer
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
#include <macgyver/DateTime.h>
#include <string>
#include <utility>

namespace SmartMet::Engine::Querydata
{
class Engine;
}  // namespace SmartMet::Engine::Querydata

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
class QueryDataLayer : public Layer
{
 private:
  const Engine::Querydata::Engine* itsQEngine;
  const std::string itsProducer;
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);
  unsigned int itsUpdateInterval = 0;  // producer update_interval in seconds

 protected:
  bool updateLayerMetaData() override;

 public:
  QueryDataLayer(const LayerConfig& config, std::string producer)
    : Layer(config), itsQEngine(config.qEngine()), itsProducer(std::move(producer))
  {
  }

  const Fmi::DateTime& modificationTime() const override;
  std::optional<Fmi::DateTime> capabilitiesExpirationTime() const override;
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
