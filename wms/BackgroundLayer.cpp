#include "BackgroundLayer.h"
#include "Config.h"
#include "Layer.h"
#include "State.h"

#include <ctpp2/CDT.hpp>
#include <gis/Box.h>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>

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

void BackgroundLayer::init(const Json::Value& theJson,
                           const State& theState,
                           const Config& theConfig,
                           const Properties& theProperties)
{
  try
  {
    Layer::init(theJson, theState, theConfig, theProperties);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void BackgroundLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Establish the data in case it is needed for the size

    auto q = getModel(theState);

    // Get projection details

    projection.update(q);
    auto crs = projection.getCRS();
    const auto& box = projection.getBox();

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // The output consists of a rect tag only. We could output a group
    // only without any tags, but the output is nicer looking this way

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "";
    group_cdt["end"] = "";
    group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);

    // Use tag inside layer instead

    CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
    tag_cdt["start"] = "<rect";
    tag_cdt["end"] = "/>";
    theState.addAttributes(theGlobals, tag_cdt, attributes);
    tag_cdt["attributes"]["width"] = Fmi::to_string(box.width());
    tag_cdt["attributes"]["height"] = Fmi::to_string(box.height());

    group_cdt["tags"].PushBack(tag_cdt);
    theLayersCdt.PushBack(group_cdt);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t BackgroundLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
