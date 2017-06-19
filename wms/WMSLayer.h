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
  bool queryable;
  bool quiet = true;
  std::string name;
  std::string title;
  std::string abstract;
  std::set<std::string> keywords;
  Spine::BoundingBox geographicBoundingBox;
  std::set<std::string> crs;
  std::vector<WMSLayerStyle> styles;
  boost::shared_ptr<WMSTimeDimension> timeDimension;  // Optional, may be empty for non-temporal
                                                      // postgis layers
  std::string customer;
  std::string productFile;  // dali product
  bool hidden;              // if this is true, dont show in GetCapabilities response
  LegendGraphicInfo legendGraphicInfo;

  friend class WMSLayerFactory;
  friend std::ostream& operator<<(std::ostream&, const WMSLayer&);

 public:
  WMSLayer();

  void addStyles(const Json::Value& root, const std::string& layerName);
  void addStyle(const std::string& layerName);
  bool isHidden() const { return hidden; }
  std::string getCustomer() const;
  std::string getName() const;
  std::string getDaliProductFile() const { return productFile; }
  std::vector<std::string> getLegendGraphic(const std::string& templateDirectory) const;

  std::string getTitle() const { return title; }
  std::string getAbstract() const { return abstract; }
  const std::set<std::string>& getSupportedCRS() const { return crs; }
  bool isQueryable() const { return queryable; }
  bool isValidCRS(const std::string& theCRS) const;
  bool isValidStyle(const std::string& theStyle) const;
  bool isValidTime(const boost::posix_time::ptime& theTime) const;
  bool isTemporal() const { return timeDimension != nullptr; }
  bool currentValue() const;  // returns true if current value can be queried from layer
                              // (time=current)
  boost::posix_time::ptime mostCurrentTime() const;  // returns the most current valid time for the
                                                     // layer

  std::string generateGetCapabilities(const Engine::Gis::Engine& gisengine);

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
