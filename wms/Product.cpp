#include "Product.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include "Warnings.h"
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <macgyver/Exception.h>
#include <spine/HTTP.h>

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

void Product::init(Json::Value& theJson, const State& theState, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Product JSON is not a JSON object (name-value pairs)");

    // Extract all members

    Properties::init(theJson, theState, theConfig);

    JsonTools::remove_string(svg_tmpl, theJson, "svg_tmpl");
    JsonTools::remove_string(type, theJson, "type");
    JsonTools::remove_int(width, theJson, "width");
    JsonTools::remove_int(height, theJson, "height");

    auto json = JsonTools::remove(theJson, "title");
    if (!json.isNull())
    {
      title = Text("Product");
      title->init(json, theConfig);
    }

    json = JsonTools::remove(theJson, "defs");
    if (!json.isNull())
      defs.init(json, theState, theConfig, *this);

    json = JsonTools::remove(theJson, "attributes");
    if (!json.isNull())
      attributes.init(json, theConfig);

    json = JsonTools::remove(theJson, "views");
    if (!json.isNull())
      views.init(json, theState, theConfig, *this);

    json = JsonTools::remove(theJson, "png");
    if (!json.isNull())
      png.init(json, theConfig);

    // refs is also allowed here

    // If SVG sizes are missing, take them from the top level projection if possible

    if (!width && projection.xsize)
      width = projection.xsize;

    if (!height && projection.ysize)
      height = projection.ysize;

#if 0
    // WMS layers may not define either directly, since the desired size is given in the query string
    if (!width || !height)
      throw Fmi::Exception(BCP, "SVG width or height is undefined");
#endif
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Product init failed!");
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
      theGlobals["title"] = title->translate(language);

    // We must process the defs section before processing the
    // product attributes or views in case they refer to some
    // of the objects defined in the defs section.

    defs.generate(theGlobals, theState);

    // The order of these two should not matter, but this
    // is the logical order.

    theState.addAttributes(theGlobals, attributes);

    views.generate(theGlobals, theState);
    if (!defs.csss.csss.empty())
      for (const auto& e : defs.csss.csss)
        theGlobals["css"][e.first + ".css"] = e.second;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Check for configuration errors
 */
// ----------------------------------------------------------------------

void Product::check_errors(const std::string& name, std::set<std::string>& warnedURLs) const
{
  try
  {
    if (warnedURLs.find(name) != warnedURLs.end())
      return;

    // For now these checks are not comprehensive. The qids used by Isoband, Isoline and Title
    // objects are not included in the check. The checks can be added if it turns out JSON
    // developers make too frequent mistakes on them too.

    Warnings warnings;
    defs.check_warnings(warnings);
    views.check_warnings(warnings);

    std::list<std::string> errors;
    for (const auto& qid_count : warnings.qid_counts)
    {
      const auto& qid = qid_count.first;
      int count = qid_count.second;
      if (count > 1)
        errors.emplace_back(fmt::format("- qid '{}' used {} times", qid, count));
    }
    if (!errors.empty())
    {
      std::string message = fmt::format("Product '{}' has the following errors:", name);
      for (const auto& err : errors)
        message += "\n" + err;
      message += '\n';
      std::cerr << message;

      warnedURLs.insert(name);
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Gather information on used grid parameters
 */
// ----------------------------------------------------------------------

ParameterInfos Product::getGridParameterInfo(const State& theState) const
{
  ParameterInfos infos;
  views.addGridParameterInfo(infos, theState);
  return infos;
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
    // Note: Fmi::hash_combine propagates Fmi::bad_hash results to final hash value
    auto hash = Fmi::hash_value(svg_tmpl);
    Fmi::hash_combine(hash, Fmi::hash_value(svg_tmpl));
    Fmi::hash_combine(hash, Fmi::hash_value(type));
    Fmi::hash_combine(hash, Fmi::hash_value(width));
    Fmi::hash_combine(hash, Fmi::hash_value(height));
    Fmi::hash_combine(hash, Dali::hash_value(title, theState));
    Fmi::hash_combine(hash, Dali::hash_value(defs, theState));
    Fmi::hash_combine(hash, Dali::hash_value(attributes, theState));
    Fmi::hash_combine(hash, Dali::hash_value(views, theState));
    Fmi::hash_combine(hash, Dali::hash_value(png, theState));
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
