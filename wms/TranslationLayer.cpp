#include "TranslationLayer.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Layer.h"
#include "State.h"

#include <ctpp2/CDT.hpp>
#include <macgyver/Exception.h>

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

void TranslationLayer::init(Json::Value& theJson,
                            const State& theState,
                            const Config& theConfig,
                            const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Translation-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    // Extract member values

    JsonTools::remove_string(tag, theJson, "tag");

    auto json = JsonTools::remove(theJson, "translations");
    if (!json.isNull())
    {
      if (!json.isObject())
        throw Fmi::Exception(BCP, "translations setting must be a JSON map");

      const auto members = json.getMemberNames();
      for (const auto& name : members)
      {
        const Json::Value& text_json = json[name];
        if (!text_json.isString())
          throw Fmi::Exception(BCP,
                               "Translation-layer name '" + name + "' translation is not a string");
        translations[name] = text_json.asString();
      }
    }
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

void TranslationLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
    if (tag.empty())
      throw Fmi::Exception(BCP, "TranslationLayer tag must be defined and be non-empty");

    if (!language || language->empty())
      throw Fmi::Exception(BCP, "Unable to generate translation-layer, language code is not set");

    if (!validLayer(theState))
      return;

    // Find the translation

    const auto& translation = translations.find(*language);
    if (translation == translations.end())
      throw Fmi::Exception(BCP, "Translation missing for language '" + *language + "'");

    // Update the globals

    if (css)
    {
      std::string name = theState.getCustomer() + "/" + *css;
      theGlobals["css"][name] = theState.getStyle(*css);
    }

    // Generate the layer with the proper cdata

    CTPP::CDT text_cdt(CTPP::CDT::HASH_VAL);
    text_cdt["start"] = "<" + tag;
    text_cdt["end"] = "</" + tag + ">";
    text_cdt["attributes"] = CTPP::CDT(CTPP::CDT::HASH_VAL);
    theState.addAttributes(theGlobals, text_cdt, attributes);
    text_cdt["cdata"] = Fmi::safexmlescape(translation->second);
    theLayersCdt.PushBack(text_cdt);
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

std::size_t TranslationLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);
    Fmi::hash_combine(hash, Fmi::hash_value(tag));
    Fmi::hash_combine(hash, Fmi::hash_value(translations));
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
