#include "ObservationSources.h"
#include "Hash.h"
#include "JsonTools.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
ObservationSource parse_source(Json::Value& theJson)
{
  ObservationSource source;

  if (theJson.isString())
  {
    source.parameter = theJson.asString();
    return source;
  }

  if (!theJson.isObject())
    throw Fmi::Exception(BCP, "Parameter list elements must be names or objects");

  JsonTools::remove_string(source.parameter, theJson, "parameter");
  JsonTools::remove_string(source.producer, theJson, "producer");
  JsonTools::remove_string(source.keyword, theJson, "keyword");
  JsonTools::remove_double(source.multiplier, theJson, "multiplier");
  JsonTools::remove_double(source.offset, theJson, "offset");

  if (source.parameter.empty())
    throw Fmi::Exception(BCP, "Parameter name missing from parameter list element");

  if (!theJson.empty())
    throw Fmi::Exception(BCP, "Unknown settings in parameter list element")
        .addParameter("settings", theJson.toStyledString());

  return source;
}
}  // namespace

std::size_t ObservationSource::hash_value() const
{
  try
  {
    auto hash = Fmi::hash_value(parameter);
    Fmi::hash_combine(hash, Fmi::hash_value(producer));
    Fmi::hash_combine(hash, Fmi::hash_value(keyword));
    Fmi::hash_combine(hash, Fmi::hash_value(multiplier));
    Fmi::hash_combine(hash, Fmi::hash_value(offset));
    return hash;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

void ObservationSources::init(Json::Value& theJson)
{
  try
  {
    if (!theJson.isObject())
      return;

    const auto& value = theJson["parameter"];
    if (!value.isArray())
      return;

    auto json = JsonTools::remove(theJson, "parameter");

    if (json.empty())
      throw Fmi::Exception(BCP, "Empty parameter list");

    sources.clear();
    for (auto& item : json)
      sources.push_back(parse_source(item));
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse the parameter list");
  }
}

std::size_t ObservationSources::hash_value() const
{
  try
  {
    std::size_t hash = 0;
    for (const auto& source : sources)
      Fmi::hash_combine(hash, source.hash_value());
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
