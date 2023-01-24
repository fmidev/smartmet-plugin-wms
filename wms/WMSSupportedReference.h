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
  WMSSupportedReference(std::string p, const Engine::Gis::BBox& b, bool e, bool g)
      : proj(std::move(p)), bbox(b), enabled(e), geographic(g)
  {
  }

  std::string proj;
  Engine::Gis::BBox bbox{};
  bool enabled = true;
  bool geographic = false;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
