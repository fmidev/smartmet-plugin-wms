// ======================================================================
/*!
 * \brief A Web Map Service layer of gridded observations
 *
 * The data is gridded querydata produced on request by the gridding of
 * the plugin, hence the capabilities come from the gridding configuration
 * instead of the querydata engine. Drawing uses the querydata path of
 * the layers as such.
 */
// ======================================================================

#pragma once

#include "Layer.h"
#include "LayerConfig.h"

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
class ObsGridLayer : public Layer
{
 public:
  ObsGridLayer(const LayerConfig& config, std::string producer)
      : Layer(config), itsGridder(config.gridder()), itsProducer(std::move(producer))
  {
  }

  const Fmi::DateTime& modificationTime() const override;

 protected:
  bool updateLayerMetaData() override;

 private:
  const Dali::Gridding::Gridder* itsGridder;
  const std::string itsProducer;
  Fmi::DateTime itsModificationTime = Fmi::date_time::from_time_t(0);
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
