// ======================================================================
/*!
 * \brief A Meb Maps Service Layer data structure for query data layer
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
class QueryDataLayer : public Layer
{
 private:
  const Engine::Querydata::Engine* itsQEngine;
  const std::string itsProducer;
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);

 protected:
  bool updateLayerMetaData() override;

 public:
  QueryDataLayer(const Config& config, std::string producer)
    : Layer(config), itsQEngine(config.qEngine()), itsProducer(std::move(producer))
  {
  }

  const Fmi::DateTime& modificationTime() const override;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
