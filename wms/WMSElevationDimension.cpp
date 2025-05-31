#include "WMSElevationDimension.h"
#include <macgyver/Exception.h>
#include <macgyver/StringConversion.h>
#include <functional>
#include <iostream>
#include <numeric>
#include <string.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
WMSElevationDimension::WMSElevationDimension(std::string level_name,
                                             FmiLevelType level_type,
                                             const std::set<int>& elevations)
    : itsLevelName(std::move(level_name)), itsLevelType(level_type), itsElevations(elevations)
{
  try
  {
    initDefaultElevation();

    auto comma_fold = [](std::string a, int b) { return std::move(a) + ',' + Fmi::to_string(b); };

    if (itsLevelType == kFmiPressureLevel)
      itsCapabilities =
          std::accumulate(std::next(elevations.rbegin()),
                          elevations.rend(),
                          Fmi::to_string(*elevations.rbegin()),  // start with last element
                          comma_fold);
    else
      itsCapabilities =
          std::accumulate(std::next(elevations.begin()),
                          elevations.end(),
                          Fmi::to_string(*elevations.begin()),  // start with first element
                          comma_fold);

    /*
  kFmiNoLevelType = 0,      //!< This is error or no initialized leveltype value
  kFmiGroundSurface = 1,    //!< Ground or water surface
  kFmiSoundingLevel = 50,   //!< Sounding level type
  kFmiAmdarLevel = 51,      //!< Amdar level type
  kFmiPressureLevel = 100,  //!< Isobaric level
  kFmiMeanSeaLevel = 102,   //!< ???
  kFmiAltitude = 103,       //!< Altitude above mean sea level
  kFmiHeight = 105,         //!< Height above ground
  kFmiHybridLevel = 109,    //!< NWP model level
  kFmiFlightLevel = 120,  //!< Flight Level tyyppi eli korkeus 100 jaloissa (FL150 = 150*100*0.305m)
  kFmiDepth = 160,        //!< Depth below mean sea level (meters)
  kFmiRoadClass1 = 169,   //!< Aurausmallin kunnossapitoluokka 1
  kFmiRoadClass2 = 170,   //!< Aurausmallin kunnossapitoluokka 2
  kFmiRoadClass3 = 171,   //!< Aurausmallin kunnossapitoluokka 3

  kFmi = 1001,             //!< Level of SYNOP observation, height 0...10 m
  kFmiAnyLevelType = 5000  //!< ???

  fmi:aurausmalli:finland:roadcondition: model - 105, levels: 102,202,302
  fmi::myocean_nemo:icethickness: model - 5000, levels: 0
  ely:observation:roadtemperature: surface - 5000, levels: 0
  ely:observation:t2m: surface - 105, levels: 2
  fmi:detectability:detectability: surface - 0, levels: 32700 (error)
  fmi:hbm:iceconcentration: surface - 160, levels:
  0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100,150,200,300,400 test:t2m: pressure -
  100, levels: 300,500,700,850,925,1000 (harmonie_skandinavia_painepinta) test:t2m: model - 109,
  levels:
  6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65
  (harmonie_skandinavia_mallipinta) test:t2m: pressure - 100, levels:
  100,150,200,250,300,400,500,600,700,800,850,925,1000 (gfs_painepinta) test:t2m: pressure - 100,
  levels: 50,100,200,250,300,400,500,600,700,850,925,1000 (hirlam_painepinta) test:t2m: pressure -
  100, levels: 250,300,400,500,600,700,850,925,1000 (ecmwf_eurooppa_painepinta) test:t2m: pressure -
  160, levels: 0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100,150,200,300,400 (hbm)
  test:t2m: model - 109, levels:
  2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65
  (hirlam_mallipinta)

     */

    switch (level_type)
    {
      case kFmiPressureLevel:
        itsUnitSymbol = "hPa";
        break;
      case kFmiFlightLevel:
        itsUnitSymbol = "ft100";
        break;
      case kFmiAltitude:
      case kFmiHeight:
      case kFmiDepth:
      case kFmi:
        itsUnitSymbol = "m";
        break;
      default:
        itsUnitSymbol = "";
    }

    switch (level_type)
    {
      case kFmiAnyLevelType:
        itsLevelName = "FmiAnyLevelType";
        break;
      case kFmiNoLevelType:
        itsLevelName = "UninitializedLevelType";
        break;
      case kFmi:
        itsLevelName = "SYNOPObservationlevel";
        break;
      case kFmiGroundSurface:
        itsLevelName = "GroundOrWaterSurface";
        break;
      case kFmiSoundingLevel:
        itsLevelName = "SoundingLevel";
        break;
      case kFmiAmdarLevel:
        itsLevelName = "AmdarLevel";
        break;
      case kFmiPressureLevel:
        itsLevelName = "IsobaricLevel";
        break;
      case kFmiMeanSeaLevel:
        itsLevelName = "MeanSeaLevel";
        break;
      case kFmiAltitude:
        itsLevelName = "AltitudeAboveMeanSeaLevel";
        break;
      case kFmiHeight:
        itsLevelName = "HeightAboveGround";
        break;
      case kFmiHybridLevel:
        itsLevelName = "NWPModelLevel";
        break;
      case kFmiFlightLevel:
        itsLevelName = "FlightLevel";
        break;
      case kFmiDepth:
        itsLevelName = "DepthBelowMeanSeaLevel";
        break;
      case kFmiRoadClass1:
        itsLevelName = "AurausmallinKunnossapitoluokka1";
        break;
      case kFmiRoadClass2:
        itsLevelName = "AurausmallinKunnossapitoluokka2";
        break;
      case kFmiRoadClass3:
        itsLevelName = "AurausmallinKunnossapitoluokka3";
        break;
    }
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking GetLegendGraphic options failed!");
  }
}

