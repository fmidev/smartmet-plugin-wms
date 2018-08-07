// ======================================================================

#include "Defs.h"
#include "Config.h"
#include "Hash.h"
#include "State.h"
#include "Symbols.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <spine/Exception.h>
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize the product from JSON
 */
// ----------------------------------------------------------------------

void Defs::init(const Json::Value& theJson,
                const State& theState,
                const Config& theConfig,
                const Properties& theParent)
{
  try
  {
    // Inherit properties
    Properties::init(theJson, theState, theConfig, theParent);

    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Defs JSON is not a JSON object (name-value pairs)");

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("qid", nulljson);
    if (!json.isNull())
      qid = json.asString();

    json = theJson.get("styles", nulljson);
    if (!json.isNull())
      styles.init(json, theConfig);

    json = theJson.get("layers", nulljson);
    if (!json.isNull())
      layers.init(json, theState, theConfig, *this);

    json = theJson.get("symbols", nulljson);
    if (!json.isNull())
      symbols.init(json, theState);
    json = theJson.get("filters", nulljson);
    if (!json.isNull())
      filters.init(json, theState);
    json = theJson.get("markers", nulljson);
    if (!json.isNull())
      markers.init(json, theState);
    json = theJson.get("patterns", nulljson);
    if (!json.isNull())
      patterns.init(json, theState);
    json = theJson.get("gradients", nulljson);
    if (!json.isNull())
      gradients.init(json, theState);
    json = theJson.get("css", nulljson);
    if (!json.isNull())
      csss.init(json, theState);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the definitions into the template hash tables
 */
// ----------------------------------------------------------------------

void Defs::generate(CTPP::CDT& theGlobals, State& theState)
{
  try
  {
    theState.inDefs(true);
    styles.generate(theGlobals, theState);
    theGlobals["layers"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
    layers.generate(theGlobals, theGlobals["layers"], theState);
    theState.inDefs(false);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t Defs::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(qid);
    Dali::hash_combine(hash, Dali::hash_value(styles, theState));
    Dali::hash_combine(hash, Dali::hash_value(layers, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
