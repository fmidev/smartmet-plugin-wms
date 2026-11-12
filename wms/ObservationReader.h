#pragma once

#ifndef WITHOUT_OBSERVATIONS

#include <macgyver/DateTime.h>
#include <string>
#include <vector>

namespace SmartMet::Plugin::Dali
{
struct ObservationSources;
}  // namespace SmartMet::Plugin::Dali

namespace Fmi
{
class Box;
class SpatialReference;
}  // namespace Fmi

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Layer;
class PointData;
class Positions;
class State;

using PointValues = std::vector<PointData>;

namespace ObservationReader
{
PointValues read(const State& state,
                 const std::vector<std::string>& parameters,
                 const Layer& layer,
                 const Positions& positions,
                 double maxdistance,
                 const Fmi::SpatialReference& crs,
                 const Fmi::Box& box,
                 const Fmi::DateTime& valid_time,
                 const Fmi::TimePeriod& valid_time_period);

// Read from the given producer instead of the producer of the layer
PointValues read(const State& state,
                 const std::vector<std::string>& parameters,
                 const Layer& layer,
                 const std::string& producer,
                 const Positions& positions,
                 double maxdistance,
                 const Fmi::SpatialReference& crs,
                 const Fmi::Box& box,
                 const Fmi::DateTime& valid_time,
                 const Fmi::TimePeriod& valid_time_period);

// Read and merge several sources, the extra parameters follow the source value
PointValues read(const State& state,
                 const ObservationSources& sources,
                 const std::vector<std::string>& extra_parameters,
                 const Layer& layer,
                 const Positions& positions,
                 double maxdistance,
                 const Fmi::SpatialReference& crs,
                 const Fmi::Box& box,
                 const Fmi::DateTime& valid_time,
                 const Fmi::TimePeriod& valid_time_period);

}
}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet

#endif
