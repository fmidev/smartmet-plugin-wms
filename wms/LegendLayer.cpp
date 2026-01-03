//======================================================================

#include "LegendLayer.h"
#include "Config.h"
#include "Hash.h"
#include "Iri.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"
#include <boost/timer/timer.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <spine/Json.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
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
    return fmt::sprintf("%d", lround(num));

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
  return "0";
}

// ----------------------------------------------------------------------
/*!
 * \brief Format a legend number
 */
// ----------------------------------------------------------------------

std::string legend_number(double theValue, const LegendLabels& theLabels)
{
  try
  {
    if (!theLabels.format)
      return Fmi::to_string(theValue);

    return pretty(theValue, theLabels.format->c_str());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the text for a legend symbol
 */
// ----------------------------------------------------------------------

std::string nonconverted_isoband_text(const Isoband& theIsoband,
                                      const LegendLabels& theLabels,
                                      const std::optional<std::string>& theLanguage,
                                      const std::string& theDefaultLanguage)
{
  try
  {
    static std::string empty;

    if (theLabels.type == "none")
      return empty;

    // Isoband specific override handled first
    if (theIsoband.label)
      return theIsoband.label->translate(theLanguage, theDefaultLanguage);

    // Then generic processing
    if (theLabels.type == "lolimit")
    {
      if (!theIsoband.lolimit)
        return empty;
      return legend_number(*theIsoband.lolimit, theLabels);
    }

    if (theLabels.type == "hilimit")
    {
      if (!theIsoband.hilimit)
        return empty;
      return legend_number(*theIsoband.hilimit, theLabels);
    }

    if (theLabels.type == "range")
    {
      if (!theIsoband.lolimit)
      {
        if (!theIsoband.hilimit)
          return "MISSING";
        return "&lt; " + legend_number(*theIsoband.hilimit, theLabels);
      }
      if (!theIsoband.hilimit)
        return "&gt; " + legend_number(*theIsoband.lolimit, theLabels);
      return (legend_number(*theIsoband.lolimit, theLabels) + theLabels.separator +
              legend_number(*theIsoband.hilimit, theLabels));
    }

    throw Fmi::Exception(BCP, "Unknown legend label type '" + theLabels.type + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the text for a legend symbol
 */
// ----------------------------------------------------------------------

std::string nonconverted_isoline_text(const Isoline& theIsoline,
                                      const LegendLabels& theLabels,
                                      const std::optional<std::string>& theLanguage,
                                      const std::string& theDefaultLanguage)
{
  try
  {
    static std::string empty;

    if (theLabels.type == "none")
      return empty;

    // Isoline specific override handled first
    if (theIsoline.label)
      return theIsoline.label->translate(theLanguage, theDefaultLanguage);

    return legend_number(theIsoline.value, theLabels);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Apply possible legend text conversions
 */
// ----------------------------------------------------------------------

std::string apply_text_conversions(const std::string& theText,
                                   const LegendLabels& theLabels,
                                   const std::optional<std::string>& theLanguage)
{
  try
  {
    if (theText.empty())
      return theText;

    // Try converting with a language
    if (theLanguage)
    {
      const auto pos = theLabels.conversions.find(*theLanguage + ":" + theText);
      if (pos != theLabels.conversions.end())
        return Fmi::safexmlescape(pos->second);
    }

    // Try converting without a language
    const auto pos = theLabels.conversions.find(theText);
    if (pos != theLabels.conversions.end())
      return Fmi::safexmlescape(pos->second);

    return Fmi::safexmlescape(theText);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the text for the isoband, apply language if necessary
 */
// ----------------------------------------------------------------------

std::string legend_text(const Isoband& theIsoband,
                        const LegendLabels& theLabels,
                        const std::optional<std::string>& theLanguage,
                        const std::string& theDefaultLanguage)
{
  try
  {
    // Note: The text may actually already be translated, if there are isoband
    // specific translations in the Isoband object itself.

    std::string text =
        nonconverted_isoband_text(theIsoband, theLabels, theLanguage, theDefaultLanguage);

    return apply_text_conversions(text, theLabels, theLanguage);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Select the text for the isoline, apply language if necessary
 */
// ----------------------------------------------------------------------

std::string legend_text(const Isoline& theIsoline,
                        const LegendLabels& theLabels,
                        const std::optional<std::string>& theLanguage,
                        const std::string& theDefaultLanguage)
{
  try
  {
    // Note: The text may actually already be translated, if there are isoband
    // specific translations in the Isoband object itself.

    std::string text =
        nonconverted_isoline_text(theIsoline, theLabels, theLanguage, theDefaultLanguage);

    return apply_text_conversions(text, theLabels, theLanguage);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::string nonconverted_symbol_text(const AttributeSelection& theAttrSel,
                                     const LegendLabels& theLabels,
                                     const std::optional<std::string>& theLanguage)
{
  const auto& separator = theLabels.separator;

  // If translation found return it
  if (theLanguage && theAttrSel.translations.find(*theLanguage) != theAttrSel.translations.end())
    return theAttrSel.translations.at(*theLanguage);

  if (theAttrSel.value)
    return Fmi::to_string(*theAttrSel.value);
  if (theAttrSel.lolimit && theAttrSel.hilimit)
    return (Fmi::to_string(*theAttrSel.lolimit) + separator + Fmi::to_string(*theAttrSel.hilimit));
  if (theAttrSel.lolimit)
    return (Fmi::to_string(*theAttrSel.lolimit) + separator);
  if (theAttrSel.hilimit)
    return (separator + Fmi::to_string(*theAttrSel.hilimit));

  return {};
}

std::string symbol_text(const AttributeSelection& theAttrSel,
                        const LegendLabels& theLabels,
                        const std::optional<std::string>& theLanguage)
{
  auto text = nonconverted_symbol_text(theAttrSel, theLabels, theLanguage);
  return apply_text_conversions(text, theLabels, theLanguage);
}

}  // namespace

// ----------------------------------------------------------------------
/*!
 * \brief Initialize from JSON
 */
// ----------------------------------------------------------------------

void LegendLayer::init(Json::Value& theJson,
                       const State& theState,
                       const Config& theConfig,
                       const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Symbol-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_int(x, theJson, "x");
    JsonTools::remove_int(y, theJson, "y");
    JsonTools::remove_int(dx, theJson, "dx");
    JsonTools::remove_int(dy, theJson, "dy");

    auto json = JsonTools::remove(theJson, "symbols");

    if (json.isArray())
      JsonTools::extract_array("symbols", symbol_vector, json, theConfig);
    else
      symbols.init(json, theConfig);

    json = JsonTools::remove(theJson, "labels");
    labels.init(json, theConfig);

    json = JsonTools::remove(theJson, "isobands");
    if (!json.isNull())
      JsonTools::extract_array("isobands", isobands, json, theConfig);

    json = JsonTools::remove(theJson, "isolines");
    if (!json.isNull())
      JsonTools::extract_array("isolines", isolines, json, theConfig);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void LegendLayer::generate_from_symbol_vector(CTPP::CDT& theGlobals,
                                              CTPP::CDT& theLayersCdt,
                                              State& theState)
{
  try
  {
    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Begin a G-group and add style to itself
    CTPP::CDT group_cdt(CTPP::CDT::HASH_VAL);
    group_cdt["start"] = "<g";
    group_cdt["end"] = "";  // we will terminate the last layer separately

    // Add layer attributes to the legend, symbols and labels have their own attributes
    theState.addAttributes(theGlobals, group_cdt, attributes);

    theLayersCdt.PushBack(group_cdt);

    int xpos = x;
    int ypos = y;

    for (const auto& symbol : symbol_vector)
    {
      CTPP::CDT inner_group_cdt(CTPP::CDT::HASH_VAL);
      inner_group_cdt["start"] = "";
      inner_group_cdt["end"] = "";

      CTPP::CDT symbol_cdt(CTPP::CDT::HASH_VAL);
      symbol_cdt["start"] = "<use";
      symbol_cdt["end"] = "/>";

      if (!symbol.symbol)
        throw Fmi::Exception(BCP, "Array of symbols must define a symbol for each array member");

      theState.addAttributes(theGlobals, symbol_cdt, symbols.attributes);

      std::string iri = *symbol.symbol;
      std::string IRI = Iri::normalize(iri);
      if (theState.addId(IRI))
        theGlobals["includes"][iri] = theState.getSymbol(iri);
      symbol_cdt["attributes"]["xlink:href"] = "#" + IRI;
      symbol_cdt["attributes"]["x"] = Fmi::to_string(xpos);
      symbol_cdt["attributes"]["y"] = Fmi::to_string(ypos);

      inner_group_cdt["tags"].PushBack(symbol_cdt);
      theLayersCdt.PushBack(inner_group_cdt);

      auto text = symbol_text(symbol, labels, language);
      if (!text.empty())
      {
        CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
        text_cdt["start"] = "<text";
        text_cdt["end"] = "</text>";
        text_cdt["cdata"] = text;

        auto attrs = labels.attributes;
        theState.addAttributes(theGlobals, text_cdt, attrs);

        text_cdt["attributes"]["x"] = Fmi::to_string(xpos + labels.dx);
        text_cdt["attributes"]["y"] = Fmi::to_string(ypos + labels.dy);
        theLayersCdt.PushBack(text_cdt);
      }
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
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
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

    std::unique_ptr<boost::timer::auto_cpu_timer> timer;
    if (theState.useTimer())
    {
      std::string report = "LegendLayer::generate finished in %t sec CPU, %w sec real\n";
      timer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
    }

    if (!symbol_vector.empty())
      return generate_from_symbol_vector(theGlobals, theLayersCdt, theState);

    // A symbol must be defined

    if (!symbols.symbol)
      throw Fmi::Exception(BCP, "Must define default symbol with 'symbol' in a legend-layer");

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

      std::string text =
          legend_text(isoband, labels, language, theState.getConfig().defaultLanguage());

      if (!text.empty())
      {
        CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
        text_cdt["start"] = "<text";
        text_cdt["end"] = "</text>";
        text_cdt["cdata"] = text;

        auto attrs = labels.attributes;
        if (isoband.label)
          attrs.add(isoband.label->attributes);  // isoband specific attrs override

        theState.addAttributes(theGlobals, text_cdt, attrs);

        text_cdt["attributes"]["x"] = Fmi::to_string(xpos + labels.dx);
        text_cdt["attributes"]["y"] = Fmi::to_string(ypos + labels.dy);
        theLayersCdt.PushBack(text_cdt);
      }

      // Update position

      xpos += dx;
      ypos += dy;
    }

    // Process all isolines

    for (const auto& isoline : isolines)
    {
      // First the symbol

      {
        CTPP::CDT inner_group_cdt(CTPP::CDT::HASH_VAL);
        inner_group_cdt["start"] = "";
        inner_group_cdt["end"] = "";

        CTPP::CDT symbol_cdt(CTPP::CDT::HASH_VAL);
        symbol_cdt["start"] = "<use";
        symbol_cdt["end"] = "/>";

        theState.addAttributes(theGlobals, symbol_cdt, isoline.attributes);
        theState.addAttributes(theGlobals, symbol_cdt, symbols.attributes);

        symbol_cdt["attributes"]["x"] = Fmi::to_string(xpos);
        symbol_cdt["attributes"]["y"] = Fmi::to_string(ypos);

        std::string iri = *symbols.symbol;

        std::string IRI = Iri::normalize(iri);
        if (theState.addId(IRI))
          theGlobals["includes"][iri] = theState.getSymbol(iri);

        symbol_cdt["attributes"]["xlink:href"] = "#" + IRI;

        inner_group_cdt["tags"].PushBack(symbol_cdt);

        theLayersCdt.PushBack(inner_group_cdt);
      }

      // Then the text

      std::string text =
          legend_text(isoline, labels, language, theState.getConfig().defaultLanguage());

      if (!text.empty())
      {
        CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
        text_cdt["start"] = "<text";
        text_cdt["end"] = "</text>";
        text_cdt["cdata"] = text;

        auto attrs = labels.attributes;
        if (isoline.label)
          attrs.add(isoline.label->attributes);  // isoline specific attrs override

        theState.addAttributes(theGlobals, text_cdt, attrs);

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
    throw Fmi::Exception::Trace(BCP, "Operation failed!").addParameter("qid", qid);
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
    Fmi::hash_combine(hash, Fmi::hash_value(x));
    Fmi::hash_combine(hash, Fmi::hash_value(y));
    Fmi::hash_combine(hash, Fmi::hash_value(dx));
    Fmi::hash_combine(hash, Fmi::hash_value(dy));
    Fmi::hash_combine(hash, Dali::hash_value(symbols, theState));
    Fmi::hash_combine(hash, Dali::hash_value(labels, theState));
    Fmi::hash_combine(hash, Dali::hash_value(isobands, theState));
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
