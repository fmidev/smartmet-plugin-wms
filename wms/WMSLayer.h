// ======================================================================
/*!
 * \brief A Meb Maps Service Layer data structure
 *
 * Characteristics:
 *
 *  -
 *  -
 */
// ======================================================================

#pragma once

#include "WMSLayerStyle.h"
#include "WMSLegendGraphicInfo.h"
#include "WMSLegendGraphicSettings.h"
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
  bool hidden = false;  // if this is true, dont show in GetCapabilities response
  boost::posix_time::ptime metadataTimestamp;
  unsigned int metadataUpdateInterval;

  Spine::BoundingBox geographicBoundingBox;
  std::map<std::string, std::string> crs;             // id to GDAL definition
  std::map<std::string, Engine::Gis::BBox> crs_bbox;  // id to bounding box mapping

  std::vector<WMSLayerStyle> itsStyles;
  boost::shared_ptr<WMSTimeDimension> timeDimension;  // Optional, may be empty for non-temporal
                                                      // postgis layers
  std::string customer;
  std::string productFile;  // dali product
  //  LegendGraphicInfo legendGraphicInfo;
  NamedLegendGraphicInfo itsNamedLegendGraphicInfo;
  WMSLegendGraphicSettings legendGraphicSettings;

  friend class WMSLayerFactory;
  friend std::ostream& operator<<(std::ostream&, const WMSLayer&);

 public:
  WMSLayer(const WMSConfig& config);

  void addStyle(const Json::Value& root,
                const std::string& layerName,
                const WMSLayerStyle& layerStyle);
  void addStyle(const std::string& layerName, const WMSLayerStyle& layerStyle);
  bool isHidden() const { return hidden; }
  const std::string& getName() const { return name; }
  const std::string& getCustomer() const { return customer; }
  const std::string& getDaliProductFile() const { return productFile; }
  LegendGraphicResult getLegendGraphic(const WMSLegendGraphicSettings& settings,
                                       const std::string& legendGraphicID) const;

  bool isValidCRS(const std::string& theCRS) const;
  bool isValidStyle(const std::string& theStyle) const;
  bool isValidTime(const boost::posix_time::ptime& theTime) const;
  bool isTemporal() const { return timeDimension != nullptr; }
  bool currentValue() const;  // returns true if current value can be queried from layer
                              // (time=current)

  // returns the most current valid time for the layer
  boost::posix_time::ptime mostCurrentTime() const;

  // Empty for hidden layers
  boost::optional<CTPP::CDT> generateGetCapabilities(const Engine::Gis::Engine& gisengine);

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
};

using SharedWMSLayer = boost::shared_ptr<WMSLayer>;
using SharedWMSLayers = std::map<std::string, SharedWMSLayer>;

std::ostream& operator<<(std::ostream& ost, const WMSLayer& layer);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfoItem& lgi);
std::ostream& operator<<(std::ostream& ost, const LegendGraphicInfo& lgi);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
