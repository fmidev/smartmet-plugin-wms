#pragma once

#include "IsolineLayer.h"
#include "Label.h"
#include <optional>
#include <vector>

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
class Config;
class Plugin;
class State;

// For label corner coordinates (estimates)

struct Coordinate
{
  double x = 0;
  double y = 0;
};

using Corners = std::array<Coordinate, 4>;

// For candidate positions
struct Candidate
{
  double isovalue = 0;
  double x = 0;
  double y = 0;
  double angle = 0;      // degrees
  double curvature = 0;  // total change in degrees along path, not "circle" curvature
  int id = 0;            // unique feature ID for each separate isoline
  double weight = 0;     // weight for minimum spanning tree algorithm
  Corners corners;       // approximated label corner coordinates

  // Needed for emplace_back
  Candidate(double p1, double p2, double p3, double p4, double p5, int p6, double p7)
      : isovalue(p1), x(p2), y(p3), angle(p4), curvature(p5), id(p6), weight(p7)
  {
  }
};

using Candidates = std::vector<Candidate>;

class IsolabelLayer : public IsolineLayer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  Label label;

  std::vector<double> angles{0, -45, 45, 180};  // rotate isolines to find directed extrema
  bool upright = false;                         // force labels to be upright
  double max_angle = 60;                        // and never rotate more than +-60 degrees

  double min_isoline_length = 50;  // minimum isoline length for label placement

  double min_distance_other = 20;  // min dist (px) to labels for other isovalues
  double min_distance_same = 50;   // min dist (px) to the same isovalue labels
  double min_distance_self = 200;  // min dist (px) to labels on the same isoline segment

  double min_distance_edge = 10;    // min dist (px) to image edge
  double max_distance_edge = 9999;  // max dist (px) to image edge

  double max_curvature = 90;  // max total curvature change

  int stencil_size = 5;  // for curvature and extrema analysis

  std::vector<double> isovalues;  // generated from isolines, isobands and isovalues settings

  Attributes textattributes;

 private:
  Candidates find_candidates(const std::vector<OGRGeometryPtr>& geoms) const;
  Candidates select_best_candidates(const Candidates& candidates, const Fmi::Box& box) const;

  void fix_orientation_gridEngine(Candidates& candidates,
                                  const Fmi::Box& box,
                                  const State& state,
                                  const Fmi::SpatialReference& crs) const;
  void fix_orientation(Candidates& candidates,
                       const Fmi::Box& box,
                       const State& state,
                       const Fmi::SpatialReference& crs) const;

};  // namespace Dali

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
