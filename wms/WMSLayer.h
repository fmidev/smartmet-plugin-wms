// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure
 */
// ======================================================================

#pragma once

#include "Text.h"
#include "WMSElevationDimension.h"
#include "WMSIntervalDimension.h"
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
 private:
  void setProductFile(const std::string& theProductFile);

 protected:
  bool quiet = true;

  // GetCapabilities settings
  std::string name;
  boost::optional<Dali::Text> title;
  boost::optional<Dali::Text> abstract;
  boost::optional<std::set<std::string> > keywords;
  boost::optional<int> opaque;  // Note: optional<bool> is error prone
  boost::optional<int> queryable;
  boost::optional<int> cascaded;
  boost::optional<int> no_subsets;
  boost::optional<int> fixed_width;
  boost::optional<int> fixed_height;
  boost::optional<int> width;  // If layer is used as a legend file width and height are needed here
  boost::optional<int> height;

  const WMSConfig& wmsConfig;
  bool hidden = false;                 // If this is true, dont show in GetCapabilities response
  bool timeDimensionDisabled = false;  // Can any timestamp can be used in GetMap-request
  boost::posix_time::ptime metadataTimestamp = boost::posix_time::not_a_date_time;
  unsigned int metadataUpdateInterval = 5;

  Spine::BoundingBox geographicBoundingBox;
  std::map<std::string, WMSSupportedReference> refs;
  std::map<std::string, Engine::Gis::BBox> projected_bbox;  // final bbox for GetCapabilities output

  std::set<std::string> enabled_refs;
  std::set<std::string> disabled_refs;

  std::map<std::string, WMSLayerStyle> itsStyles;
  boost::shared_ptr<WMSTimeDimensions> timeDimensions{
      nullptr};  // Optional, may be empty for non-temporal postgis layers
  boost::shared_ptr<WMSElevationDimension> elevationDimension{nullptr};  // Optional
  boost::shared_ptr<WMSIntervalDimension> intervalDimension{nullptr};    // Optional

  std::string customer;
  std::string productFile;  // dali product
  std::map<std::string, LegendGraphicResultPerLanguage> itsLegendGraphicResults;
  std::map<std::string, std::string> itsLegendFiles;
  boost::posix_time::ptime itsProductFileModificationTime;

  friend class WMSLayerFactory;
  friend std::ostream& operator<<(std::ostream& os, const WMSLayer& layer);

 public:
  virtual ~WMSLayer() = default;
  WMSLayer(const WMSConfig& config);
  WMSLayer(const WMSLayer& other) = delete;
  WMSLayer(WMSLayer&& other) = delete;
  WMSLayer& operator=(const WMSLayer& other) = delete;
  WMSLayer& operator=(WMSLayer&& other) = delete;

  bool identicalRefs(const WMSLayer& layer) const;
  bool identicalGeographicBoundingBox(const WMSLayer& layer) const;
  bool identicalProjectedBoundingBox(const WMSLayer& layer) const;
  bool identicalTimeDimension(const WMSLayer& layer) const;
  bool identicalElevationDimension(const WMSLayer& layer) const;

  bool isHidden() const { return hidden; }
  void setCustomer(const std::string& c);
  const std::string& getName() const { return name; }
  const std::string& getCustomer() const { return customer; }
  const std::string& getProductFile() const { return productFile; }
  LegendGraphicResult getLegendGraphic(const std::string& legendGraphicID,
                                       const std::string& language) const;
  const std::map<std::string, std::string>& getLegendFiles() const { return itsLegendFiles; }
  std::pair<std::string, std::string> getDefaultInterval() const;

  bool isValidInterval(int interval_start, int interval_end) const;
  bool isValidCRS(const std::string& theCRS) const;
  bool isValidStyle(const std::string& theStyle) const;
  bool isValidTime(const boost::posix_time::ptime& theTime,
                   const boost::optional<boost::posix_time::ptime>& theReferenceTime) const;
  bool isValidReferenceTime(const boost::posix_time::ptime& theReferenceTime) const;
  bool isValidElevation(int theElevation) const;
  bool isTemporal() const { return timeDimensions != nullptr; }
  bool currentValue() const;  // returns true if current value can be queried from layer
                              // (time=current)
  // Legend width, height is read from separate file
  void setLegendDimension(const WMSLayer& legendLayer, const std::string& styleName);
  const boost::optional<int>& getWidth() const { return width; }
  const boost::optional<int>& getHeight() const { return height; }

  // returns the most current valid time for the layer
  boost::posix_time::ptime mostCurrentTime(
      const boost::optional<boost::posix_time::ptime>& reference_time) const;
  // Empty for hidden layers

  boost::optional<CTPP::CDT> generateGetCapabilities(
      bool multiple_intervals,
      const Engine::Gis::Engine& gisengine,
      const std::string& language,
      const boost::optional<std::string>& starttime,
      const boost::optional<std::string>& endtime,
      const boost::optional<std::string>& reference_time);

  boost::optional<CTPP::CDT> getLayerBaseInfo(const std::string& language) const;
  boost::optional<CTPP::CDT> getGeographicBoundingBoxInfo() const;
  boost::optional<CTPP::CDT> getProjectedBoundingBoxInfo() const;
  boost::optional<CTPP::CDT> getTimeDimensionInfo(
      bool multiple_intervals,
      const boost::optional<std::string>& starttime,
      const boost::optional<std::string>& endtime,
      const boost::optional<std::string>& reference_time) const;
  boost::optional<CTPP::CDT> getIntervalDimensionInfo() const;
  boost::optional<CTPP::CDT> getReferenceDimensionInfo() const;
  boost::optional<CTPP::CDT> getElevationDimensionInfo() const;
  boost::optional<CTPP::CDT> getStyleInfo(const std::string& language) const;
  const boost::shared_ptr<WMSTimeDimensions>& getTimeDimensions() const;

  // To be called after crs and crs_bbox have been initialized
  void initProjectedBBoxes();

  //
  void initLegendGraphicInfo(const Json::Value& root);

  // Update layer metadata for GetCapabilities (time,spatial dimensions)
  virtual bool updateLayerMetaData() = 0;

  // Debugging info
  virtual std::string info() const;

  // returns timestamp when metadata was previous time updated
  boost::posix_time::ptime metaDataUpdateTime() const { return metadataTimestamp; };
  // inherited layers can override this to determine weather metadata must be updated
  virtual bool mustUpdateLayerMetaData() { return true; }
  // by default interval is 5 seconds, but for some layers it could be longer
  unsigned int metaDataUpdateInterval() const { return metadataUpdateInterval; }
  virtual const boost::posix_time::ptime& modificationTime() const;
  // Add interval dimension
  void addIntervalDimension(int interval_start, int interval_end, bool interval_default);

  // read json file
  static Json::Value readJsonFile(const std::string& theFileName);
  static Json::Value parseJsonString(const std::string& theJsonString);
};

using SharedWMSLayer = boost::shared_ptr<WMSLayer>;
using SharedWMSLayers = std::map<std::string, SharedWMSLayer>;

std::ostream& operator<<(std::ostream& ost, const WMSLayer& layer);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfoItem& lgi);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfo& lgi);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
