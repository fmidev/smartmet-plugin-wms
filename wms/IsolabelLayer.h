#pragma once

#include "IsolineLayer.h"
#include "Label.h"
#include <boost/optional.hpp>
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

// For candidate positions
struct Candidate
{
  double isovalue;
  double x;
  double y;
  double angle;      // degrees
  double curvature;  // total change in degrees along path, not "circle" curvature
  int id;            // unique feature ID for each separate isoline
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
