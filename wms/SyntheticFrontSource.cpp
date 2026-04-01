// ======================================================================
/*!
 * \brief SyntheticFrontSource implementation
 */
// ======================================================================

#include "SyntheticFrontSource.h"
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{

namespace
{

FrontType parseFrontType(const std::string& s)
{
  if (s == "cold") return FrontType::Cold;
  if (s == "warm") return FrontType::Warm;
  if (s == "occluded") return FrontType::Occluded;
  if (s == "stationary") return FrontType::Stationary;
  if (s == "trough") return FrontType::Trough;
  if (s == "ridge") return FrontType::Ridge;
  throw Fmi::Exception(BCP, "Unknown front type '" + s + "'");
}

FrontSide parseFrontSide(const std::string& s)
{
  if (s == "left") return FrontSide::Left;
  if (s == "right") return FrontSide::Right;
  throw Fmi::Exception(BCP, "Unknown front side '" + s + "', expected 'left' or 'right'");
}

}  // namespace

SyntheticFrontSource::SyntheticFrontSource(const Json::Value& theJson)
{
  try
  {
    const auto& frontsJson = theJson["fronts"];
    if (frontsJson.isNull() || !frontsJson.isArray())
      return;

    for (const auto& item : frontsJson)
    {
      FrontCurve curve;

      curve.type = parseFrontType(item.get("type", "cold").asString());
      curve.side = parseFrontSide(item.get("side", "left").asString());

      const auto& pts = item["points"];
      if (!pts.isArray())
        throw Fmi::Exception(BCP, "Front 'points' must be an array");

      for (const auto& pt : pts)
      {
        if (!pt.isArray() || pt.size() < 2)
          throw Fmi::Exception(BCP, "Each front point must be [lon, lat]");
        curve.points.emplace_back(pt[0].asDouble(), pt[1].asDouble());
      }

      if (curve.points.size() >= 2)
        itsCurves.push_back(std::move(curve));
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to parse synthetic front source");
  }
}

std::vector<FrontCurve> SyntheticFrontSource::getFronts(
    const Fmi::DateTime& /* validTime */) const
{
  return itsCurves;
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
