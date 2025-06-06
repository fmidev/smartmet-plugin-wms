// ======================================================================
/*!
 * \brief General PostGIS layer
 */
// ======================================================================

#pragma once

#include "Attributes.h"
#include "PostGISLayerBase.h"
#include "PostGISLayerFilter.h"

#include <engines/gis/MapOptions.h>
#include <gis/Types.h>

namespace SmartMet
{
namespace Plugin
{
namespace Dali
{
class Config;
class Plugin;
class State;

class IceMapLayer : public PostGISLayerBase
{
 public:
  void init(Json::Value& theJson,
            const State& theState,
            const Config& theConfig,
            const Properties& theProperties) override;

  void generate(CTPP::CDT& theGlobals, CTPP::CDT& theLayersCdt, State& theState) override;

  std::size_t hash_value(const State& theState) const override;

  std::string getParameterValue(const std::string& theKey) const;

 private:
  std::map<std::string, std::string> itsParameters;

  void handleSymbol(const Fmi::Feature& theResultItem,
                    CTPP::CDT& theGroupCdt,
                    const State& theState) const;
  void handleTextField(const Fmi::Feature& theResultItem,
                       const PostGISLayerFilter& theFilter,
                       CTPP::CDT& theGlobals,
                       CTPP::CDT& theLayersCdt,
                       CTPP::CDT& theGroupCdt,
                       const State& theState) const;
  void handleNamedLocation(const Fmi::Feature& theResultItem,
                           const PostGISLayerFilter& theFilter,
                           CTPP::CDT& theGlobals,
                           CTPP::CDT& theLayersCdt,
                           CTPP::CDT& theGroupCdt,
                           State& theState) const;

  void handleLabel(const Fmi::Feature& theResultItem,
                   const PostGISLayerFilter& theFilter,
                   CTPP::CDT& theGlobals,
                   CTPP::CDT& theLayersCdt,
                   const State& theState) const;

  void handleMeanTemperature(const Fmi::Feature& theResultItem,
                             const PostGISLayerFilter& theFilter,
                             CTPP::CDT& theGlobals,
                             CTPP::CDT& theLayersCdt,
                             State& theState) const;

  void handleTrafficRestrictions(const Fmi::Feature& theResultItem,
                                 const PostGISLayerFilter& theFilter,
                                 CTPP::CDT& theGlobals,
                                 CTPP::CDT& theLayersCdt,
                                 const State& theState) const;

  void handleIceEgg(const Fmi::Feature& theResultItem,
                    const PostGISLayerFilter& theFilter,
                    unsigned int& theMapId,
                    CTPP::CDT& theGlobals,
                    CTPP::CDT& theGroupCdt,
                    CTPP::CDT& theLayersCdt,
                    const State& theState) const;

  static void addLocationName(double theXPos,
                              double theYPos,
                              const std::string& theFirstName,
                              const std::string& theSecondName,
                              int thePosition,
                              int theArrowAngle,
                              const PostGISLayerFilter& theFilter,
                              CTPP::CDT& theGlobals,
                              CTPP::CDT& theLayersCdt,
                              CTPP::CDT& theGroupCdt,
                              const State& theState);

  void handleGeometry(const Fmi::Feature& theResultItem,
                      const PostGISLayerFilter& theFilter,
                      unsigned int& theMapId,
                      CTPP::CDT& theGlobals,
                      CTPP::CDT& theGroupCdt,
                      const State& theState) const;
  void handleResultItem(const Fmi::Feature& theResultItem,
                        const PostGISLayerFilter& theFilter,
                        unsigned int& theMapId,
                        CTPP::CDT& theGlobals,
                        CTPP::CDT& theLayersCdt,
                        CTPP::CDT& theGroupCdt,
                        State& theState) const;

  std::vector<std::string> getAttributeColumns() const;

};  // class IceMapLayer

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
