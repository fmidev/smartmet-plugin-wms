// ======================================================================
/*!
 * \brief A Web Map Service elevation dimension data structure
 *
 * Characteristics:
 *
 */
// ======================================================================

#pragma once

#include <macgyver/StringConversion.h>
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
                        const std::set<int>& elevations);

#if 0  
  WMSElevationDimension(std::string level_name,
                        short level_type,
                        std::string unit_symbol,
                        int lolimit,
                        int hilimit,
                        int step);
#endif

  bool isValidElevation(int elevation) const;
  const std::string& getDefaultElevation() const { return itsDefaultElevation; }
  void setDefaultElevation(int level) { itsDefaultElevation = Fmi::to_string(level); }
  const std::string& getLevelName() const;
  const std::string& getUnitSymbol() const;
  const std::string& getCapabilities() const;
  bool isOK() const;
  bool isIdentical(const WMSElevationDimension& td) const;

 protected:
  void initDefaultElevation();

  std::string itsLevelName;
  std::string itsDefaultElevation;
  short itsLevelType;
  std::set<int> itsElevations;
  std::string itsUnitSymbol;
  std::string itsCapabilities;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
