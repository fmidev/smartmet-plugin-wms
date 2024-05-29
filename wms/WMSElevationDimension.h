// ======================================================================
/*!
 * \brief A Web Map Service elevation dimension data structure
 *
 * Characteristics:
 *
 */
// ======================================================================

#pragma once

#include <newbase/NFmiLevelType.h>
#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
class WMSElevationDimension
{
 public:
  WMSElevationDimension(std::string level_name,
                        FmiLevelType level_type,
                        const std::set<int>& elevations);

  WMSElevationDimension(std::string level_name,
                        short level_type,
                        std::string unit_symbol,
                        const std::set<int>& elevations,
                        int step);

  bool isValidElevation(int elevation) const;
  std::string getDefaultElevation() const;
  const std::string& getLevelName() const;
  const std::string& getUnitSymbol() const;
  const std::string& getCapabilities() const;
  bool isOK() const;
  bool isIdentical(const WMSElevationDimension& td) const;

 protected:
  std::string itsLevelName;
  short itsLevelType;
  std::set<int> itsElevations;
  std::string itsUnitSymbol;
  std::string itsCapabilities;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
