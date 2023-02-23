// ======================================================================

#include "State.h"
#include "Plugin.h"
#include "StyleSheet.h"
#include <ctpp2/CDT.hpp>
#include <macgyver/Exception.h>
#include <macgyver/Hash.h>
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
      itsRequest(theRequest),
      itsLocalTimePool(boost::make_shared<TS::LocalTimePool>())
{
  auto prec = theRequest.getParameter("precision");
  if (prec)
    precision = Fmi::stod(*prec);
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

Engine::Querydata::Q State::getModel(const Engine::Querydata::Producer& theProducer) const
{
  try
  {
    // Use cached Q if there is one
    auto key = theProducer;

    auto res = itsQCache.find(key);
    if (res != itsQCache.end())
      return res->second;

    // Get the data from the engine
    auto q = itsPlugin.getQEngine().get(theProducer);

    // Update estimated expiration time for the product
    updateExpirationTime(q->expirationTime());

    // Cache the obtained data and return it. The cache is
    // request specific, no need for mutexes here.
    itsQCache.insert(std::make_pair(key, q));
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

Engine::Querydata::Q State::getModel(const Engine::Querydata::Producer& theProducer,
                                     const boost::posix_time::ptime& theOriginTime) const
{
  try
  {
    // We cache the data so that QEngine cannot delete it while we still need it for further layers
    auto key = theProducer + " @ " + Fmi::to_iso_string(theOriginTime);

    auto res = itsQCache.find(key);
    if (res != itsQCache.end())
      return res->second;

    // Get the data from the engine
    auto q = itsPlugin.getQEngine().get(theProducer, theOriginTime);

    // Update estimated expiration time for the product
    updateExpirationTime(q->expirationTime());

    // Cache the obtained data and return it. The cache is
    // request specific, no need for mutexes here.
    itsQCache.insert(std::make_pair(key, q));
    return q;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Get Q for a valid time period
 */
// ----------------------------------------------------------------------

Engine::Querydata::Q State::getModel(const Engine::Querydata::Producer& theProducer,
                                     const boost::posix_time::time_period& theTimePeriod) const
{
  try
  {
    // Use cached Q if there is one
    auto key = theProducer + " from " + Fmi::to_iso_string(theTimePeriod.begin()) + " to " +
               Fmi::to_iso_string(theTimePeriod.end());

    auto res = itsQCache.find(key);
    if (res != itsQCache.end())
      return res->second;

    // Get the data from the engine
    auto q = itsPlugin.getQEngine().get(theProducer, theTimePeriod);

    // Update estimated expiration time for the product
    updateExpirationTime(q->expirationTime());

    // Cache the obtained data and return it. The cache is
    // request specific, no need for mutexes here.
    itsQCache.insert(std::make_pair(key, q));
    return q;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to obtain data for the requested time period!")
        .addParameter("starttime", Fmi::to_iso_string(theTimePeriod.begin()))
        .addParameter("endtime", Fmi::to_iso_string(theTimePeriod.end()));
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

    std::string localIri;
    std::map<std::string,std::string> parameters;

    if (theAttributes.getLocalIriAndParameters("filter",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getFilter(itsCustomer, localIri, itUsesWms),parameters);
    }

    if (theAttributes.getLocalIriAndParameters("marker",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getMarker(itsCustomer, localIri, itUsesWms),parameters);
    }

    if (theAttributes.getLocalIriAndParameters("marker-start",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getMarker(itsCustomer, localIri, itUsesWms),parameters);
    }

    if (theAttributes.getLocalIriAndParameters("marker-mid",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getMarker(itsCustomer, localIri, itUsesWms),parameters);
    }

    if (theAttributes.getLocalIriAndParameters("marker-end",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getMarker(itsCustomer, localIri, itUsesWms),parameters);
    }

    if (theAttributes.getLocalIriAndParameters("fill",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getPattern(itsCustomer, localIri, itUsesWms),parameters);
    }

    if (theAttributes.getLocalIriAndParameters("linearGradient",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getGradient(itsCustomer, localIri, itUsesWms),parameters);
    }

    if (theAttributes.getLocalIriAndParameters("radialGradient",localIri,parameters)  &&  addId(localIri))
    {
      theGlobals["includes"][localIri] = replaceVariables(itsPlugin.getGradient(itsCustomer, localIri, itUsesWms),parameters);
    }
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
      theAttributes.generatePresentation(
          theLayer, *this, getStyle(*theCSS, theAttributes.getSelector()));
    else
      theAttributes.generatePresentation(theLayer, *this);
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
                                      const Attributes& theLayerAttributes,
                                      const Attributes& theObjectAttributes) const
{
  try
  {
    // Note: Object attributes override layer attributes
    if (theCSS)
    {
      const auto css1 = getStyle(*theCSS, theLayerAttributes.getSelector());
      theLayerAttributes.generatePresentation(theLayer, *this, css1);

      const auto css2 = getStyle(*theCSS, theObjectAttributes.getSelector());
      theObjectAttributes.generatePresentation(theLayer, *this, css2);

      // Adding presentation attributes defined in the CSS class

      if (getType() == "topojson")
      {
        StyleSheet styleSheet;
        styleSheet.add(this->getStyle(*theCSS));

        std::string selector = theLayerAttributes.getSelector();
        if (selector.empty())
          selector = theObjectAttributes.getSelector();

        if (!selector.empty())
        {
          auto decl = styleSheet.declarations(selector);
          for (const auto& name_style : decl)
            theLayer["presentation"][name_style.first] = name_style.second;
        }
      }
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
 * \brief Fetch parsed CSS from the cache
 */
// ----------------------------------------------------------------------
std::map<std::string, std::string> State::getStyle(const std::string& theCSS,
                                                   const std::string& theSelector) const
{
  try
  {
    return itsPlugin.getStyle(itsCustomer, theCSS, itUsesWms, theSelector);
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
    auto ret = itsSymbols.insert({theName, theValue});
    return ret.second;
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
    auto ret = itsFilters.insert({theName, theValue});
    return ret.second;
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
    auto ret = itsMarkers.insert({theName, theValue});
    return ret.second;
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
    auto ret = itsPatterns.insert({theName, theValue});
    return ret.second;
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
    auto ret = itsGradients.insert({theName, theValue});
    return ret.second;
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
      return Fmi::hash_value(itsSymbols[theName]);
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
    return itsPlugin.getFilter(itsCustomer, theName, itUsesWms);
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
      return Fmi::hash_value(itsFilters[theName]);

    return itsPlugin.getFilterHash(itsCustomer, theName, itUsesWms);
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
      return Fmi::hash_value(itsPatterns[theName]);
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
      return Fmi::hash_value(itsMarkers[theName]);
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
      return Fmi::hash_value(itsGradients[theName]);
    return itsPlugin.getGradientHash(itsCustomer, theName, itUsesWms);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Establish precision for coordinates
 */
// ----------------------------------------------------------------------

double State::getPrecision(const std::string& theName) const
{
  if (precision)
    return *precision;

  return getConfig().defaultPrecision(getType(), theName);
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
 * \brief Get the last modification time
 */
// ----------------------------------------------------------------------

const boost::optional<boost::posix_time::ptime>& State::getModificationTime() const
{
  return itsModificationTime;
}

// ----------------------------------------------------------------------
/*!
 * \brief Update the estimated expiration time if necessary
 *
 * The final expiration time is the one estimated to be first in the
 * future out of all the data used in the layers of the product.
 */
// ----------------------------------------------------------------------

void State::updateModificationTime(const boost::posix_time::ptime& theTime) const
{
  if (!itsModificationTime)
    itsModificationTime = theTime;
  else
    itsModificationTime = std::max(*itsModificationTime, theTime);
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
