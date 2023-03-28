// ======================================================================
/*!
 * \brief An individual layer in a view
 *
 * Characteristics:
 *
 *  - unique projection with specific bounding box forced by view
 *  - may override layer/global producer and time
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layers.h"
#include "ParameterInfo.h"
#include "Properties.h"
#include <boost/optional.hpp>
#include <json/json.h>
#include <set>
#include <string>
#include <vector>

namespace CTPP
{
class CDT;
}

namespace Fmi
{
class Box;
class Spatialreference;
}  // namespace Fmi

namespace SmartMet
{
namespace Spine
{
class Parameter;
}
namespace Plugin
{
namespace Dali
{
class Config;
class Defs;
class Plugin;
class State;
class View;

class Layer : public Properties
{
 public:
  ~Layer() override = default;
  Layer() = default;
  Layer(const Layer& other) = delete;
  Layer(Layer&& other) = delete;
  Layer& operator=(const Layer& other) = delete;
  Layer& operator=(Layer&& other) = delete;

  using Properties::init;
  virtual void init(Json::Value& theJson,
                    const State& theState,
                    const Config& theConfig,
                    const Properties& theProperties);

  virtual void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) = 0;

  // Base provides a reasonable default!
  virtual std::size_t hash_value(const State& theState) const;

  void addGridParameterInfo(ParameterInfos& infos, const State& theState) const;

  // Does the layer satisfy resolution etc constraints?
  bool validLayer(const State& theState) const;

  void setProjection(Projection& proj);

  // Get the model data
  Engine::Querydata::Q getModel(const State& theState) const;

  // Generate clipPath to the output if needed
  void addClipRect(CTPP::CDT& theCdt,
                   CTPP::CDT& theGlobals,
                   const Fmi::Box& theBox,
                   const State& theState);

  // Generate bounding box for clipping paths and testing point insidedness

  Fmi::Box getClipBox(const Fmi::Box& theBox) const;

  // Generate bounding box for fetching observations
  std::map<std::string, double> getClipBoundingBox(const Fmi::Box& theBox,
                                                   const Fmi::SpatialReference& theCRS) const;

  // Layer specific settings
  std::string qid;

  // Resolution range
  boost::optional<double> minresolution;
  boost::optional<double> maxresolution;

  // Allowed and disallowed formats
  std::set<std::string> enable;
  std::set<std::string> disable;

  // External style sheet
  boost::optional<std::string> css;

  // SVG attributes (id, class, style, transform, filter...)
  Attributes attributes;

  // Inner layers
  Layers layers;

  boost::optional<std::string> type;

  static bool isFlashOrMobileProducer(const std::string& producer);

 private:
  bool validResolution() const;
  bool validType(const std::string& theType) const;

};  // class Layer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
