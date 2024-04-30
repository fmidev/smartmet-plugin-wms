#include "IsolineFilter.h"
#include "JsonTools.h"
#include <fmt/core.h>
#include <gis/Box.h>
#include <gis/GeometrySmoother.h>
#include <macgyver/Exception.h>
#include <ogr_geometry.h>
#include <string>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
namespace
{

// Parse name given in config file into an enum type for the filter

Fmi::GeometrySmoother::Type parse_filter_name(const std::string& name)
{
  try
  {
    if (name == "none")
      return Fmi::GeometrySmoother::Type::None;
    if (name == "average")
      return Fmi::GeometrySmoother::Type::Average;
    if (name == "linear")
      return Fmi::GeometrySmoother::Type::Linear;
    if (name == "gaussian")
      return Fmi::GeometrySmoother::Type::Gaussian;
    if (name == "tukey")
      return Fmi::GeometrySmoother::Type::Tukey;
    throw Fmi::Exception(BCP, "Unknown isoline filter type '" + name + "'");
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

}  // namespace

// Initialize from JSON config
void IsolineFilter::init(Json::Value& theJson)
{
  try
  {
    if (theJson.isNull())
      return;
    if (!theJson.isObject())
      throw Fmi::Exception(BCP, "Isoline filter settings must be in a JSON object");

    Fmi::GeometrySmoother::Type type = Fmi::GeometrySmoother::Type::None;
    double radius = 0.0;
    uint iterations = 1;

    JsonTools::remove_double(radius, theJson, "radius");
    JsonTools::remove_uint(iterations, theJson, "iterations");

    std::string stype;
    JsonTools::remove_string(stype, theJson, "type");
    if (!stype.empty())
      type = parse_filter_name(stype);

    // Sanity checks
    if (radius < 0)
      throw Fmi::Exception(BCP, "Isoline filter radius must be nonnegative");
    if (radius > 100)
      throw Fmi::Exception(BCP, "Isoline filter radius must be less than 100 pixels");
    if (iterations > 100)
      throw Fmi::Exception(BCP, "Isoline filter number of iterations must be less than 100");

    smoother.type(type);
    smoother.radius(radius);
    smoother.iterations(iterations);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// Apply smoother
void IsolineFilter::apply(std::vector<OGRGeometryPtr>& geoms, bool preserve_topology) const
{
  smoother.apply(geoms, preserve_topology);
}

// Alter the pixel radius to a metric one based on a BBOX
void IsolineFilter::bbox(const Fmi::Box& box)
{
  smoother.bbox(box);
}

// Hash value for caching purposes
std::size_t IsolineFilter::hash_value() const
{
  return smoother.hash_value();
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
