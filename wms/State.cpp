// ======================================================================

#include "State.h"
#include "Plugin.h"
#include <ctpp2/CDT.hpp>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <stdexcept>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Set the state of the plugin
 *
 * The alternative would be to have queries which would always have
 * as input both the State object and the plugin object, which means
 * we would have to either make QEngine a public member of the plugin,
 * provide an accessor for it, or provide an accessor for a model.
 * Initializing the state with the QEngine clarifies usage - you
 * use only one variable with a state, and you cannot bypass it.
 * We MUST use the same Q object throughout the generation of
 * the product, even if the latest Q for the chosen model is updated
 * during the generation of the product.
 */
// ----------------------------------------------------------------------

State::State(Plugin& thePlugin, const Spine::HTTP::Request& theRequest)
    : itsPlugin(thePlugin),
      itsInDefs(false),
      itUsesTimer(false),
      itUsesWms(false),
      itsRequest(theRequest)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the contour engine
 */
// ----------------------------------------------------------------------

const Engine::Contour::Engine& State::getContourEngine() const
{
  try
  {
    return itsPlugin.getContourEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the grid engine
 */
// ----------------------------------------------------------------------

const Engine::Grid::Engine* State::getGridEngine() const
{
  try
  {
    return itsPlugin.getGridEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the querydata engine
 */
// ----------------------------------------------------------------------

const Engine::Querydata::Engine& State::getQEngine() const
{
  try
  {
    return itsPlugin.getQEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the GIS engine
 */
// ----------------------------------------------------------------------

const Engine::Gis::Engine& State::getGisEngine() const
{
  try
  {
    return itsPlugin.getGisEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the GEO engine
 */
// ----------------------------------------------------------------------

const Engine::Geonames::Engine& State::getGeoEngine() const
{
  try
  {
    return itsPlugin.getGeoEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the OBS engine
 */
// ----------------------------------------------------------------------

#ifndef WITHOUT_OBSERVATION
Engine::Observation::Engine& State::getObsEngine() const
{
  try
  {
    return itsPlugin.getObsEngine();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}
#endif

// ----------------------------------------------------------------------
/*!
 * \brief Get the configuration object
 */
// ----------------------------------------------------------------------

const Config& State::getConfig() const
{
  try
  {
    return itsPlugin.getConfig();
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get cached Q
 *
 * We want to use the same Q in all layers even though a newer one
 * might become available while the product is being generated.
 * Hence we cache all Q requests to the QEngine to make sure
 * we use the same data all the time.
 *
 * If a specific origin time is requested, there is no need to
 * use a cache, but expiration time estimation would have to
 * be updated.
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q State::get(const Engine::Querydata::Producer& theProducer) const
{
  try
  {
    // Use cached Q if there is one
    auto res = itsQCache.find(theProducer);
    if (res != itsQCache.end())
      return res->second;

    // Get the data from the engine
    auto q = itsPlugin.getQEngine().get(theProducer);

    // Update estimated expiration time for the product
    updateExpirationTime(q->expirationTime());

    // Cache the obtained data and return it. The cache is
    // request specific, no need for mutexes here.
    itsQCache.insert(std::make_pair(theProducer, q));
    return q;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Q with origintime
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q State::get(const Engine::Querydata::Producer& theProducer,
                                const boost::posix_time::ptime& theOriginTime) const
{
  try
  {
    return itsPlugin.getQEngine().get(theProducer, theOriginTime);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Require the given ID to be free
 */
// ----------------------------------------------------------------------

void State::requireId(const std::string& theID) const
{
  try
  {
    if (!addId(theID))
      throw Fmi::Exception(BCP, ("ID '" + theID + "' is defined more than once"));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add ID to registry of used names
 */
// ----------------------------------------------------------------------

bool State::addId(const std::string& theID) const
{
  try
  {
    if (itsUsedIds.find(theID) != itsUsedIds.end())
      return false;

    itsUsedIds.insert(theID);
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add attributes to the generated CDT at the global level
 */
// ----------------------------------------------------------------------

void State::addAttributes(CTPP::CDT& theGlobals, const Attributes& theAttributes) const
{
  try
  {
    addAttributes(theGlobals, theGlobals, theAttributes);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add attributes to the generated CDT
 *
 * Adding attributes involves:
 *
 *   - Adding attribute names and values to the local CDT
 *   - Including necessary definitions from url(#id) references to globals
 *
 * Section "17.1.4 Processing of IRI references" list multiple tags
 * which can reference external data. The ones which may cause
 * an include into the defs section are:
 *
 *	  filters   : filter
 *    markers   : marker, marker-start, marker-mid, marker-end
 *    patterns  : pattern
 *    gradients : linearGradient, radialGradient
 *
 * The include is done if the value of the attribute is of the form
 * "url(#elementID) *AND* elementID has not been defined so far.
 * This allows the user to define a component in the defs section
 * instead of including if from an external file.
 *
 * The reference may in SVG be also be an xpointer, but we do not
 * support that form in generating SVG from JSON configs.
 */
// ----------------------------------------------------------------------

void State::addAttributes(CTPP::CDT& theGlobals,
                          CTPP::CDT& theLocals,
                          const Attributes& theAttributes) const
{
  try
  {
    // The locals are easily handled by inserting them to the CDT
    theAttributes.generate(theLocals, *this);

    // See also Attributes::hash_value!

    // Now process the globals

    auto iri = theAttributes.getLocalIri("filter");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getFilter(*iri, itUsesWms);

    iri = theAttributes.getLocalIri("marker");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getMarker(itsCustomer, *iri, itUsesWms);

    iri = theAttributes.getLocalIri("marker-start");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getMarker(itsCustomer, *iri, itUsesWms);

    iri = theAttributes.getLocalIri("marker-mid");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getMarker(itsCustomer, *iri, itUsesWms);

    iri = theAttributes.getLocalIri("marker-end");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getMarker(itsCustomer, *iri, itUsesWms);

    iri = theAttributes.getLocalIri("fill");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getPattern(itsCustomer, *iri, itUsesWms);

    iri = theAttributes.getLocalIri("linearGradient");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getGradient(itsCustomer, *iri, itUsesWms);

    iri = theAttributes.getLocalIri("radialGradient");
    if (iri && addId(*iri))
      theGlobals["includes"][*iri] = itsPlugin.getGradient(itsCustomer, *iri, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add presentation attributes to the CDT
 */
// ----------------------------------------------------------------------

void State::addPresentationAttributes(CTPP::CDT& theLayer,
                                      const boost::optional<std::string>& theCSS,
                                      const Attributes& theAttributes) const
{
  try
  {
    if (theCSS)
      theAttributes.generatePresentation(theLayer, *this, getStyle(*theCSS));
    else
      theAttributes.generatePresentation(theLayer, *this);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
/*!
 * \brief Add presentation attributes to the CDT
 */
// ----------------------------------------------------------------------

void State::addPresentationAttributes(CTPP::CDT& theLayer,
                                      const boost::optional<std::string>& theCSS,
                                      const Attributes& theLayerAttributes,
                                      const Attributes& theObjectAttributes) const
{
  try
  {
    // Note: Object attributes override layer attributes
    if (theCSS)
    {
      const auto css = getStyle(*theCSS);
      theLayerAttributes.generatePresentation(theLayer, *this, css);
      theObjectAttributes.generatePresentation(theLayer, *this, css);
    }
    else
    {
      theLayerAttributes.generatePresentation(theLayer, *this);
      theObjectAttributes.generatePresentation(theLayer, *this);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch CSS from the cache
 */
// ----------------------------------------------------------------------

std::string State::getStyle(const std::string& theCSS) const
{
  try
  {
    return itsPlugin.getStyle(itsCustomer, theCSS, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Store symbol SVG to state object, overriding any lower layer data
 */
// ----------------------------------------------------------------------
bool State::setSymbol(const std::string& theName, const std::string& theValue) const
{
  try
  {
    if (itsSymbols.count(theName) > 0)
    {
      itsSymbols[theName] = theValue;
      return false;  // Overriding symbols already in there
    }
    itsSymbols[theName] = theValue;
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Store filter SVG to state object, overriding any lower layer data
 */
// ----------------------------------------------------------------------
bool State::setFilter(const std::string& theName, const std::string& theValue) const
{
  try
  {
    if (itsFilters.count(theName) > 0)
    {
      itsFilters[theName] = theValue;
      return false;  // Overriding filters already in there
    }
    itsFilters[theName] = theValue;
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Store marker SVG to state object, overriding any lower layer data
 */
// ----------------------------------------------------------------------
bool State::setMarker(const std::string& theName, const std::string& theValue) const
{
  try
  {
    if (itsMarkers.count(theName) > 0)
    {
      itsMarkers[theName] = theValue;
      return false;  // Overriding markers already in there
    }
    itsMarkers[theName] = theValue;
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Store pattern SVG to state object, overriding any lower layer data
 */
// ----------------------------------------------------------------------
bool State::setPattern(const std::string& theName, const std::string& theValue) const
{
  try
  {
    if (itsPatterns.count(theName) > 0)
    {
      itsPatterns[theName] = theValue;
      return false;  // Overriding patterns already in there
    }
    itsPatterns[theName] = theValue;
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Store gradient SVG to state object, overriding any lower layer data
 */
// ----------------------------------------------------------------------
bool State::setGradient(const std::string& theName, const std::string& theValue) const
{
  try
  {
    if (itsGradients.count(theName) > 0)
    {
      itsGradients[theName] = theValue;
      return false;  // Overriding gradients already in there
    }
    itsGradients[theName] = theValue;
    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch symbol SVG from the cache
 */
// ----------------------------------------------------------------------

std::string State::getSymbol(const std::string& theName) const
{
  try
  {
    if (itsSymbols.count(theName) > 0)
      return itsSymbols[theName];
    return itsPlugin.getSymbol(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch symbol hash value
 */
// ----------------------------------------------------------------------

std::size_t State::getSymbolHash(const std::string& theName) const
{
  try
  {
    if (itsSymbols.count(theName) > 0)
      return boost::hash_value(itsSymbols[theName]);
    return itsPlugin.getSymbolHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch filter SVG from the cache
 */
// ----------------------------------------------------------------------

std::string State::getFilter(const std::string& theName) const
{
  try
  {
    if (itsFilters.count(theName) > 0)
      return itsFilters[theName];
    return itsPlugin.getFilter(theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch filter hash value
 */
// ----------------------------------------------------------------------

std::size_t State::getFilterHash(const std::string& theName) const
{
  try
  {
    if (itsFilters.count(theName) > 0)
      return boost::hash_value(itsFilters[theName]);
    return itsPlugin.getFilterHash(theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch pattern SVG from the cache
 */
// ----------------------------------------------------------------------

std::string State::getPattern(const std::string& theName) const
{
  try
  {
    if (itsPatterns.count(theName) > 0)
      return itsPatterns[theName];
    return itsPlugin.getPattern(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch pattern hash from the cache
 */
// ----------------------------------------------------------------------

std::size_t State::getPatternHash(const std::string& theName) const
{
  try
  {
    if (itsPatterns.count(theName) > 0)
      return boost::hash_value(itsPatterns[theName]);
    return itsPlugin.getPatternHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch marker SVG from the cache
 */
// ----------------------------------------------------------------------

std::string State::getMarker(const std::string& theName) const
{
  try
  {
    if (itsMarkers.count(theName) > 0)
      return itsMarkers[theName];
    return itsPlugin.getMarker(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch marker hash
 */
// ----------------------------------------------------------------------

std::size_t State::getMarkerHash(const std::string& theName) const
{
  try
  {
    if (itsMarkers.count(theName) > 0)
      return boost::hash_value(itsMarkers[theName]);
    return itsPlugin.getMarkerHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch gradient SVG from the cache
 */
// ----------------------------------------------------------------------

std::string State::getGradient(const std::string& theName) const
{
  try
  {
    if (itsGradients.count(theName) > 0)
      return itsGradients[theName];
    return itsPlugin.getGradient(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Fetch gradient hash value
 */
// ----------------------------------------------------------------------

std::size_t State::getGradientHash(const std::string& theName) const
{
  try
  {
    if (itsGradients.count(theName) > 0)
      return boost::hash_value(itsGradients[theName]);
    return itsPlugin.getGradientHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate a new unique ID valid for SVG
 */
//----------------------------------------------------------------------

std::string State::generateUniqueId() const
{
  std::string ret = "generated_id_";
  ret += Fmi::to_string(itsNextId++);
  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Get the estimated expiration time
 */
// ----------------------------------------------------------------------

const boost::optional<boost::posix_time::ptime>& State::getExpirationTime() const
{
  return itsExpirationTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Update the estimated expiration time if necessary
 *
 * The final expiration time is the one estimated to be first in the
 * future out of all the data used in the layers of the product.
 */
// ----------------------------------------------------------------------

void State::updateExpirationTime(const boost::posix_time::ptime& theTime) const
{
  if (!itsExpirationTime)
    itsExpirationTime = theTime;
  else
    itsExpirationTime = std::min(*itsExpirationTime, theTime);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return a generated QID for the given prefix
 *
 * We simply count the number of times each prefix has been used and
 * appends the counter to the prefix.
 */
// ----------------------------------------------------------------------

std::string State::makeQid(const std::string& thePrefix) const
{
  auto num = ++itsQids[thePrefix];
  return thePrefix + Fmi::to_string(num);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the producer is for observations
 */
// ----------------------------------------------------------------------

bool State::isObservation(const boost::optional<std::string>& theProducer) const
{
  std::string model = (theProducer ? *theProducer : getConfig().defaultModel());
  return isObservation(model);
}

// ----------------------------------------------------------------------
/*!
 * \brief Return true if the producer is for observations
 */
// ----------------------------------------------------------------------

bool State::isObservation(const std::string& theProducer) const
{
#ifdef WITHOUT_OBSERVATION
  return false;
#else
  if (getConfig().obsEngineDisabled())
    return false;

  auto observers = getObsEngine().getValidStationTypes();
  return (observers.find(theProducer) != observers.end());
#endif
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
