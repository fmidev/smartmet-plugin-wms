// ======================================================================
/*!
 * \brief Utilities for building grid-engine queries and handling their results
 */
// ======================================================================

#pragma once

#include <gis/Types.h>
#include <grid-content/queryServer/definition/Query.h>
#include <macgyver/DateTime.h>
#include <trax/InterpolationType.h>
#include <optional>
#include <string>
#include <vector>

namespace SmartMet
{
namespace Engine
{
namespace Grid
{
class Engine;
}
}  // namespace Engine

}  // namespace SmartMet

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
struct ParameterInfo;
class Positions;
class Projection;
class Smoother;
class State;

// Add the grid-engine parameter string of the parameter with the multiplier and offset applied
// to the attributes, use it as the projection parameter if there is none yet, and add the
// producers to the query if the parameter name was used as is. Returns the parameter string.
std::string add_grid_parameter(const Engine::Grid::Engine& gridEngine,
                               const std::string& producerName,
                               const std::string& parameterName,
                               const std::optional<double>& multiplier,
                               const std::optional<double>& offset,
                               Projection& projection,
                               T::AttributeList& attributeList,
                               QueryServer::Query& query);

// Add the start, end and list of times, the time zone and the optional analysis time
void add_time_attributes(T::AttributeList& attributeList,
                         const Fmi::DateTime& validTime,
                         const std::optional<Fmi::DateTime>& origintime);

// Set the geometry, level and forecast selections of the layer parameter to a query parameter
void set_query_parameter_selections(QueryServer::QueryParameter& param,
                                    const ParameterInfo& paraminfo);

// Set the point locations of the positions to the query, or a geometry query if there are none
void set_query_locations(QueryServer::Query& query,
                         const std::optional<Positions>& positions,
                         const Projection& projection,
                         const ParameterInfo& paraminfo,
                         const State& theState);

// Add the grid.size attribute, or the grid.width and grid.height attributes of the projection
void add_grid_size_attributes(QueryServer::Query& query, const Projection& projection);

// Add the clipping bounding box of the requested projection, and grid.countSize unless
// animating
void add_grid_bbox_attributes(QueryServer::Query& query,
                              const Projection& projection,
                              const Fmi::Box& clipbox,
                              bool animation_enabled);

// Add the bboxcrs, cx, cy and resolution settings of the projection
void add_projection_attributes(QueryServer::Query& query, const Projection& projection);

// Add the bounding box given in the projection settings, used with the data projection
void add_data_bbox_attribute(QueryServer::Query& query, const Projection& projection);

// Add the contour smoothing, subdivision and thread count attributes
void add_contour_attributes(QueryServer::Query& query,
                            const Smoother& smoother,
                            int subdivide,
                            const std::optional<int>& contour_threads);

// Add the contour extrapolation, multiplier, offset and coordinate type attributes
void add_contour_value_attributes(QueryServer::Query& query,
                                  int extrapolation,
                                  const std::optional<double>& multiplier,
                                  const std::optional<double>& offset);

// Parse the contour interpolation method name
std::optional<Trax::InterpolationType> parse_contour_interpolation(const std::string& interpolation);

// Convert the WKB contours returned by the query into despeckled OGRGeometry objects
std::vector<OGRGeometryPtr> contour_geometries(const QueryServer::Query& query,
                                               const std::optional<double>& minarea,
                                               const std::string& areaunit,
                                               const Fmi::Box& box,
                                               const Fmi::SpatialReference& crs);

// Remove small areas from a contour if a minimum area is set
OGRGeometryPtr despeckle_contour(const OGRGeometryPtr& geomPtr,
                                 const std::optional<double>& minarea,
                                 const std::string& areaunit,
                                 const Fmi::Box& box,
                                 const Fmi::SpatialReference& crs);

// Set the area, time and level interpolation methods of a vector query parameter
void set_vector_interpolation(QueryServer::QueryParameter& param, const std::string& interpolation);

// The first non-empty value vector in the query result, and its coordinates if available
std::shared_ptr<QueryServer::ParameterValues> first_value_vector(
    const QueryServer::Query& query, const T::Coordinate_vec*& coordinates);

// True if the grid rows are ordered from south to north based on the returned coordinates
bool is_south_up(const T::Coordinate_vec* coordinates, int width);

// The values in north-up row order
std::vector<float> north_up_values(const T::ParamValue_vec& values,
                                   const T::Coordinate_vec* coordinates,
                                   int width,
                                   int height);

// Update the projection size from the query result unless an explicit size was requested
void update_projection_size(Projection& projection, const QueryServer::Query& query);

// Update the projection from the query result if the data projection was requested
void update_data_projection(Projection& projection, const QueryServer::Query& query);

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
