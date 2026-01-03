#include "GroupLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "RasterLayer.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <gis/Box.h>
#include <grid-files/common/ImageFunctions.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

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

void GroupLayer::init(Json::Value& theJson,
                      const State& theState,
                      const Config& theConfig,
                      const Properties& theProperties)
{
  try
  {
    Layer::init(theJson, theState, theConfig, theProperties);

    compression = 1;
    JsonTools::remove_int(compression, theJson, "compression");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void GroupLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Generating layers
    layers.generate(theGlobals, theLayersCdt, theState);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t GroupLayer::hash_value(const State& theState) const
{
  try
  {
    std::size_t hash = 0;
    Fmi::hash_combine(hash, Layer::hash_value(theState));
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