WMSElevationDimension::WMSElevationDimension(std::string level_name,
                                             short level_type,
                                             std::string unit_symbol,
                                             const std::set<int>& elevations,
                                             int step)
    : itsLevelName(level_name), itsLevelType(level_type), itsElevations(elevations)
{
  try
  {
    initDefaultElevation();

    auto comma_fold = [](std::string a, int b) { return std::move(a) + ',' + Fmi::to_string(b); };

    if (strcasecmp(itsLevelName.c_str(), "PRESSURE") == 0)
    {
      if (*elevations.begin() <= 1000 || (*elevations.rbegin() <= 1000 && elevations.size() > 1))
        itsUnitSymbol = "hPa";
      else
        itsUnitSymbol = "Pa";

      // start with last element
      itsCapabilities = std::accumulate(std::next(elevations.rbegin()),
                                        elevations.rend(),
                                        Fmi::to_string(*elevations.rbegin()),
                                        comma_fold);
    }
    else
    {
      // start with first element

      itsUnitSymbol = unit_symbol;
      char buf[100];
      if (elevations.size() > 1 && step > 0)
      {
        sprintf(buf, "%d/%d/%d", *elevations.begin(), *elevations.rbegin(), step);
        itsCapabilities = buf;
      }
      else if (elevations.size() > 1 &&
               (int)(*elevations.begin() + elevations.size() - 1) == *elevations.rbegin())
      {
        sprintf(buf, "%d/%d/1", *elevations.begin(), *elevations.rbegin());
        itsCapabilities = buf;
      }
      else
      {
        itsCapabilities = std::accumulate(std::next(elevations.begin()),
                                          elevations.end(),
                                          Fmi::to_string(*elevations.begin()),
                                          comma_fold);
      }
    }

    // std::cout << "CAPABILITIES : " << itsLevelName << " : " << itsUnitSymbol << " : " <<
    // itsCapabilities << "\n";
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Checking GetLegendGraphic options failed!");
  }
}

void WMSElevationDimension::initDefaultElevation()
{
  if (itsElevations.empty())
    return;

  if (itsLevelType == kFmiPressureLevel || itsLevelType == 2)
    itsDefaultElevation = Fmi::to_string(*itsElevations.rbegin());
  else
    itsDefaultElevation = Fmi::to_string(*itsElevations.begin());
}

bool WMSElevationDimension::isValidElevation(int elevation) const
{
  return itsElevations.find(elevation) != itsElevations.end();
}

const std::string& WMSElevationDimension::getUnitSymbol() const
{
  return itsUnitSymbol;
}

const std::string& WMSElevationDimension::getCapabilities() const
{
  return itsCapabilities;
}

const std::string& WMSElevationDimension::getLevelName() const
{
  return itsLevelName;
}

bool WMSElevationDimension::isOK() const
{
  // If itsCapabilities is empty or '0' or level type is kFmiNoLevelType(0) dont show elevation
  // dimension
  return (!itsCapabilities.empty() && itsCapabilities != "0" && itsLevelType != kFmiNoLevelType);
}

bool WMSElevationDimension::isIdentical(const WMSElevationDimension& td) const
{
  return (td.itsCapabilities == itsCapabilities && td.itsUnitSymbol == itsUnitSymbol &&
          td.getDefaultElevation() == getDefaultElevation());
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
