#pragma once
#include "Attributes.h"
#include "Layer.h"

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class GraticuleLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  std::size_t hash_value(const State& theState) const override;

  struct GraticuleLabels
  {
    std::string layout = "none";  // none, edges, grid, center, left_bottom
    unsigned int step = 10;       // desired multiples in degrees
    bool upright = false;         // upright or along tangent
    bool degree_sign = true;      // append degree sign (&deg;)
    bool minus_sign = true;       // use negative sign (&minus;) or append N/S/W/E
    int dx = 0;                   // offset in x/y direction or along normal in tangent mode
    int dy = 0;                   // in edges/left_bottom mode these are offsets from the edges
    Attributes attributes;
  };

  struct Graticule
  {
    std::string layout = "grid";  // grid, ticks
    unsigned int step = 10;       // desired multiples in degrees
    std::vector<int> except;  // ignored multiples (for example 10 if 1x1 graticule is generated)
    unsigned int length = 5;  // tick length
    GraticuleLabels labels;   // settings for labels
  };

 private:
  // Note: if for example generating 10x10, 5x5 and 1x1 graticules in different styles, it is
  // advisable to configure in order 1, 5 and 10 so that the most significant graticule is drawn
  // last on top of the less significant ones. In WMS mode dash patterns will break at tile edges,
  // using colour and line thickness instead would work better.

  std::vector<Graticule> graticules;

  // This one can be used to automatically generate a mask based on the generated labels
  // so that the lines will be cut automatically. The generated mask is of the form
  //
  // <mask id="qid-mask">
  //   <rect id="qid-mask" fill="white" width="100%" height="100%"/>
  //   <use xlink:href="#qid" style="filter:url(#alphadilation"/>
  // </mask>
  //
  // And the <g> group containing the lines will use style="mask:url(#isolabelmask)"

  std::string mask;        // for example "url(#alphadilation)", empty if no mask is wanted
  std::string mask_id;     // if empty, qid+mask will be used
  double precision = 1.0;  // coordinate output precision
};

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
