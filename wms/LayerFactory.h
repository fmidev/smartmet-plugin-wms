// ======================================================================
/*!
 * \brief Factory for layers
 *
 * Create a layer based on the type specified in the JSON
 */
// ======================================================================

// TODO: Add caching

#pragma once

#include <json/json.h>

namespace SmartMet::Plugin::Dali
{
class Layer;
}  // namespace SmartMet::Plugin::Dali

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
