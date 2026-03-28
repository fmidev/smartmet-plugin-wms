// ======================================================================
/*!
 * \brief A Web Maps Service Layer data structure
 */
// ======================================================================

#pragma once

#include "../Text.h"
#include "ElevationDimension.h"
#include "IntervalDimension.h"
#include "LayerConfig.h"
#include "LayerStyle.h"
#include "LegendGraphicInfo.h"
#include "LegendGraphicSettings.h"
#include "SupportedReference.h"
#include "TimeDimension.h"
#include <ctpp2/CDT.hpp>
#include <gis/BBox.h>
#include <macgyver/DateTime.h>
#include <spine/Json.h>
#include <spine/Value.h>
#include <list>
#include <map>
#include <memory>
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
namespace OGC
{
class Layer
{
 private:
  void setProductFile(const std::string& theProductFile);

 protected:
  bool quiet = true;

  // GetCapabilities settings
  std::string name;
  std::optional<Dali::Text> title;
  std::optional<Dali::Text> abstract;
  std::optional<std::set<std::string>> keywords;
  std::optional<int> opaque;  // Note: optional<bool> is error prone
  std::optional<int> queryable;
  std::optional<int> cascaded;
  std::optional<int> no_subsets;
  std::optional<int> fixed_width;
  std::optional<int> fixed_height;
  std::optional<int> width;  // If layer is used as a legend file width and height are needed here
  std::optional<int> height;

  std::optional<int> timestep;  // minutes

  const LayerConfig config;
  bool hidden = false;                 // If this is true, dont show in GetCapabilities response
  bool timeDimensionDisabled = false;  // Can any timestamp can be used in GetMap-request

  // These can be used to limit capabilities from current time, since
  // we may have observations available since early 1900s, but we do
  // not wish to show some WMS layers for them if the layer is
  // intended for latest observations only. Large GetCapabilities
  // responses mess up most GUIs.
  std::optional<Fmi::TimeDuration> capabilities_start;
  std::optional<Fmi::TimeDuration> capabilities_end;

  Fmi::DateTime metadataTimestamp = Fmi::DateTime::NOT_A_DATE_TIME;
  unsigned int metadataUpdateInterval = 5;

  Spine::BoundingBox geographicBoundingBox;
  std::map<std::string, SupportedReference> refs;
  std::map<std::string, Fmi::BBox> projected_bbox;  // final bbox for GetCapabilities output

  std::set<std::string> enabled_refs;
  std::set<std::string> disabled_refs;

  std::map<std::string, LayerStyle> itsStyles;
  std::map<std::string, Json::Value> itsSubstitutions;  // variant overrides

  std::shared_ptr<TimeDimensions>
      timeDimensions;  // Optional, may be empty for non-temporal postgis layers
  std::shared_ptr<ElevationDimension> elevationDimension;  // Optional
  std::shared_ptr<IntervalDimension> intervalDimension;    // Optional

  std::string customer;
  std::string productFile;  // dali product
  std::map<std::string, LegendGraphicResultPerLanguage> itsLegendGraphicResults;
  std::map<std::string, std::string> itsLegendFiles;
  Fmi::DateTime itsProductFileModificationTime;

  friend class LayerFactory;
  friend std::ostream& operator<<(std::ostream& os, const Layer& layer);

 public:
  virtual ~Layer() = default;
  Layer(const LayerConfig& lc);
  Layer(const Layer& other) = delete;
  Layer(Layer&& other) = delete;
  Layer& operator=(const Layer& other) = delete;
  Layer& operator=(Layer&& other) = delete;

  bool identicalRefs(const Layer& layer) const;
  bool identicalGeographicBoundingBox(const Layer& layer) const;
  bool identicalProjectedBoundingBox(const Layer& layer) const;
  bool identicalTimeDimension(const Layer& layer) const;
  bool identicalElevationDimension(const Layer& layer) const;
  bool identicalKeywords(const Layer& layer) const;

  // Returns projected bounding box / CRS entries that differ from
  // `inherited`.  If all entries are identical returns an empty optional.
  // Used by LayerHierarchy to emit only the delta vs an ancestor.
  std::optional<CTPP::CDT> getProjectedBoundingBoxDelta(const Layer& inherited) const;

