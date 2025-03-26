#include "GraticuleLayer.h"
#include "Config.h"
#include "Hash.h"
#include "JsonTools.h"
#include "State.h"
#include <ctpp2/CDT.hpp>
#include <spine/Json.h>
#include <set>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
const std::set<std::string> valid_label_layouts = {
    "none", "edges", "grid", "center", "left_bottom"};
const std::set<std::string> valid_line_layouts = {"grid", "ticks"};

}  // namespace

GraticuleLayer::Graticule remove_graticule(Json::Value& theJson, const Config& theConfig)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "graticules settings must be JSON objects");

    GraticuleLayer::Graticule g;
    JsonTools::remove_string(g.layout, theJson, "layout");
    JsonTools::remove_uint(g.step, theJson, "step");
    JsonTools::remove_uint(g.length, theJson, "length");

    // exceptions to the given step
    auto json = JsonTools::remove(theJson, "except");
    if (json.isArray())
    {
      for (const auto& num : json)
      {
        if (num.isUInt())
          g.except.push_back(num.asUInt());
        else
          throw Fmi::Exception(BCP,
                               "graticule except setting must be an array of unsigned integers");
      }
    }
    else if (json.isUInt())
      g.except.push_back(json.asUInt());
    else
      throw Fmi::Exception(BCP, "graticule except setting must be an array of unsigned integers");

    // label rendering

    json = JsonTools::remove(theJson, "labels");

    if (!json.isNull())  // labels are not obligatory
    {
      if (!json.isObject())
        throw Fmi::Exception(BCP, "graticule labels setting must be a JSON object");

      auto& l = g.labels;  // shorthand
      JsonTools::remove_string(l.layout, json, "layout");
      JsonTools::remove_uint(l.step, json, "step");
      JsonTools::remove_bool(l.upright, json, "upright");
      JsonTools::remove_bool(l.degree_sign, json, "degree_sign");
      JsonTools::remove_bool(l.minus_sign, json, "minus_sign");
      JsonTools::remove_int(l.dx, json, "dx");
      JsonTools::remove_int(l.dy, json, "dy");
      l.attributes.init(json, theConfig);
    }

    // Sanity checks
    if (valid_line_layouts.find(g.layout) == valid_line_layouts.end())
      throw Fmi::Exception(BCP, "graticule layout must be grid|ticks")
          .addParameter("layout", g.layout);

    if (g.step < 1)
      throw Fmi::Exception(BCP, "graticule step must be at least 1")
          .addParameter("step", Fmi::to_string(g.step));

    if (g.layout == "ticks" && g.length < 1)
      throw Fmi::Exception(BCP, "graticule tick length must be at least 1");

    if (valid_label_layouts.find(l.layout) == valid_label_layouts.end())
      throw Fmi::Exception(BCP,
                           "graticule labels layout must be none|edges|grid|center|left_bottom")
          .addParameter("layout", l.layout);

    if (l.layout != "none")
    {
      if (l.step < 1)
        throw Fmi::Exception(BCP, "graticule label step must be at least 1")
            .addParameter("step", Fmi::to_string(l.step));
    }

    return g;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void GraticuleLayer::init(Json::Value& theJson,
                          const State& theState,
                          const Config& theConfig,
                          const Properties& theProperties)
{
  try
  {
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Gratitule-layer JSON is not a JSON object");

    Layer::init(theJson, theState, theConfig, theProperties);

    precision = theState.getPrecision("graticule");

    // Extract member values

    JsonTools::remove_string(mask, theJson, "mask");
    JsonTools::remove_string(mask_id, theJson, "mask_id");
    JsonTools::remove_double(precision, theJson, "precision");

    auto json = JsonTools::remove(theJson, "graticules");

    if (json.isNull())
      throw Fmi::Exception(BCP, "Graticule-layer must define at least one graticule");

    if (json.isObject())
      graticules.emplace_back(remove_graticule(json, theConfig));
    else if (json.isArray())
    {
      for (auto& gjson : json)
        graticules.emplace_back(remove_graticule(gjson, theConfig));
    }
    else
      throw Fmi::Exception(
          BCP,
          "graticules setting must be a single graticule definition or an array of definitions");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void GraticuleLayer::generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState)
{
  try
  {
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

std::size_t GraticuleLayer::hash_value(const State& theState) const
{
  try
  {
    auto hash = Layer::hash_value(theState);

    for (const auto& g : graticules)
    {
      Fmi::hash_combine(hash, Fmi::hash_value(g.layout));
      Fmi::hash_combine(hash, Fmi::hash_value(g.step));
      Fmi::hash_combine(hash, Fmi::hash_value(g.except));
      Fmi::hash_combine(hash, Fmi::hash_value(g.length));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.layout));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.step));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.upright));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.degree_sign));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.minus_sign));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.dx));
      Fmi::hash_combine(hash, Fmi::hash_value(g.labels.dy));
      Fmi::hash_combine(hash, g.labels.attributes.hash_value(theState));
    }
    Fmi::hash_combine(hash, Fmi::hash_value(mask));
    Fmi::hash_combine(hash, Fmi::hash_value(mask_id));
    Fmi::hash_combine(hash, Fmi::hash_value(precision));

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
