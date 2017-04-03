#pragma once

#include <map>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class StyleSheet
{
 public:
  using Declarations = std::map<std::string, std::string>;
  using SelectorDeclarations = std::map<std::string, Declarations>;

  void add(const std::string& theCSS);

  const Declarations& declarations(const std::string& theSelector) const;

 private:
  SelectorDeclarations itsStyleSheet;

};  // class StyleSheet
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
