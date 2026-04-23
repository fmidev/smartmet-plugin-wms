// ======================================================================
/*!
 * \brief JSON catalog endpoint for Dali products
 *
 * Walks the Dali product tree (customers/X/products/Y.json),
 * extracts lightweight metadata from each product JSON, looks up the
 * producer's time dimension via the Querydata engine, and emits a
 * hierarchical JSON response. Consumers use this to drive a Dali layer
 * browser (e.g. leaflet-fmi) without needing the WMS GetCapabilities
 * pathway, since Dali products are not OGC layers.
 */
// ======================================================================

#pragma once

#include <spine/HTTP.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Plugin;
class State;

class DaliCapabilities
{
 public:
  explicit DaliCapabilities(const Plugin& thePlugin);

  // Produce the JSON catalog response. The 'customer' query parameter,
  // if present, restricts the walk to that customer's products.
  std::string generate(const Spine::HTTP::Request& theRequest, State& theState) const;

 private:
  const Plugin& itsPlugin;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
