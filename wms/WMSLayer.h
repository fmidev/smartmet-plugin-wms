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
#include "WMSTimeDimension.h"

#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <ctpp2/CDT.hpp>
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
}

namespace Plugin
{
namespace WMS
{
typedef std::vector<std::map<std::string, std::string> > LegendGraphicInfo;

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

  bool hidden = false;  // if this is true, dont show in GetCapabilities response

  Spine::BoundingBox geographicBoundingBox;
  std::set<std::string> crs;
  std::vector<WMSLayerStyle> styles;
  boost::shared_ptr<WMSTimeDimension> timeDimension;  // Optional, may be empty for non-temporal
                                                      // postgis layers
  std::string customer;
  std::string productFile;  // dali product
  LegendGraphicInfo legendGraphicInfo;

  friend class WMSLayerFactory;
  friend std::ostream& operator<<(std::ostream&, const WMSLayer&);

 public:
  WMSLayer();

  void addStyles(const Json::Value& root, const std::string& layerName);
  void addStyle(const std::string& layerName);
  bool isHidden() const { return hidden; }
  const std::string& getName() const { return name; }
  const std::string& getCustomer() const { return customer; }
  const std::string& getDaliProductFile() const { return productFile; }
  std::vector<std::string> getLegendGraphic(const std::string& templateDirectory) const;

  const std::set<std::string>& getSupportedCRS() const { return crs; }
  bool isValidCRS(const std::string& theCRS) const;
  bool isValidStyle(const std::string& theStyle) const;
  bool isValidTime(const boost::posix_time::ptime& theTime) const;
  bool isTemporal() const { return timeDimension != nullptr; }
  bool currentValue() const;  // returns true if current value can be queried from layer
                              // (time=current)
  boost::posix_time::ptime mostCurrentTime() const;  // returns the most current valid time for the
                                                     // layer

  // Empty for hidden layers
  boost::optional<CTPP::CDT> generateGetCapabilities(const Engine::Gis::Engine& gisengine);

  // Update layer metadata for GetCapabilities (time,spatial dimensions)
  virtual void updateLayerMetaData() = 0;

  // Debugging info
  virtual std::string info() const;
};

typedef boost::shared_ptr<WMSLayer> SharedWMSLayer;
typedef std::map<std::string, SharedWMSLayer> SharedWMSLayers;

std::ostream& operator<<(std::ostream& ost, const WMSLayer& layer);

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
