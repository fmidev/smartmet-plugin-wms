#pragma once

#include "IsolineLayer.h"
#include "Label.h"
#include <boost/optional.hpp>
#include <list>
#include <vector>

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
  double angle;  // radians
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
  boost::optional<std::string> symbol;

  std::vector<double> angles{0, -45, 45};  // rotate isolines to find directed extrema
  int max_total_count = 100;               // max total number of labels
  double min_distance_other = 10;  // min dist to already selected labels for other isovalues in px
  double min_distance_self = 50;  // min dist to already selected labels for the same isovalue in px
  double min_distance_edge = 20;  // min dist image edge in px
  double max_distance_edge = 9999;  // max dist to image edge in px
  double max_curvature = 30;        // max total curvature change
  double curvature_length = 15;     // measure curvature change over +- 15 pixels

  std::vector<double> isovalues;  // generated from isolines, isobands and isovalues settings

 private:
  std::vector<CandidateList> find_candidates(const std::vector<OGRGeometryPtr>& geoms);
  std::vector<CandidateList> select_best_candidates(
      const std::vector<CandidateList>& candidates) const;
  void fix_orientation(std::vector<CandidateList>& candidates,
                       const Fmi::Box& box,
                       OGRSpatialReference& crs) const;

};  // namespace Dali

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
