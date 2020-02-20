#include "ParameterInfo.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
bool operator==(const ParameterInfo& first, const ParameterInfo& second)
{
  return (first.parameter == second.parameter && first.producer == second.producer &&
          first.level == second.level
#if 0
         &&
         first.levelId == second.levelId &&
         first.forecastType == second.forecastType &&
         first.forecastNumber == second.forecastNumber &&
         first.geometryId == second.geometryId
#endif
  );
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
