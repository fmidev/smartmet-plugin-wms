
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

#include "WMSConfig.h"

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
struct PostGISMetaDataSettings
{
  bool found = false;
  unsigned int update_interval = 5;
  std::string pgname;
  std::string schema;
  std::string table;
  std::string field;
  std::string where;
};

class WMSPostGISLayer : public WMSLayer
{
 private:
  const Engine::Gis::Engine* itsGisEngine;
  Engine::Gis::MetaDataQueryOptions mdq_options;
  bool hasTemporalDimension;
  PostGISMetaDataSettings itsMetaDataSettings;

 protected:
  virtual void updateLayerMetaData();
  virtual bool mustUpdateLayerMetaData();

 public:
  WMSPostGISLayer(const WMSConfig& config, const Json::Value& json);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
