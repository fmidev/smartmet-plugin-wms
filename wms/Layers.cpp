#include "Layers.h"
#include "Hash.h"
#include "Layer.h"
#include "LayerFactory.h"
#include <spine/Exception.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>

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
      throw SmartMet::Spine::Exception(BCP, "Layers JSON is not a JSON array");

    for (unsigned int i = 0; i < theJson.size(); i++)
    {
      const Json::Value& json = theJson[i];
      boost::shared_ptr<Layer> layer(LayerFactory::create(json));
      layer->init(json, theState, theConfig, theProperties);
      layers.push_back(layer);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    BOOST_FOREACH (auto& layer, layers)
    {
      // Each layer may actually generate multiple CDT layers
      // (Animations, inner tags etc). Each is pushed separately
      // to the back of the layers CDT
      layer->generate(theGlobals, theLayersCdt, theState);
    }
  }
  catch (...)
  {
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
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
    throw SmartMet::Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
