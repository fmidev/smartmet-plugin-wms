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




bool Layers::getProjection(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState,Projection& projection)
{
  try
  {
    if (layers.size() == 0)
      return false;

    auto first = layers.begin();

    if (*(*first)->projection.crs != "data")
    {
      projection  = (*first)->projection;
      return true;
    }

    for (auto& layer : layers)
    {
      //std::cout << "PROJECTION (" << *layer->type <<  ") : " << *layer->projection.crs << "\n";
      if (*layer->type != "map" &&  (layer->attributes.value("display") != "none" || theState.getRequest().getParameter("optimizesize") == std::string("0")))
      {
        layer->generate(theGlobals, theLayersCdt, theState);
        if (*layer->projection.crs != "data")
        {
          projection = layer->projection;
          return true;
        }

        if (layer->layers.layers.size() > 0)
        {
          CTPP::CDT tmpGlobals(theGlobals);
          State tmpState(theState.getPlugin(),theState.getRequest());
          tmpState.setCustomer(theState.getCustomer());
          CTPP::CDT tmpLayersCdt;

          if (layer->layers.getProjection(tmpGlobals,theLayersCdt,tmpState,projection))
            return true;
        }
      }
    }

    return false;
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
    if (layers.size() == 0)
      return;

    Projection projection;

    auto first = layers.begin();

    if (*(*first)->projection.crs == "data")
    {
      CTPP::CDT tmpGlobals(theGlobals);
      State tmpState(theState.getPlugin(),theState.getRequest());
      tmpState.setCustomer(theState.getCustomer());
      CTPP::CDT tmpLayersCdt;
      getProjection(tmpGlobals,theLayersCdt,tmpState,projection);
    }
    else
    {
      projection = (*first)->projection;
    }

    for (auto& layer : layers)
    {
      // Each layer may actually generate multiple CDT layers
      // (Animations, inner tags etc). Each is pushed separately
      // to the back of the layers CDT
      //std::cout << "### PROJECTION (" << *layer->type <<  ") : " << *layer->projection.crs << "\n";

      if (layer->attributes.value("display") != "none" ||theState.getRequest().getParameter("optimizesize") == std::string("0"))
      {
        if (*layer->projection.crs == "data")
        {
          layer->projection = projection;
        }

        layer->generate(theGlobals, theLayersCdt, theState);
      }
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
