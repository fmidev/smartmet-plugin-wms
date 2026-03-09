#include "ParameterInfo.h"
#include "Hash.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
std::size_t ParameterInfo::hash_value() const
{
  auto hash = Fmi::hash_value(parameter);
  Fmi::hash_combine(hash, Fmi::hash_value(source));
  Fmi::hash_combine(hash, Fmi::hash_value(producer));
  Fmi::hash_combine(hash, Fmi::hash_value(forecastType));
  Fmi::hash_combine(hash, Fmi::hash_value(forecastNumber));
  Fmi::hash_combine(hash, Fmi::hash_value(geometryId));
  Fmi::hash_combine(hash, Fmi::hash_value(levelId));
  Fmi::hash_combine(hash, Fmi::hash_value(level));
  Fmi::hash_combine(hash, Fmi::hash_value(pressure));
  Fmi::hash_combine(hash, Fmi::hash_value(elevation_unit));
  return hash;
}

std::string ParameterInfo::to_string() const
{
  std::string ret = "parameter= " + parameter;
  ret += " source=";
  if (source)
    ret += *source;
  ret += " producer=";
  if (producer)
    ret += *producer;
  ret += " forecastType=";
  if (forecastType)
    ret += Fmi::to_string(*forecastType);
  ret += " forecastNumber=";
  if (forecastNumber)
    ret += Fmi::to_string(*forecastNumber);
  ret += " geometryId=";
  if (geometryId)
    ret += Fmi::to_string(*geometryId);
  ret += " levelId=";
  if (levelId)
    ret += Fmi::to_string(*levelId);
  ret += " level=";
  if (level)
    ret += Fmi::to_string(*level);
  ret += " pressure=";
  if (pressure)
    ret += Fmi::to_string(*pressure);
  ret += " elevation_unit=";
  if (elevation_unit)
    ret += *elevation_unit;
  return ret;
}

bool operator==(const ParameterInfo& first, const ParameterInfo& second)
{
  // clang-format off
  return (first.parameter == second.parameter &&
          first.source == second.source &&
          first.producer == second.producer &&
          first.forecastType == second.forecastType &&
          first.forecastNumber == second.forecastNumber &&
          first.geometryId == second.geometryId &&
          first.levelId == second.levelId &&
          first.level == second.level &&
          first.pressure == second.pressure &&
          first.elevation_unit == second.elevation_unit);
  // clang-format on
}

// Add to end of the list unless its a duplicate
bool add(ParameterInfos& infos, const ParameterInfo& info)
{
  auto it = find(infos.begin(), infos.end(), info);
  if (it != infos.end())
    return false;
  infos.push_back(info);
  return true;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
