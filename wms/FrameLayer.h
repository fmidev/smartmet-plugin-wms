// ======================================================================
/*!
 * \brief A frame layer
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "Layer.h"
#include "Text.h"
#include <boost/optional.hpp>
#include <gis/Types.h>
#include <string>

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

struct FrameDimension
{
  double bottomLatitude = 0.0;
  double leftLongitude = 0.0;
  double topLatitude = 0.0;
  double rightLongitude = 0.0;

  std::size_t hash_value() const;
};

struct TicInfo
{
  double step = 1.0;  // step in degrees
  int length = 10;    // length in pixels
  std::size_t hash_value() const;
};

struct FrameScale
{
  FrameScale() = default;

  FrameDimension dimension;
  boost::optional<TicInfo> smallTic;
  boost::optional<TicInfo> intermediateTic;
  boost::optional<TicInfo> longTic;
  std::string ticPosition;  // inside or outside of inner frame
  boost::optional<double> labelStep;
  boost::optional<std::string> labelPosition;  // inside, outside of inner frame

  std::size_t hash_value() const;
};

class FrameLayer : public Layer
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

 private:
  FrameDimension itsInnerBorder;
  boost::optional<FrameDimension> itsOuterBorder;
  boost::optional<std::string> itsPattern;
  boost::optional<FrameScale> itsScale;
  double itsPrecision = 1.0;
  OGRGeometryPtr itsGeom;
  Attributes itsScaleAttributes;

  void addScale(CTPP::CDT& theLayersCdt, const State& theState);
  void addTic(CTPP::CDT& theLayersCdt, double x1, double y1, double x2, double y2);
  void addScaleNumber(CTPP::CDT& theLayersCdt, double x, double y, const std::string& num);

};  // class FrameLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
