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

#include "WMSConfig.h"

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSNonTemporalLayer : public WMSLayer
{
 private:
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);

 protected:
  bool updateLayerMetaData() override { return true; }

 public:
  explicit WMSNonTemporalLayer(const WMSConfig& config) : WMSLayer(config) {}

  const Fmi::DateTime& modificationTime() const override { return itsModificationTime; }
};

class WMSMapLayer : public WMSNonTemporalLayer
{
 public:
  WMSMapLayer(const WMSConfig& config, const Json::Value& json);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
