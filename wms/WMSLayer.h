// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure
 */
// ======================================================================

#pragma once

#include "WMSElevationDimension.h"
#include "WMSLayerStyle.h"
#include "WMSLegendGraphicInfo.h"
#include "WMSLegendGraphicSettings.h"
#include "WMSSupportedReference.h"
#include "WMSTimeDimension.h"
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <ctpp2/CDT.hpp>
#include <engines/gis/BBox.h>
#include <spine/Json.h>
#include <spine/Value.h>
#include <list>
#include <map>
#include <set>

namespace SmartMet
{
namespace Engine
{
namespace Gis
{
class Engine;
}
}  // namespace Engine

namespace Plugin
{
namespace WMS
{
class WMSConfig;

class WMSLayer
{
 protected:
  bool quiet = true;

  // GetCapabilities settings
  std::string name;
  boost::optional<std::string> title;
  boost::optional<std::string> abstract;
  boost::optional<std::set<std::string> > keywords;
  boost::optional<int> opaque;  // Note: optional<bool> is error prone
  boost::optional<int> queryable;
  boost::optional<int> cascaded;
  boost::optional<int> no_subsets;
  boost::optional<int> fixed_width;
  boost::optional<int> fixed_height;

  const WMSConfig& wmsConfig;
  bool hidden{false};  // If this is true, dont show in GetCapabilities response
  bool timeDimensionDisabled{
      false};  // If this is true, any timestamp can be used in GetMap-request
  boost::posix_time::ptime metadataTimestamp;
  unsigned int metadataUpdateInterval;

  Spine::BoundingBox geographicBoundingBox;
  std::map<std::string, WMSSupportedReference> refs;
  std::map<std::string, Engine::Gis::BBox> projected_bbox;  // final bbox for GetCapabilities output

  std::set<std::string> enabled_refs;
  std::set<std::string> disabled_refs;

  std::vector<WMSLayerStyle> itsStyles;
  boost::shared_ptr<WMSTimeDimensions> timeDimensions{
      nullptr};  // Optional, may be empty for non-temporal postgis layers
  boost::shared_ptr<WMSElevationDimension> elevationDimension{nullptr};  // Optional

  std::string customer;
  std::string productFile;  // dali product
  NamedLegendGraphicInfo itsNamedLegendGraphicInfo;
  WMSLegendGraphicSettings itsLegendGraphicSettings;

  friend class WMSLayerFactory;
  friend std::ostream& operator<<(std::ostream&, const WMSLayer&);

 public:
  virtual ~WMSLayer() = default;
  WMSLayer(const WMSConfig& config);

  bool identicalRefs(const WMSLayer& layer) const;
  bool identicalGeographicBoundingBox(const WMSLayer& layer) const;
  bool identicalProjectedBoundingBox(const WMSLayer& layer) const;
  bool identicalTimeDimension(const WMSLayer& layer) const;
  bool identicalElevationDimension(const WMSLayer& layer) const;

  void addStyle(const Json::Value& root,
                const std::string& layerName,
                const WMSLayerStyle& layerStyle);
  void addStyle(const std::string& layerName, const WMSLayerStyle& layerStyle);
  bool isHidden() const { return hidden; }
  void setCustomer(const std::string& c);
  const std::string& getName() const { return name; }
  const std::string& getCustomer() const { return customer; }
  const std::string& getDaliProductFile() const { return productFile; }
  LegendGraphicResult getLegendGraphic(const WMSLegendGraphicSettings& settings,
                                       const std::string& legendGraphicID,
                                       const std::string& language) const;

  bool isValidCRS(const std::string& theCRS) const;
  bool isValidStyle(const std::string& theStyle) const;
  bool isValidTime(const boost::posix_time::ptime& theTime,
                   const boost::optional<boost::posix_time::ptime>& theReferenceTime) const;
  bool isValidReferenceTime(const boost::posix_time::ptime& theReferenceTime) const;
  bool isValidElevation(int theElevation) const;
  bool isTemporal() const { return timeDimensions != nullptr; }
  bool currentValue() const;  // returns true if current value can be queried from layer
                              // (time=current)

  // returns the most current valid time for the layer
  boost::posix_time::ptime mostCurrentTime(
      const boost::optional<boost::posix_time::ptime>& reference_time) const;

  // Empty for hidden layers

  boost::optional<CTPP::CDT> generateGetCapabilities(
													 bool multiple_intervals,
													 const Engine::Gis::Engine& gisengine,
													 const boost::optional<std::string>& starttime,
													 const boost::optional<std::string>& endtime,
													 const boost::optional<std::string>& reference_time);

  boost::optional<CTPP::CDT> getLayerBaseInfo() const;
  boost::optional<CTPP::CDT> getGeographicBoundingBoxInfo() const;
  boost::optional<CTPP::CDT> getProjectedBoundingBoxInfo() const;
  boost::optional<CTPP::CDT> getTimeDimensionInfo(
												  bool  multiple_intervals,
												  const boost::optional<std::string>& starttime,
												  const boost::optional<std::string>& endtime,
												  const boost::optional<std::string>& reference_time) const;
  boost::optional<CTPP::CDT> getReferenceDimensionInfo() const;
  boost::optional<CTPP::CDT> getElevationDimensionInfo() const;
  boost::optional<CTPP::CDT> getStyleInfo() const;
  const boost::shared_ptr<WMSTimeDimensions>& getTimeDimensions() const;

  // To be called after crs and crs_bbox have been initialized
  void initProjectedBBoxes();

  // Update layer metadata for GetCapabilities (time,spatial dimensions)
  virtual void updateLayerMetaData() = 0;

  // Debugging info
  virtual std::string info() const;

  // returns timestamp when metadata was previous time updated
  boost::posix_time::ptime metaDataUpdateTime() const { return metadataTimestamp; };
  // inherited layers can override this to determine weather metadata must be updated
  virtual bool mustUpdateLayerMetaData() { return true; }
  // by default interval is 5 seconds, but for some layers it could be longer
  unsigned int metaDataUpdateInterval() const { return metadataUpdateInterval; }
  // read json file
  static Json::Value readJsonFile(const std::string theFileName);
  static Json::Value parseJsonString(const std::string theJsonString);
};

using SharedWMSLayer = boost::shared_ptr<WMSLayer>;
using SharedWMSLayers = std::map<std::string, SharedWMSLayer>;

std::ostream& operator<<(std::ostream& ost, const WMSLayer& layer);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfoItem& lgi);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfo& lgi);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
