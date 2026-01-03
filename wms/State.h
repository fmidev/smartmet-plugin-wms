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
#include <engines/geonames/Engine.h>
#include <engines/grid/Engine.h>
#include <engines/querydata/Q.h>
#include <grid-files/common/ImageFunctions.h>
#include <spine/HTTP.h>
#include <timeseries/TimeSeriesInclude.h>
#include <map>
#include <optional>
#include <set>
#include <vector>

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
namespace Grid
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
  State(Plugin& thePlugin, const Spine::HTTP::Request& theRequest);

  // Get the engines
  const Engine::Grid::Engine* getGridEngine() const;
  const Engine::Querydata::Engine& getQEngine() const;
  const Engine::Contour::Engine& getContourEngine() const;
  const Engine::Gis::Engine& getGisEngine() const;
  const Engine::Geonames::Engine& getGeoEngine() const;
#ifndef WITHOUT_OBSERVATION
  Engine::Observation::Engine& getObsEngine() const;
#endif

  // Give access to configuration variables
  const Config& getConfig() const;

  const Spine::HTTP::Request& getRequest() const { return itsRequest; }
  Plugin& getPlugin() { return itsPlugin; }

  const std::string& getName() const { return itsName; }
  void setName(const std::string& theName) { itsName = theName; }

  // Customer
  const std::string& getCustomer() const { return itsCustomer; }
  void setCustomer(const std::string& theCustomer) { itsCustomer = theCustomer; }
  // Format
  const std::string& getType() const { return itsType; }
  void setType(const std::string& theType) { itsType = theType; }
  // Get Q to be used in the current product
  Engine::Querydata::Q getModel(const Engine::Querydata::Producer& theProducer) const;
  Engine::Querydata::Q getModel(const Engine::Querydata::Producer& theProducer,
                                const Fmi::DateTime& theOriginTime) const;

  // If no origintime is set, try extracting a minimal part of multifiles for example to keep the
  // data has for old radar files the same even if newer ones have overridden the hash value for
  // the full multifile.
  Engine::Querydata::Q getModel(const Engine::Querydata::Producer& theProducer,
                                const Fmi::TimePeriod& theTimePeriod) const;

  // Require given ID to be free, and mark it used if it is free
  void requireId(const std::string& theID) const;

  // If given ID has not been used, mark it used now
  bool addId(const std::string& theID) const;

  // Create unique ID for the given prefix
  std::string makeQid(const std::string& thePrefix) const;

  // Fetch CSS contents
  std::string getStyle(const std::string& theCSS) const;
  std::map<std::string, std::string> getStyle(const std::string& theCSS,
                                              const std::string& theSelector) const;

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

  // Fetch colormap contents
  bool setColorMap(const std::string& theName, const std::string& theValue) const;
  std::string getColorMap(const std::string& theName) const;
  std::size_t getColorMapHash(const std::string& theName) const;

  // Fetch filter contents
  bool setFilter(const std::string& theName, const std::string& theValue) const;
  std::string getFilter(const std::string& theName) const;
  std::size_t getFilterHash(const std::string& theName) const;

  // Get precision
  double getPrecision(const std::string& theName) const;

  // Add attributes
  void addAttributes(CTPP::CDT& theGlobals, const Attributes& theAttributes) const;

  void addAttributes(CTPP::CDT& theGlobals,
                     CTPP::CDT& theLocals,
                     const Attributes& theAttributes) const;

  // Add presentation attributes

  void addPresentationAttributes(CTPP::CDT& theLayer,
                                 const std::optional<std::string>& theCSS,
                                 const Attributes& theAttributes) const;

  void addPresentationAttributes(CTPP::CDT& theLayer,
                                 const std::optional<std::string>& theCSS,
                                 const Attributes& theLayerAttributes,
                                 const Attributes& theObjectAttributes) const;

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
  const std::optional<Fmi::DateTime>& getExpirationTime() const;
  void updateExpirationTime(const Fmi::DateTime& theTime) const;

  // Get modification time
  const std::optional<Fmi::DateTime>& getModificationTime() const;
  void updateModificationTime(const Fmi::DateTime& theTime) const;

  // Test if producer is for observations
  bool isObservation(const std::optional<std::string>& theProducer) const;
  bool isObservation(const std::string& theProducer) const;

  mutable uint arcCounter = 0;
  mutable uint insertCounter = 0;
  mutable std::map<std::size_t, uint> arcHashMap;
  mutable bool animation_enabled = false;
  mutable int animation_timestep = 0;
  mutable int animation_loopstep = 0;
  mutable int animation_timesteps = 0;
  mutable int animation_loopsteps = 0;

 private:
  Plugin& itsPlugin;
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
  // Colormaps we already know of
  mutable std::map<std::string, std::string> itsColorMaps;

  // Unique Qids created by us
  mutable std::map<std::string, std::size_t> itsQids;

  // Next ID to be used
  mutable std::size_t itsNextId = 0;

  // Estimated expiration time
  mutable std::optional<Fmi::DateTime> itsExpirationTime;

  // Last modification time
  mutable std::optional<Fmi::DateTime> itsModificationTime;

  // Are we in the Defs section?
  bool itsInDefs = false;

  // Should we time execution?
  bool itUsesTimer = false;

  // Are we in WMS mode?
  bool itUsesWms = false;

  // Generic name for the requested product is either customer/product of WMS LAYERS value
  std::string itsName;

  // The customer
  std::string itsCustomer;

  // The image format
  std::string itsType;

  // Precision
  std::optional<double> precision;

  // The request itself
  const Spine::HTTP::Request itsRequest;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
