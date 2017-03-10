// ======================================================================
/*!
 * \brief State variables for the current request
 *
 * For example we cache requested producers so that the data will
 * remain the same for each layer in the product. The cache cannot
 * be in the plugin since then it would be shared by all requests.
 * Instead, we pass the state object along to all components that
 * generate actual content.
 *
 * Since the State object is always passed along, we also store
 * in it all engines that may be needed by the layers. This way
 * we do not have to decide on whether to call the plugin or
 * the state object to get access to "global stuff".
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include <engines/querydata/Q.h>
#include <engines/geonames/Engine.h>
#include <boost/optional.hpp>
#include <map>
#include <set>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace Engine
{
namespace Querydata
{
class Engine;
}
namespace Contour
{
class Engine;
}
namespace Gis
{
class Engine;
}
#ifndef WITHOUT_OBSERVATION
namespace Observation
{
class Engine;
}
#endif
}

namespace Plugin
{
namespace Dali
{
class Config;
class Filter;
class Plugin;

class State
{
 public:
  State() = delete;

  // Set the state of the plugin
  State(const Plugin& thePlugin);

  // Get the engines
  const Engine::Querydata::Engine& getQEngine() const;
  const Engine::Contour::Engine& getContourEngine() const;
  const Engine::Gis::Engine& getGisEngine() const;
  const Engine::Geonames::Engine& getGeoEngine() const;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine& getObsEngine() const;
#endif

  // Give access to configuration variables
  const Config& getConfig() const;

  // Customer
  const std::string& getCustomer() const { return itsCustomer; }
  void setCustomer(const std::string& theCustomer) { itsCustomer = theCustomer; }
  // Format
  const std::string& getType() const { return itsType; }
  void setType(const std::string& theType) { itsType = theType; }
  // Get Q to be used in the current product
  Engine::Querydata::Q get(const Engine::Querydata::Producer& theProducer) const;

  // Require given ID to be free, and mark it used if it is free
  void requireId(const std::string& theID);

  // If given ID has not been used, mark it used now
  bool addId(const std::string& theID) const;

  // Fetch CSS contents
  std::string getStyle(const std::string& theCSS) const;

  // Fetch symbol contents
  std::string getSymbol(const std::string& theName) const;
  std::size_t getSymbolHash(const std::string& theName) const;

  // Fetch pattern contents
  std::string getPattern(const std::string& theName) const;
  std::size_t getPatternHash(const std::string& theName) const;

  // Fetch marker contents
  std::string getMarker(const std::string& theName) const;
  std::size_t getMarkerHash(const std::string& theName) const;

  // Fetch gradient contents
  std::string getGradient(const std::string& theName) const;
  std::size_t getGradientHash(const std::string& theName) const;

  // Fetch filter contents
  std::string getFilter(const std::string& theName) const;
  std::size_t getFilterHash(const std::string& theName) const;

  // Add attributes
  void addAttributes(CTPP::CDT& theGlobals, const Attributes& theAttributes);

  void addAttributes(CTPP::CDT& theGlobals, CTPP::CDT& theLocals, const Attributes& theAttributes);

  // Generation stage

  void inDefs(bool flag) { itsInDefs = flag; }
  bool inDefs() const { return itsInDefs; }
  // Timer for performance measurements
  void useTimer(bool flag) { itUsesTimer = flag; }
  bool useTimer() const { return itUsesTimer; }
  // Establish WMS / regular mode
  void useWms(bool flag) { itUsesWms = flag; }
  bool useWms() const { return itUsesWms; }
  // Generate an unique ID when a readable cannot be generated based on some
  // existing ID.

  std::string generateUniqueId() const;

 private:
  const Plugin& itsPlugin;
  mutable std::map<Engine::Querydata::Producer, Engine::Querydata::Q> itsQCache;

  // Names which have already been used for styling
  mutable std::map<std::string, std::string> itsUsedStyles;

  // Names which have already been used as unique IDs in the SVG
  mutable std::set<std::string> itsUsedIds;

  // Next ID to be used
  mutable std::size_t itsNextId = 0;

  // Are we in the Defs section?
  bool itsInDefs;

  // Should we time execution?
  bool itUsesTimer;

  // Are we in WMS mode?
  bool itUsesWms;

  // The customer
  std::string itsCustomer;

  // The image format
  std::string itsType;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
