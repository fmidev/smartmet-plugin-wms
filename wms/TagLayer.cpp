#include "TagLayer.h"
#include "Hash.h"
#include "Layer.h"
#include "State.h"
#include <spine/Exception.h>
#include <ctpp2/CDT.hpp>
#include <boost/foreach.hpp>

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

void TagLayer::init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Symbol-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    Json::Value nulljson;

    auto json = theJson.get("tag", nulljson);
    if (!json.isNull())
      tag = json.asString();

    json = theJson.get("cdata", nulljson);
    if (!json.isNull())
      (cdata = Text())->init(json, theConfig);
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 */
// ----------------------------------------------------------------------

void TagLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!tag || tag->empty())
      throw Spine::Exception(BCP, "TagLayer tag must be defined and be non-empty");

    if (!validLayer(theState))
      return;

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Detect whether we're defining a composite tag such as
    // clipPath or mask or a normal element such as circle
    // or polyline based on the presence of child elements
    // or cdata to be embedded in the tag.

    bool composite = !layers.empty() || cdata;

    if (!composite)
    {
      // Do not group this layer and use attributes in tag instead

      CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
      group_cdt["start"] = "";
      group_cdt["end"] = "";
      group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);

      // Use tag inside layer instead

      CTPP::CDT tag_cdt(CTPP::CDT::HASH_VAL);
      tag_cdt["start"] = "<" + *tag;
      tag_cdt["end"] = "/>";
      theState.addAttributes(theGlobals, tag_cdt, attributes);

      group_cdt["tags"].PushBack(tag_cdt);
      theLayersCdt.PushBack(group_cdt);
    }
    else
    {
      // The tag is a composite
      CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
      group_cdt["start"] = "<" + *tag;
      group_cdt["end"] = "";
      group_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
      if (cdata)
        group_cdt["cdata"] = cdata->translate(language);
      theState.addAttributes(theGlobals, group_cdt, attributes);

      theLayersCdt.PushBack(group_cdt);

      // Then expand the inner layers

      layers.generate(theGlobals, theLayersCdt, theState);

      // Add the extra terminator to end the composite element as well once
      // the last layer has has been terminated.

      std::string end;

      if (layers.empty() && cdata)
        ;  // for example <text>Foo bar</text>
      else
      {
        end += "\n";
        if (!theState.inDefs())  // ident in body, not in defs
          end += "  ";
      }
      end += "</" + *tag + ">";

      theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat(end);
    }
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Hash value
 */
// ----------------------------------------------------------------------

std::size_t TagLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, Dali::hash_value(tag));
    boost::hash_combine(hash, Dali::hash_value(cdata, theState));
    return hash;
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Operation failed!", NULL);
  }
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
