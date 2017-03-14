// ======================================================================

#include "State.h"
#include "Plugin.h"
#include <spine/Exception.h>
#include <macgyver/StringConversion.h>

#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
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

State::State(const Plugin& thePlugin)
    : itsPlugin(thePlugin),
      itsQCache(),
      itsUsedStyles(),
      itsUsedIds(),
      itsInDefs(false),
      itUsesTimer(false),
      itsCustomer(),
      itsType()
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
 * use a cache.
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

    // Cache the obtained data and return it. The cache is
    // request specific, no need for mutexes here.
    itsQCache.insert(std::make_pair(theProducer, q));
    return q;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Require the given ID to be free
 */
// ----------------------------------------------------------------------

void State::requireId(const std::string& theID)
{
  try
  {
    if (!addId(theID))
      throw Spine::Exception(BCP, ("ID '" + theID + "' is defined more than once"));
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add attributes to the generated CDT at the global level
 */
// ----------------------------------------------------------------------

void State::addAttributes(CTPP::CDT& theGlobals, const Attributes& theAttributes)
{
  try
  {
    addAttributes(theGlobals, theGlobals, theAttributes);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
                          const Attributes& theAttributes)
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

    iri = theAttributes.getLocalIri("pattern");
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getSymbol(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getSymbolHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getFilter(theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getFilterHash(theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getPattern(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getPatternHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getMarker(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getMarkerHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getGradient(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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
    return itsPlugin.getGradientHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
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

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
