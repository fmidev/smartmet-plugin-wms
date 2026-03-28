
// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure for PostGIS data layer
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "LayerConfig.h"
#include "Layer.h"

#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
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
  PostGISLayer(const LayerConfig& config, Json::Value& json);
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
