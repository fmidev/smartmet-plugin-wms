// ======================================================================
/*!
 * \brief A Web Map Service ensemble member dimension data structure
 *
 * Lists the perturbed ensemble members of a grid layer. The control
 * member is not selectable through the dimension.
 */
// ======================================================================

#pragma once

#include <set>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
// Forecast type of perturbed ensemble members in the grid content (radon convention)
constexpr int PerturbedForecastType = 3;

class MemberDimension
{
 public:
  explicit MemberDimension(const std::set<int>& members);

  bool isValidMember(int member) const;
  int getDefaultMember() const { return *itsMembers.begin(); }
  const std::string& getCapabilities() const { return itsCapabilities; }

 private:
  std::set<int> itsMembers;
  std::string itsCapabilities;
};

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
