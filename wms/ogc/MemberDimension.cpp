#include "MemberDimension.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
MemberDimension::MemberDimension(const std::set<int>& members) : itsMembers(members)
{
  if (itsMembers.empty())
    throw Fmi::Exception(BCP, "Member dimension requires at least one member");

  // An explicit list instead of a min/max/step range so that clients can
  // build a menu directly from the values
  for (auto member : itsMembers)
  {
    if (!itsCapabilities.empty())
      itsCapabilities += ',';
    itsCapabilities += Fmi::to_string(member);
  }
}

bool MemberDimension::isValidMember(int member) const
{
  return itsMembers.find(member) != itsMembers.end();
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