  bool isHidden() const { return hidden; }
  void setCustomer(const std::string& c);
  const std::string& getName() const { return name; }
  const std::string& getCustomer() const { return customer; }
  const std::string& getProductFile() const { return productFile; }
  LegendGraphicResult getLegendGraphic(const std::string& legendGraphicID,
                                       const std::string& language) const;
  const std::map<std::string, std::string>& getLegendFiles() const { return itsLegendFiles; }
  std::pair<std::string, std::string> getDefaultInterval() const;

  void setDefaultElevation(int level);

  bool isValidInterval(int interval_start, int interval_end) const;
  bool isValidCRS(const std::string& theCRS) const;
  bool isValidStyle(const std::string& theStyle) const;
  bool isValidTime(const Fmi::DateTime& theTime,
                   const std::optional<Fmi::DateTime>& theReferenceTime) const;
  bool isValidReferenceTime(const Fmi::DateTime& theReferenceTime) const;
  bool isValidElevation(int theElevation) const;
  bool isTemporal() const { return timeDimensions != nullptr; }
  bool currentValue() const;  // returns true if current value can be queried from layer
                              // (time=current)
  // Legend width, height is read from separate file
  void setLegendDimension(const Layer& legendLayer, const std::string& styleName);
  const std::optional<int>& getWidth() const { return width; }
  const std::optional<int>& getHeight() const { return height; }

  // returns the most current valid time for the layer
  Fmi::DateTime mostCurrentTime(const std::optional<Fmi::DateTime>& reference_time) const;
  // Empty for hidden layers

  std::optional<CTPP::CDT> generateGetCapabilities(
      bool multiple_intervals,
      bool show_hidden,
      const Engine::Gis::Engine& gisengine,
      const std::string& language,
      const std::string& defaultLanguage,
      const std::optional<Fmi::DateTime>& starttime,
      const std::optional<Fmi::DateTime>& endtime,
      const std::optional<Fmi::DateTime>& reference_time);

  std::map<std::string, Json::Value> getSubstitutions() const { return itsSubstitutions; }

  std::optional<CTPP::CDT> getLayerBaseInfo(const std::string& language,
                                            const std::string& defaultLanguage) const;
  std::optional<CTPP::CDT> getGeographicBoundingBoxInfo() const;
  std::optional<CTPP::CDT> getProjectedBoundingBoxInfo() const;
  std::optional<CTPP::CDT> getTimeDimensionInfo(
      bool multiple_intervals,
      const std::optional<Fmi::DateTime>& starttime,
      const std::optional<Fmi::DateTime>& endtime,
      const std::optional<Fmi::DateTime>& reference_time) const;
  std::optional<CTPP::CDT> getIntervalDimensionInfo() const;
  std::optional<CTPP::CDT> getReferenceDimensionInfo() const;
  std::optional<CTPP::CDT> getElevationDimensionInfo() const;
  std::optional<CTPP::CDT> getStyleInfo(const std::string& language,
                                        const std::string& defaultLanguage) const;
  const std::shared_ptr<TimeDimensions>& getTimeDimensions() const;

  std::pair<std::optional<Fmi::DateTime>, std::optional<Fmi::DateTime>>
  getLimitedCapabilitiesInterval(const std::optional<Fmi::DateTime>& starttime,
                                 const std::optional<Fmi::DateTime>& endtime) const;

  // To be called after crs and crs_bbox have been initialized
  void initProjectedBBoxes();

  //
  void initLegendGraphicInfo(const Json::Value& root);

  // Update layer metadata for GetCapabilities (time,spatial dimensions)
  virtual bool updateLayerMetaData() = 0;

  // Debugging info
  virtual std::string info() const;

  // returns timestamp when metadata was previous time updated
  Fmi::DateTime metaDataUpdateTime() const { return metadataTimestamp; }
  // inherited layers can override this to determine weather metadata must be updated
  virtual bool mustUpdateLayerMetaData() { return true; }
  // by default interval is 5 seconds, but for some layers it could be longer
  unsigned int metaDataUpdateInterval() const { return metadataUpdateInterval; }
  virtual const Fmi::DateTime& modificationTime() const;
  // Add interval dimension
  void addIntervalDimension(int interval_start, int interval_end, bool interval_default);

  // read json file
  static Json::Value readJsonFile(const std::string& theFileName);
  static Json::Value parseJsonString(const std::string& theJsonString);
};

using SharedLayer = std::shared_ptr<Layer>;
using SharedLayers = std::map<std::string, SharedLayer>;

std::ostream& operator<<(std::ostream& ost, const Layer& layer);

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
