// ======================================================================
/*!
 * \brief Layer hierarchy structure for WMS layers
 */
// ======================================================================

#pragma once

#include "LayerProxy.h"
#include <memory>
#include <optional>

namespace SmartMet
{
#ifndef WITHOUT_AUTHENTICATION
namespace Engine
{
namespace Authentication
{
class Engine;
}
}  // namespace Engine
#endif

namespace Plugin
{
namespace WMS
{
class LayerHierarchy
{
 public:
  enum class HierarchyType
  {
    flat,
    recursive,
    recursivetimes
  };

#ifndef WITHOUT_AUTHENTICATION
  LayerHierarchy(const std::map<std::string, LayerProxy>& layerMap,
                    const std::optional<std::string>& wms_namespace,
                    HierarchyType hierarchy_type,
                    bool reveal_hidden,
                    const std::optional<std::string>& apikey,
                    bool auth,
                    Engine::Authentication::Engine* authEngine);
#else
  LayerHierarchy(const std::map<std::string, LayerProxy>& layerMap,
                    const std::optional<std::string>& wms_namespace,
                    HierarchyType hierarchy_type,
                    bool reveal_hidden);
#endif

  ~LayerHierarchy() = default;
  explicit LayerHierarchy(std::string n);
  LayerHierarchy() = delete;
  LayerHierarchy(const LayerHierarchy& other) = delete;
  LayerHierarchy(LayerHierarchy&& other) = delete;
  LayerHierarchy& operator=(const LayerHierarchy& other) = delete;
  LayerHierarchy& operator=(LayerHierarchy&& other) = delete;

  CTPP::CDT getCapabilities(bool multiple_intervals,
                            const std::string& language,
                            const std::string& defaultLanguage,
                            const std::optional<Fmi::DateTime>& starttime,
                            const std::optional<Fmi::DateTime>& endtime,
                            const std::optional<Fmi::DateTime>& reference_time) const;

  std::string name;
  bool show_hidden = false;
  bool authenticate = false;

  // Per-leaf data sources (set during tree construction)
  std::optional<LayerProxy> baseInfoLayer;
  std::optional<LayerProxy> geographicBoundingBox;
  std::optional<LayerProxy> projectedBoundingBox;
  std::optional<LayerProxy> timeDimension;
  std::optional<LayerProxy> elevationDimension;

  const LayerHierarchy* parent = nullptr;
  std::list<boost::shared_ptr<LayerHierarchy>> sublayers;
  std::optional<Fmi::DateTime> reference_time;

 private:
#ifndef WITHOUT_AUTHENTICATION
  void processLayers(const std::map<std::string, LayerProxy>& layerMap,
                     const std::optional<std::string>& wms_namespace,
                     HierarchyType hierarchy_type,
                     const std::optional<std::string>& apikey,
                     Engine::Authentication::Engine* authEngine);
#else
  void processLayers(const std::map<std::string, LayerProxy>& layerMap,
                     const std::optional<std::string>& wms_namespace,
                     HierarchyType hierarchy_type);
#endif
};

std::ostream& operator<<(std::ostream& ost, const LayerHierarchy& lh);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
