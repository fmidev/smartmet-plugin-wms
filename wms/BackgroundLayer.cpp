#include "BackgroundLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Layer.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <gis/Box.h>
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    bool has_data_proj = (projection.crs && *projection.crs == "data");

    // Update projection from querydata if necessary
    if (has_data_proj)
    {
      if (!source || *source != "grid")
      {
        auto q = getModel(theState);
        projection.update(q);
      }
      else
      {
        return;
      }
    }

    projection.update(q);
    // const auto& crs = projection.getCRS();
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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
    std::size_t hash = 0;
    Dali::hash_combine(hash, Layer::hash_value(theState));
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
