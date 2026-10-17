// ======================================================================
/*!
 * \brief A Circle layer, typically used to draw circles around airports or radars
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Circle.h"
#include "CircleLabels.h"
#include "Layer.h"
#include <gis/Types.h>
#include <spine/Location.h>
#include <optional>
#include <string>
#include <vector>

namespace CTPP
{
class CDT;
}

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class State;

class CircleLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;
  void getFeatureInfo(CTPP::CDT& theInfo, const State& theState) override;
  void addMVTLayer(class MVTTileBuilder& builder, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  Spine::LocationList resolveLocations(const State& theState) const;
  void generateLabels(CTPP::CDT& theGlobals,
                      CTPP::CDT& theLayersCdt,
                      State& theState,
                      const Spine::LocationList& locations,
                      const std::vector<OGRGeometryPtr>& all_circles,
                      const Fmi::CoordinateTransformation& transformation,
                      const Fmi::Box& clipbox,
                      const Fmi::Box& box) const;
  void addCircle(CTPP::CDT& theGlobals,
                 CTPP::CDT& group_cdt,
                 State& theState,
                 const OGRGeometryPtr& geom,
                 double lon,
                 double lat,
                 const Circle& circle,
                 const Fmi::Box& box,
                 const Fmi::SpatialReference& crs,
                 double precision) const;

  std::string keyword;           // locations from database
  std::set<std::string> places;  // locations from database
  std::set<int> geoids;          // locations from database
  std::string features;  // optional comma separated list of feature codes for the places search
  bool lines = true;     // render as polylines or as polygons
  std::vector<Circle> circles;  // actual circle definitions
  CircleLabels labels;          // optional radius labels

};  // class CircleLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
