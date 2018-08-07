//======================================================================

#include "LegendLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "Layer.h"
#include "State.h"
#include <boost/move/make_unique.hpp>
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <macgyver/StringConversion.h>
#include <spine/Exception.h>
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Pretty print a number with given precision
 *
 * From: gis/source/OGR-exportToSvg.cpp
 */
// ----------------------------------------------------------------------

std::string pretty(double num, const char* format)
{
  if (strcmp(format, "%.0f") == 0)
    return fmt::sprintf("%d", static_cast<long>(round(num)));

  std::string ret = fmt::sprintf(format, num);
  std::size_t pos = ret.size();
  while (pos > 0 && ret[--pos] == '0')
  {
  }

  if (ret[pos] != ',' && ret[pos] != '.')
    return ret;

  ret.resize(pos);

  if (ret != "-0")
    return ret;
  else
    return "0";
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a legend number
 */
// ----------------------------------------------------------------------

std::string legend_text(double theValue, const LegendLabels& theLabels)
{
  try
  {
    if (!theLabels.format)
      return Fmi::to_string(theValue);

    return pretty(theValue, theLabels.format->c_str());
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the text for a legend symbol
 */
// ----------------------------------------------------------------------

std::string legend_text(const Isoband& theIsoband, const LegendLabels& theLabels)
{
  try
  {
    static std::string empty;

    if (theLabels.type == "none")
      return empty;
    else if (theLabels.type == "lolimit")
    {
      if (!theIsoband.lolimit)
        return empty;
      return legend_text(*theIsoband.lolimit, theLabels);
    }
    else if (theLabels.type == "hilimit")
    {
      if (!theIsoband.hilimit)
        return empty;
      return legend_text(*theIsoband.hilimit, theLabels);
    }
    else if (theLabels.type == "range")
    {
      if (!theIsoband.lolimit)
      {
        if (!theIsoband.hilimit)
          return "MISSING";
        return "&lt; " + legend_text(*theIsoband.hilimit, theLabels);
      }
      else
      {
        if (!theIsoband.hilimit)
          return "&gt; " + legend_text(*theIsoband.lolimit, theLabels);
        return (legend_text(*theIsoband.lolimit, theLabels) + theLabels.separator +
                legend_text(*theIsoband.hilimit, theLabels));
      }
    }
    else
      throw Spine::Exception(BCP, "Unknown legend label type '" + theLabels.type + "'");
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the text for the layer, apply language if necessary
 */
// ----------------------------------------------------------------------

std::string legend_text(const Isoband& theIsoband,
                        const LegendLabels& theLabels,
                        boost::optional<std::string>& theLanguage)
{
  try
  {
    std::string text = legend_text(theIsoband, theLabels);
    if (text.empty())
      return text;

    // Try converting with a language
    if (theLanguage)
    {
      const auto pos = theLabels.conversions.find(*theLanguage + ":" + text);
      if (pos != theLabels.conversions.end())
        return pos->second;
    }

    // Try converting without a language
    const auto pos = theLabels.conversions.find(text);
    if (pos != theLabels.conversions.end())
      return pos->second;

    return text;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void LegendLayer::init(const Json::Value& theJson,
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

    auto json = theJson.get("x", nulljson);
    if (!json.isNull())
      x = json.asInt();

    json = theJson.get("y", nulljson);
    if (!json.isNull())
      y = json.asInt();

    json = theJson.get("dx", nulljson);
    if (!json.isNull())
      dx = json.asInt();

    json = theJson.get("dy", nulljson);
    if (!json.isNull())
      dy = json.asInt();

    json = theJson.get("symbols", nulljson);
    if (!json.isNull())
      symbols.init(json, theConfig);

    json = theJson.get("labels", nulljson);
    if (!json.isNull())
      labels.init(json, theConfig);

    json = theJson.get("isobands", nulljson);
    if (!json.isNull())
      Spine::JSON::extract_array("isobands", isobands, json, theConfig);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Generate the layer details into the template hash
 *
 * Generated content is something like this:
 *
 *     <g attributes..>
 *      <use xlink:href="#symbol" x="x" y="y" class="foo"/>
 *      <text x="x" y="y" class="Label">0.1 - 0.2</text>
 *      ...
 *     </g>
 *
 * Note that we do not use a layer/tag system since at this moment
 * cdata is not supported for tags.
 */
// ----------------------------------------------------------------------

void LegendLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (!validLayer(theState))
      return;

    // Time execution

    std::string report = "LegendLayer::generate finished in %t sec CPU, %w sec real\n";
    boost::movelib::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
      timer = boost::movelib::make_unique<boost::timer::auto_cpu_timer>(2, report);

    // A symbol must be defined

    if (!symbols.symbol)
      throw Spine::Exception(BCP, "Must define default symbol with 'symbol' in a legend-layer");

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }
    if (symbols.css)
    {
      std::string name = theState.getCustomer() + "/" + *symbols.css;
      theGlobals["css"][name] = theState.getStyle(*symbols.css);
    }

    // Begin a G-group and add style to it

    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";  // we will terminate the last layer separately

    // Add layer attributes to the legend, symbols and labels have their own attributes
    theState.addAttributes(theGlobals, group_cdt, attributes);

    theLayersCdt.PushBack(group_cdt);

    // Process all the isobands

    int xpos = x;
    int ypos = y;

    for (std::size_t num = 0; num < isobands.size(); ++num)
    {
      const Isoband& isoband = isobands[num];

      // Skip missing value symbol if not explicitly given a symbol

      if (!isoband.lolimit && !isoband.hilimit && !symbols.missing)
        continue;

      // First the symbol

      {
        CTPP::CDT inner_group_cdt(CTPP::CDT::HASH_VAL);
        inner_group_cdt["start"] = "";
        inner_group_cdt["end"] = "";

        CTPP::CDT symbol_cdt(CTPP::CDT::HASH_VAL);
        symbol_cdt["start"] = "<use";
        symbol_cdt["end"] = "/>";

        theState.addAttributes(theGlobals, symbol_cdt, isoband.attributes);
        theState.addAttributes(theGlobals, symbol_cdt, symbols.attributes);

        symbol_cdt["attributes"]["x"] = Fmi::to_string(xpos);
        symbol_cdt["attributes"]["y"] = Fmi::to_string(ypos);

        std::string iri;
        if (num == 0 && symbols.start)
          iri = *symbols.start;
        else if (num == isobands.size() - 1 && symbols.end)
          iri = *symbols.end;
        else
          iri = *symbols.symbol;

        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        symbol_cdt["attributes"]["xlink:href"] = "#" + IRI;

        inner_group_cdt["tags"].PushBack(symbol_cdt);

        theLayersCdt.PushBack(inner_group_cdt);
      }

      // Then the text

      std::string text = legend_text(isoband, labels, language);

      if (!text.empty())
      {
        CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
        text_cdt["start"] = "<text";
        text_cdt["end"] = "</text>";
        text_cdt["cdata"] = text;

        theState.addAttributes(theGlobals, text_cdt, labels.attributes);

        text_cdt["attributes"]["x"] = Fmi::to_string(xpos + labels.dx);
        text_cdt["attributes"]["y"] = Fmi::to_string(ypos + labels.dy);
        theLayersCdt.PushBack(text_cdt);
      }

      // Update position

      xpos += dx;
      ypos += dy;
    }

    // Expand inner layers
    layers.generate(theGlobals, theLayersCdt, theState);

    // Close the grouping
    theLayersCdt[theLayersCdt.Size() - 1]["end"].Concat("\n  </g>");
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

std::size_t LegendLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    boost::hash_combine(hash, Dali::hash_value(x));
    boost::hash_combine(hash, Dali::hash_value(y));
    boost::hash_combine(hash, Dali::hash_value(dx));
    boost::hash_combine(hash, Dali::hash_value(dy));
    boost::hash_combine(hash, Dali::hash_value(symbols, theState));
    boost::hash_combine(hash, Dali::hash_value(labels, theState));
    boost::hash_combine(hash, Dali::hash_value(isobands, theState));
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
