#pragma once

#include "IsolineLayer.h"
#include "Label.h"
#include <boost/optional.hpp>
#include <list>
#include <vector>

namespace Fmi
{
class Box;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

struct Candidate
{
  double x;
  double y;
  double angle;      // degrees
  double curvature;  // total change in degrees along path, not "circle" curvature
};

using CandidateList = std::list<Candidate>;

class IsolabelLayer : public IsolineLayer
{
 public:
  virtual void init(const Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState);

  virtual std::size_t hash_value(const State& theState) const;

  Label label;

  std::vector<double> angles{0, -45, 45, 180};  // rotate isolines to find directed extrema
  bool upright = false;                         // force labels to be upright
  double max_angle = 60;                        // and never rotate more than +-60 degrees

  double min_distance_other = 10;   // min dist (px) to already selected labels for other isovalues
  double max_distance_other = 100;  // max distance (px) to labels for other isovalues
  double min_distance_self = 50;  // min dist (px) to already selected labels for the same isovalue

  double min_distance_edge = 10;    // min dist (px) to image edge
  double max_distance_edge = 9999;  // max dist (px) to image edge

  double max_curvature = 90;  // max total curvature change

  int stencil_size = 5;  // for curvature and extrema analysis

  std::vector<double> isovalues;  // generated from isolines, isobands and isovalues settings

 private:
  std::vector<CandidateList> find_candidates(const std::vector<OGRGeometryPtr>& geoms);
  std::vector<CandidateList> select_best_candidates(const std::vector<CandidateList>& candidates,
                                                    const Fmi::Box& box) const;
  void fix_orientation(std::vector<CandidateList>& candidates,
                       const Fmi::Box& box,
                       OGRSpatialReference& crs) const;

};  // namespace Dali

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
