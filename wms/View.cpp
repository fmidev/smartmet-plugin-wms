// ======================================================================

#include "View.h"
#include "Attributes.h"
#include "Config.h"
#include "Hash.h"
#include "Product.h"
#include "State.h"

#include <ctpp2/CDT.hpp>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize the view from JSON
 */
// ----------------------------------------------------------------------

void View::init(Json::Value& theJson,
                const State& theState,
                const Config& theConfig,
                const Properties& theProperties)
{
  try
  {
    // This special case probably does not make any sense, but we'' check it anyway
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "View JSON is not a JSON object (name-value pairs)");

    Properties::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("qid", nulljson);
    if (!json.isNull())
      qid = json.asString();

    json = theJson.get("attributes", nulljson);
    if (!json.isNull())
      attributes.init(json, theConfig);

    json = theJson.get("layers", nulljson);
    if (!json.isNull())
      layers.init(json, theState, theConfig, *this);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate warnings if needed
 */
// ----------------------------------------------------------------------

void View::check_warnings(Warnings& warnings) const
{
  try
  {
    if (attributes.value("display") == "none")
      return;

    if (!qid.empty())
      ++warnings.qid_counts[qid];

    layers.check_warnings(warnings);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the view into the template hash tables
 */
// ----------------------------------------------------------------------

void View::generate(CTPP::CDT& theGlobals, CTPP::CDT& theViewCdt, State& theState)
{
  try
  {
    if (attributes.value("display") == "none" &&
        theState.getRequest().getParameter("optimizesize") != std::string("0"))
      return;

    // Currently we always group the view
    theViewCdt["start"] = "<g";
    theViewCdt["end"] = "</g>";

    theState.addAttributes(theGlobals, theViewCdt, attributes);

    theViewCdt["layers"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);

    layers.generate(theGlobals, theViewCdt["layers"], theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract information on used parameters
 */
// ----------------------------------------------------------------------

void View::addGridParameterInfo(ParameterInfos& infos, const State& theState) const
{
  layers.addGridParameterInfo(infos, theState);
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value = combined hash from all layers
 *
 * Other members are ignored, their hash comes from the hash
 * for the JSON string.
 */
// ----------------------------------------------------------------------

std::size_t View::hash_value(const State& theState) const
{
  try
  {
    auto hash = Properties::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(qid));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(layers, theState));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
