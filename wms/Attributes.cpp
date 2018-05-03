// ======================================================================

#include "Attributes.h"
#include "Config.h"
#include "Hash.h"
#include "State.h"
#include "StyleSheet.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <ctpp2/CDT.hpp>
#include <spine/Exception.h>
#include <spine/HTTP.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
// ----------------------------------------------------------------------
/*!
 * \brief Add an attribute
 */
// ----------------------------------------------------------------------

void Attributes::add(const std::string& theName, const std::string& theValue)
{
  attributes[theName] = theValue;
}

// ----------------------------------------------------------------------
/*!
 * \brief Initialize attributes from JSON
 *
 * Note: We allow JSON values to be integers and doubles, but
 *       convert them directly to strings since that is how
 *       we are going to represent them when printing SVG
 *       attributes. This feature allows the user to put
 *       attributes in a natural form into the attributes without
 *       bothering with unnecessary quotes.
 */
// ----------------------------------------------------------------------

void Attributes::init(const Json::Value& theJson, const Config& theConfig)
{
  try
  {
    // Allowed since it makes JSON self documenting
    if (theJson.isNull())
      return;

    if (!theJson.isObject())
      throw Spine::Exception(BCP, "Attributes JSON is not a JSON object (name-value pairs)");

    // Iterate trhough all the members

    const auto members = theJson.getMemberNames();
    BOOST_FOREACH (const auto& name, members)
    {
      const Json::Value& json = theJson[name];

      switch (json.type())
      {
        case Json::nullValue:
          break;
        case Json::intValue:
        {
          attributes[name] = Fmi::to_string(json.asInt());
          break;
        }
        case Json::uintValue:
        {
          attributes[name] = Fmi::to_string(json.asUInt());
          break;
        }
        case Json::realValue:
        {
          attributes[name] = Fmi::to_string(json.asDouble());
          break;
        }
        case Json::stringValue:
        case Json::booleanValue:
        {
          attributes[name] = json.asString();
          break;
        }
        case Json::arrayValue:
        {
          throw Spine::Exception(BCP, "Arrays are not allowed as an Attribute value");
        }
        case Json::objectValue:
        {
          throw Spine::Exception(BCP, "Maps are not allowed as an Attribute value");
        }
      }
    }
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the attributes to the CDT
 *
 * Note: Both librsvg and WebKit fail to obey presentation attributes
 *       if a class-attribute is used. Both do work if both class
 *       and style is used. Hence we collect all presentation attributes
 *       into a single style so that a class can be defined simultaneously.
 *
 * Note: We disallow using the style attribute directly since we wish
 *       to reserve the keyword for selecting the product style.
 *       Hence we know the attributes should not contain a style tag.
 *
 * Note: We allow qid in the input, but disable it in the output
 */
// ----------------------------------------------------------------------

void Attributes::generate(CTPP::CDT& theLocals, State& theState) const
{
  try
  {
    const auto& regular_attributes = theState.getConfig().regularAttributes();
    const auto& presentation_attributes = theState.getConfig().presentationAttributes();

    CTPP::CDT attrs = CTPP::CDT(CTPP::CDT::HASH_VAL);

    // Collect presentation attributes into a single style attribute
    std::map<std::string, std::string> style;

    BOOST_FOREACH (const auto& attribute, attributes)
    {
      const auto& attr_name = attribute.first;
      const auto& attr_value = attribute.second;

      // Validate attribute name

      bool is_regular = (regular_attributes.find(attr_name) != regular_attributes.end());
      bool is_presentation = (!is_regular && (presentation_attributes.find(attr_name) !=
                                              presentation_attributes.end()));
      bool is_valid = ((attr_name == "qid") | is_presentation | is_regular);

      if (!is_valid)
        throw Spine::Exception(BCP, "Illegal SVG attribute name '" + attr_name + "'");

      // Make sure the ID is unique
      if (attr_name == "id")
        theState.requireId(attr_value);

      // Handle the attribute

      if (is_regular)
        attrs[attr_name] = attr_value;
      else if (is_presentation)
        style[attr_name] = attr_value;
    }

    // Now add a style attribute if necessary

    if (!style.empty())
    {
      std::string text;
      BOOST_FOREACH (const auto& name_value, style)
      {
        if (!text.empty())
          text += "; ";
        text += name_value.first + ':' + name_value.second;
      }
      attrs["style"] = text;
    }

    // There may be pre-existing attributes, so we merge instead of assigning
    theLocals["attributes"].MergeCDT(attrs);
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Add the presentation attributes to the CDT, including picking them from the CSS
 */
// ----------------------------------------------------------------------

void Attributes::generatePresentation(CTPP::CDT& theLocals,
                                      State& theState,
                                      const std::string& theCSS) const
{
  try
  {
    // Collect presentation attributes into a single map
    std::map<std::string, std::string> style;

    // Extract CSS class definitions first
    const auto classiter = attributes.find("class");
    if (classiter != attributes.end())
    {
      auto selector = "." + classiter->second;
      StyleSheet ss;
      ss.add(theCSS);
      style = ss.declarations(selector);
    }

    // Override with element specific attributes

    const auto& regular_attributes = theState.getConfig().regularAttributes();
    const auto& presentation_attributes = theState.getConfig().presentationAttributes();

    for (const auto& attribute : attributes)
    {
      const auto& attr_name = attribute.first;
      const auto& attr_value = attribute.second;

      // Validate attribute name

      bool is_regular = (regular_attributes.find(attr_name) != regular_attributes.end());
      bool is_presentation = (!is_regular && (presentation_attributes.find(attr_name) !=
                                              presentation_attributes.end()));
      bool is_valid = ((attr_name == "qid") | is_presentation | is_regular);

      if (!is_valid)
        throw Spine::Exception(BCP, "Illegal SVG attribute name '" + attr_name + "'");

      // Handle the attribute

      style[attr_name] = attr_value;
    }

    // And output final set of styles

    for (const auto& name_value : style)
      theLocals["presentation"][name_value.first] = name_value.second;
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

std::size_t Attributes::hash_value(const State& theState) const
{
  try
  {
    std::size_t hash = Dali::hash_value(attributes);

    // Special attributes. See also State::addAttributes

    auto iri = getLocalIri("filter");
    if (iri)
      boost::hash_combine(hash, theState.getFilterHash(*iri));

    iri = getLocalIri("marker");
    if (iri)
      boost::hash_combine(hash, theState.getMarkerHash(*iri));

    iri = getLocalIri("marker-start");
    if (iri)
      boost::hash_combine(hash, theState.getMarkerHash(*iri));

    iri = getLocalIri("marker-mid");
    if (iri)
      boost::hash_combine(hash, theState.getMarkerHash(*iri));

    iri = getLocalIri("marker-end");
    if (iri)
      boost::hash_combine(hash, theState.getMarkerHash(*iri));

    iri = getLocalIri("fill");
    if (iri)
      boost::hash_combine(hash, theState.getPatternHash(*iri));

    iri = getLocalIri("linearGradient");
    if (iri)
      boost::hash_combine(hash, theState.getGradientHash(*iri));

    iri = getLocalIri("radialGradient");
    if (iri)
      boost::hash_combine(hash, theState.getGradientHash(*iri));

    return hash;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Extract named attribute local IRI if there is one
 */
// ----------------------------------------------------------------------

boost::optional<std::string> Attributes::getLocalIri(const std::string& theName) const
{
  try
  {
    boost::optional<std::string> ret;

    // return empty value if there is no such attribute

    const auto& att = attributes.find(theName);
    if (att == attributes.end())
    {
      return ret;
    }

    // return empty value if the value does not look like a local IRI

    const auto& value = att->second;
    if (value.empty() || value.substr(0, 5) != "url(#" || value[value.size() - 1] != ')')
    {
      return ret;
    }

    // Return the local name
    ret = value.substr(5, value.size() - 6);
    return ret;
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Remove and return an attribute
 */
// ----------------------------------------------------------------------

boost::optional<std::string> Attributes::remove(const std::string& theName)
{
  auto pos = attributes.find(theName);
  if (pos == attributes.end())
    return {};

  std::string value = pos->second;
  attributes.erase(pos);
  return value;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
