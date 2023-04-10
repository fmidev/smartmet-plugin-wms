// ======================================================================
/*!
 * \brief Layer hierarchy structure for WMS layers
 */
// ======================================================================

#pragma once

#include "WMSLayerProxy.h"
#include <boost/shared_ptr.hpp>

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
                    const boost::optional<std::string>& wms_namespace,
                    HierarchyType hierarchy_type,
                    const boost::optional<std::string>& apikey,
                    Engine::Authentication::Engine* authEngine);
#else
  WMSLayerHierarchy(const std::map<std::string, WMSLayerProxy>& layerMap,
                    const boost::optional<std::string>& wms_namespace,
                    HierarchyType hierarchy_type);
#endif

  ~WMSLayerHierarchy() = default;
  explicit WMSLayerHierarchy(std::string n);
  WMSLayerHierarchy() = delete;
  WMSLayerHierarchy(const WMSLayerHierarchy& other) = delete;
  WMSLayerHierarchy(WMSLayerHierarchy&& other) = delete;
  WMSLayerHierarchy& operator=(const WMSLayerHierarchy& other) = delete;
  WMSLayerHierarchy& operator=(WMSLayerHierarchy&& other) = delete;

  CTPP::CDT getCapabilities(bool multiple_intervals,
                            const boost::optional<std::string>& starttime,
                            const boost::optional<std::string>& endtime,
                            const boost::optional<std::string>& reference_time) const;

  std::string name;
  // Parts that can be inhereted by sublayers
  boost::optional<const WMSLayerProxy&> baseInfoLayer;
  boost::optional<const WMSLayerProxy&> geographicBoundingBox;
  boost::optional<const WMSLayerProxy&> projectedBoundingBox;
  boost::optional<const WMSLayerProxy&> timeDimension;
  boost::optional<const WMSLayerProxy&> elevationDimension;

  boost::optional<const WMSLayerHierarchy&> parent;
  std::list<boost::shared_ptr<WMSLayerHierarchy>> sublayers;
  boost::optional<const boost::posix_time::ptime&> reference_time;

 private:
#ifndef WITHOUT_AUTHENTICATION
  void processLayers(const std::map<std::string, WMSLayerProxy>& layerMap,
                     const boost::optional<std::string>& wms_namespace,
                     HierarchyType hierarchy_type,
                     const boost::optional<std::string>& apikey,
                     Engine::Authentication::Engine* authEngine);
#else
  void processLayers(const std::map<std::string, WMSLayerProxy>& layerMap,
                     const boost::optional<std::string>& wms_namespace,
                     HierarchyType hierarchy_type);
#endif
};

std::ostream& operator<<(std::ostream& ost, const WMSLayerHierarchy& lh);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
