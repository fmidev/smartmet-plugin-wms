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

#include "WMSConfig.h"

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSQueryDataLayer : public WMSLayer
{
 private:
  const Engine::Querydata::Engine* itsQEngine;
  const std::string itsProducer;
  Fmi::DateTime itsModificationTime = boost::posix_time::from_time_t(0);

 protected:
  bool updateLayerMetaData() override;

 public:
  WMSQueryDataLayer(const WMSConfig& config, std::string producer)
      : WMSLayer(config), itsQEngine(config.qEngine()), itsProducer(std::move(producer))
  {
  }

  const Fmi::DateTime& modificationTime() const override;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
