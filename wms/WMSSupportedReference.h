#pragma once

#include <gis/BBox.h>
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
  WMSSupportedReference(std::string p, const Fmi::BBox& b, bool e, bool g)
      : proj(std::move(p)), bbox(b), enabled(e), geographic(g)
  {
  }

  std::string proj;
  Fmi::BBox bbox{};
  bool enabled = true;
  bool geographic = false;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

// ======================================================================
