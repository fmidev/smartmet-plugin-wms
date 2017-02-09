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

#include "WMSLayer.h"

#include <engines/querydata/Engine.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSQueryDataLayer : public WMSLayer
{
 private:
  const SmartMet::Engine::Querydata::Engine* itsQEngine;
  const std::string itsProducer;

 protected:
  virtual void updateLayerMetaData();

 public:
  WMSQueryDataLayer(const SmartMet::Engine::Querydata::Engine* qe, const std::string& producer)
      : itsQEngine(qe), itsProducer(producer)
  {
  }
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
