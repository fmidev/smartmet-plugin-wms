#include "Layers.h"
#include "Hash.h"
#include "Layer.h"
#include "LayerFactory.h"

#include <boost/functional/hash.hpp>
#include <ctpp2/CDT.hpp>
#include <spine/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Initialize the layers from JSON
 */
// ----------------------------------------------------------------------

void Layers::init(const Json::Value& theJson,
                  const State& theState,
                  const Config& theConfig,
                  const Properties& theProperties)
{
  try
  {
    if (!theJson.isArray())
      throw Spine::Exception(BCP, "Layers JSON is not a JSON array");

    for (const auto& json : theJson)
    {
      boost::shared_ptr<Layer> layer(LayerFactory::create(json));
      layer->init(json, theState, theConfig, theProperties);
      layers.push_back(layer);
    }
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

void Layers::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    for (auto& layer : layers)
    {
      // Each layer may actually generate multiple CDT layers
      // (Animations, inner tags etc). Each is pushed separately
      // to the back of the layers CDT
      layer->generate(theGlobals, theLayersCdt, theState);
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value = combined hash from all layers
 */
// ----------------------------------------------------------------------

std::size_t Layers::hash_value(const State& theState) const
{
  try
  {
    return Dali::hash_value(layers, theState);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
