// ======================================================================

#include "Product.h"
#include "Config.h"
#include "Hash.h"
#include "State.h"
#include <spine/Exception.h>
#include <spine/HTTP.h>
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
 * \brief Initialize the product from JSON
 */
// ----------------------------------------------------------------------

void Product::init(const Json::Value& theJson, const State& theState, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Product JSON is not a JSON object (name-value pairs)");

    // Extract all members

    Json::Value nulljson;
    Properties::init(theJson, theState, theConfig);

    auto json = theJson.get("svg_tmpl", nulljson);
    if (!json.isNull())
      svg_tmpl = json.asString();

    json = theJson.get("type", nulljson);
    if (!json.isNull())
      type = json.asString();

    json = theJson.get("width", nulljson);
    if (!json.isNull())
      width = json.asInt();

    json = theJson.get("height", nulljson);
    if (!json.isNull())
      height = json.asInt();

    json = theJson.get("title", nulljson);
    if (!json.isNull())
      title = json.asString();

    json = theJson.get("defs", nulljson);
    if (!json.isNull())
      defs.init(json, theState, theConfig, *this);

    json = theJson.get("attributes", nulljson);
    if (!json.isNull())
      attributes.init(json, theConfig);

    json = theJson.get("views", nulljson);
    if (!json.isNull())
      views.init(json, theState, theConfig, *this);

    // refs is also allowed here

    // If SVG sizes are missing, take them from the top level projection if possible

    if (!width && projection.xsize)
      width = projection.xsize;

    if (!height && projection.ysize)
      height = projection.ysize;

    // The reverse is not allowed
    if (!width || !height)
      throw Spine::Exception(BCP, "SVG width or height is undefined");
  }
  catch (...)
  {
    throw Spine::Exception(BCP, "Product init failed!", NULL);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the product into the template hash tables
 */
// ----------------------------------------------------------------------

void Product::generate(CTPP::CDT& theGlobals, State& theState)
{
  try
  {
    // Initialize the structure

    theGlobals["styles"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theGlobals["css"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theGlobals["includes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theGlobals["layers"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
    theGlobals["paths"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theGlobals["views"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);

    // And build the product itself

    theGlobals["start"] = "<g";
    theGlobals["end"] = "</g>";

    if (svg_tmpl)
      theGlobals["svg_tmpl"] = *svg_tmpl;
    if (width)
      theGlobals["width"] = *width;
    if (height)
      theGlobals["height"] = *height;
    if (title)
      theGlobals["title"] = *title;

    // We must process the defs section before processing the
    // product attributes or views in case they refer to some
    // of the objects defined in the defs section.

    defs.generate(theGlobals, theState);

    // The order of these two should not matter, but this
    // is the logical order.

    theState.addAttributes(theGlobals, attributes);

    views.generate(theGlobals, theState);
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

std::size_t Product::hash_value(const State& theState) const
{
  try
  {
    auto hash = Dali::hash_value(svg_tmpl);
    boost::hash_combine(hash, Dali::hash_value(svg_tmpl));
    boost::hash_combine(hash, Dali::hash_value(type));
    boost::hash_combine(hash, Dali::hash_value(width));
    boost::hash_combine(hash, Dali::hash_value(height));
    boost::hash_combine(hash, Dali::hash_value(title));
    boost::hash_combine(hash, Dali::hash_value(defs, theState));
    boost::hash_combine(hash, Dali::hash_value(attributes, theState));
    boost::hash_combine(hash, Dali::hash_value(views, theState));
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
