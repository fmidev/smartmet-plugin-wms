
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

#include "Config.h"

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

class PostGISLayer : public Layer
{
 private:
  const Engine::Gis::Engine* itsGisEngine;
  Engine::Gis::MetaDataQueryOptions mdq_options;
  bool hasTemporalDimension;
  PostGISMetaDataSettings itsMetaDataSettings;

 protected:
  bool updateLayerMetaData() override;
  bool mustUpdateLayerMetaData() override;

 public:
  PostGISLayer() = delete;
  PostGISLayer(const Config& config, Json::Value& json);
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
