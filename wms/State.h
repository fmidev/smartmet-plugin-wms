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
#include <boost/optional.hpp>
#include <engines/geonames/Engine.h>
#include <engines/querydata/Q.h>
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
}  // namespace Engine

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
  Engine::Querydata::Q get(const Engine::Querydata::Producer& theProducer,
                           const boost::posix_time::ptime& theOriginTime) const;

  // Require given ID to be free, and mark it used if it is free
  void requireId(const std::string& theID);

  // If given ID has not been used, mark it used now
  bool addId(const std::string& theID) const;

  // Create unique ID for the given prefix
  std::string makeQid(const std::string& thePrefix) const;

  // Fetch CSS contents
  std::string getStyle(const std::string& theCSS) const;

  // Fetch symbol contents
  bool setSymbol(const std::string& theName, const std::string& theValue) const;
  std::string getSymbol(const std::string& theName) const;
  std::size_t getSymbolHash(const std::string& theName) const;

  // Fetch pattern contents
  bool setPattern(const std::string& theName, const std::string& theValue) const;
  std::string getPattern(const std::string& theName) const;
  std::size_t getPatternHash(const std::string& theName) const;

  // Fetch marker contents
  bool setMarker(const std::string& theName, const std::string& theValue) const;
  std::string getMarker(const std::string& theName) const;
  std::size_t getMarkerHash(const std::string& theName) const;

  // Fetch gradient contents
  bool setGradient(const std::string& theName, const std::string& theValue) const;
  std::string getGradient(const std::string& theName) const;
  std::size_t getGradientHash(const std::string& theName) const;

  // Fetch filter contents
  bool setFilter(const std::string& theName, const std::string& theValue) const;
  std::string getFilter(const std::string& theName) const;
  std::size_t getFilterHash(const std::string& theName) const;

  // Add attributes
  void addAttributes(CTPP::CDT& theGlobals, const Attributes& theAttributes);

  void addAttributes(CTPP::CDT& theGlobals, CTPP::CDT& theLocals, const Attributes& theAttributes);

  // Add presentation attributes

  void addPresentationAttributes(CTPP::CDT& theLayer,
                                 const boost::optional<std::string>& theCSS,
                                 const Attributes& theAttributes);

  void addPresentationAttributes(CTPP::CDT& theLayer,
                                 const boost::optional<std::string>& theCSS,
                                 const Attributes& theLayerAttributes,
                                 const Attributes& theObjectAttributes);

  // Generation stage

  void inDefs(bool flag) { itsInDefs = flag; }
  bool inDefs() const { return itsInDefs; }

  // Timer for performance measurements
  void useTimer(bool flag) { itUsesTimer = flag; }
  bool useTimer() const { return itUsesTimer; }

  // Establish WMS / regular mode
  void useWms(bool flag) { itUsesWms = flag; }
  bool useWms() const { return itUsesWms; }

  // Generate an unique ID when one cannot be generated from a qid
  std::string generateUniqueId() const;

  // Get expiration time
  const boost::optional<boost::posix_time::ptime>& getExpirationTime() const;
  void updateExpirationTime(const boost::posix_time::ptime& theTime) const;

  // Test if producer is for observations
  bool isObservation(const boost::optional<std::string>& theProducer) const;
  bool isObservation(const std::string& theProducer) const;
  
 private:
  const Plugin& itsPlugin;
  mutable std::map<Engine::Querydata::Producer, Engine::Querydata::Q> itsQCache;

  // Names which have already been used for styling
  mutable std::map<std::string, std::string> itsUsedStyles;

  // Names which have already been used as unique IDs in the SVG
  mutable std::set<std::string> itsUsedIds;

  // Symbols we already know of
  mutable std::map<std::string, std::string> itsSymbols;
  // Filters we already know of
  mutable std::map<std::string, std::string> itsFilters;
  // Markers we already know of
  mutable std::map<std::string, std::string> itsMarkers;
  // Patterns we already know of
  mutable std::map<std::string, std::string> itsPatterns;
  // Gradients we already know of
  mutable std::map<std::string, std::string> itsGradients;

  // Unique Qids created by us
  mutable std::map<std::string, std::size_t> itsQids;

  // Next ID to be used
  mutable std::size_t itsNextId = 0;

  // Estimated expiration time
  mutable boost::optional<boost::posix_time::ptime> itsExpirationTime;

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
