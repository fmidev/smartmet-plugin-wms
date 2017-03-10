
// ======================================================================
/*!
 * \brief A Meb Maps Service Layer data structure for PostGIS data layer
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "WMSLayer.h"
#include <engines/gis/Engine.h>

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSPostGISLayer : public WMSLayer
{
 private:
  const Engine::Gis::Engine* itsGisEngine;
  Engine::Gis::MetaDataQueryOptions mdq_options;
  bool hasTemporalDimension;

 protected:
  virtual void updateLayerMetaData();

 public:
  WMSPostGISLayer(const Engine::Gis::Engine* gisengine, const Json::Value& json);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
