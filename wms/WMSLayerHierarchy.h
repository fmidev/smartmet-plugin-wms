// ======================================================================
/*!
 * \brief Layer hierarchy structure for WMS layers
 */
// ======================================================================

#pragma once

#include "WMSLayerProxy.h"
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
class WMSLayerHierarchy
{
 public:
  enum class HierarchyType
  {
    flat,
    recursive,
    recursivetimes
  };
  enum class ElementType
  {
    geo_bbox,
    proj_bbox,
    time_dim,
    elev_dim
  };

#ifndef WITHOUT_AUTHENTICATION
  WMSLayerHierarchy(const std::map<std::string, WMSLayerProxy>& layerMap,
                    const std::optional<std::string>& wms_namespace,
                    HierarchyType hierarchy_type,
                    bool reveal_hidden,
                    const std::optional<std::string>& apikey,
                    bool auth,
                    Engine::Authentication::Engine* authEngine);
#else
  WMSLayerHierarchy(const std::map<std::string, WMSLayerProxy>& layerMap,
                    const std::optional<std::string>& wms_namespace,
                    HierarchyType hierarchy_type,
                    bool reveal_hidden);
#endif

  ~WMSLayerHierarchy() = default;
  explicit WMSLayerHierarchy(std::string n);
  WMSLayerHierarchy() = delete;
  WMSLayerHierarchy(const WMSLayerHierarchy& other) = delete;
  WMSLayerHierarchy(WMSLayerHierarchy&& other) = delete;
  WMSLayerHierarchy& operator=(const WMSLayerHierarchy& other) = delete;
  WMSLayerHierarchy& operator=(WMSLayerHierarchy&& other) = delete;

  CTPP::CDT getCapabilities(bool multiple_intervals,
                            const std::string& language,
                            const std::optional<std::string>& starttime,
                            const std::optional<std::string>& endtime,
                            const std::optional<std::string>& reference_time) const;

  std::string name;
  bool show_hidden;
  bool authenticate;
  // Parts that can be inhereted by sublayers
  std::optional<WMSLayerProxy> baseInfoLayer;
  std::optional<WMSLayerProxy> geographicBoundingBox;
  std::optional<WMSLayerProxy> projectedBoundingBox;
  std::optional<WMSLayerProxy> timeDimension;
  std::optional<WMSLayerProxy> elevationDimension;

  const WMSLayerHierarchy* parent;
  std::list<boost::shared_ptr<WMSLayerHierarchy>> sublayers;
  std::optional<Fmi::DateTime> reference_time;

 private:
#ifndef WITHOUT_AUTHENTICATION
  void processLayers(const std::map<std::string, WMSLayerProxy>& layerMap,
                     const std::optional<std::string>& wms_namespace,
                     HierarchyType hierarchy_type,
                     const std::optional<std::string>& apikey,
                     Engine::Authentication::Engine* authEngine);
#else
  void processLayers(const std::map<std::string, WMSLayerProxy>& layerMap,
                     const std::optional<std::string>& wms_namespace,
                     HierarchyType hierarchy_type);
#endif
};

std::ostream& operator<<(std::ostream& ost, const WMSLayerHierarchy& lh);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
