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
  boost::posix_time::ptime itsModificationTime = boost::posix_time::from_time_t(0);

 protected:
  virtual void updateLayerMetaData();

 public:
  WMSQueryDataLayer(const WMSConfig& config, const std::string& producer)
      : WMSLayer(config), itsQEngine(config.qEngine()), itsProducer(producer)
  {
  }

  const boost::posix_time::ptime& modificationTime() const override;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
