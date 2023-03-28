#pragma once
#include <list>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Verification
{
 public:
  std::string report(const std::string& theFormat) const;
  void warning(std::string& theMessage);
  void error(std::string& theMessage);

 private:
  std::list<std::string> warnings;
  std::list<std::string> errors;
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
