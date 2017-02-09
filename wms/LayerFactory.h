// ======================================================================
/*!
 * \brief Factory for layers
 *
 * Create a layer based on the type specified in the JSON
 */
// ======================================================================

// TODO: Add caching

#pragma once

#include "Layer.h"
#include <json/json.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace LayerFactory
{
Layer* create(const Json::Value& theJson);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
