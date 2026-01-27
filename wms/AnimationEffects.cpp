#include "AnimationEffects.h"
#include "Hash.h"
#include "JsonTools.h"

#include <boost/numeric/conversion/cast.hpp>
#include <macgyver/Exception.h>
#include <stdexcept>
#include <spine/Json.h>

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

void AnimationEffects::init(Json::Value &theJson)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isArray())
      throw Fmi::Exception(BCP, "Ranges JSON is not a JSON array");

    for (auto& json : theJson)
    {
      if (!json.isNull())
        initEffect(json);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void AnimationEffects::initEffects(Json::Value &theJson)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isArray())
      throw Fmi::Exception(BCP, "Ranges JSON is not a JSON array");

    for (auto& json : theJson)
    {
      if (!json.isNull())
        initEffect(json);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}




void AnimationEffects::initEffect(Json::Value& theJson)
{
  try
  {
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Effects JSON is not a JSON object");

    Effect effect;

    effect.enabled = false;
    effect.start_step = 0;
    effect.end_step = 10000;

    JsonTools::remove_bool(effect.enabled, theJson, "enabled");
    JsonTools::remove_string(effect.effect, theJson, "effect");
    JsonTools::remove_int(effect.start_step, theJson, "start_step");
    JsonTools::remove_int(effect.end_step, theJson, "end_step");


    effects.push_back(effect);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value for the layer
 */
// ----------------------------------------------------------------------

std::size_t AnimationEffects::hash_value() const
{
  try
  {
    std::size_t hash = 0;
    for (auto it = effects.begin(); it != effects.end(); ++it)
    {
      auto hash = Fmi::hash_value(it->enabled);
      Fmi::hash_combine(hash, Fmi::hash_value(it->effect));
      Fmi::hash_combine(hash, Fmi::hash_value(it->start_step));
      Fmi::hash_combine(hash, Fmi::hash_value(it->end_step));
    }

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
