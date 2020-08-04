#pragma once

#include <engines/gis/BBox.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
// A WMS reference
struct WMSSupportedReference
{
  WMSSupportedReference(const std::string& p, const Engine::Gis::BBox b, bool e = true)
      : proj(p), bbox(b), enabled(e)
  {
  }

  std::string proj{};
  Engine::Gis::BBox bbox{};
  bool enabled{true};
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
