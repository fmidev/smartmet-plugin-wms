#include "NullLayer.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void NullLayer::init(Json::Value& theJson,
                     const State& theState,
                     const Config& theConfig,
                     const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Null-layer JSON is not a JSON object");

    // This will check for trivial configuration errors on basic properties
    Layer::init(theJson, theState, theConfig, theProperties);

    // Remove all remaining settings so that the plugin will not warn on unused
    // settings. Null layer is used to disable stuff easily, remaining settings
    // are there to enable something easily.

    theJson = Json::Value();
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

void NullLayer::check_warnings(Warnings& warnings) const
{
  // No warnings on qid since nothing is generated
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void NullLayer::generate(CTPP::CDT& /* theGlobals */,
                         CTPP::CDT& /* theLayersCdt */,
                         State& /* theState */)
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t NullLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
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
