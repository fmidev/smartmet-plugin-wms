#pragma once
#include "Verification.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{
std::string report_ascii(const std::list<std::string>& errors,
                         const std::list<std::string>& warnings)
{
  if (errors.empty() && warnings.empty())
    return {};

  std::string ret;
  if (!errors.empty())
  {
    for (const auto& msg : errors)
      ret += "Error: " + msg + "\n";
  }
  if (!warnings.empty())
  {
    for (const auto& msg : warnings)
      ret += "Warning: " + msg + "\n";
  }
  return ret;
}
}  // namespace

std::string Verification::report(const std::string& format) const
{
  if (format == "ascii")
    return report_ascii(errors, warnings);

  throw Fmi::Exception(BCP, "Unknown verification format '" + format + "'");
}

void Verification::warning(std::string& theMessage)
{
  warnings.emplace_back(std::move(theMessage));
}

void Verification::error(std::string& theMessage)
{
  errors.emplace_back(std::move(theMessage));
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
