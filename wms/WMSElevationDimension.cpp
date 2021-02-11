#include "WMSElevationDimension.h"
#include <macgyver/Exception.h>
#include <functional>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{

WMSElevationDimension::WMSElevationDimension(const std::string& level_name, FmiLevelType level_type, const std::set<int>& elevations) 
  : itsLevelName(level_name), itsLevelType(level_type), itsElevations(elevations) 
{  
  auto comma_fold = [](std::string a, int b) {
	return std::move(a) + ',' + std::to_string(b);
  };
  
  if(itsLevelType == kFmiPressureLevel)
	  itsCapabilities = std::accumulate(std::next(elevations.rbegin()), elevations.rend(),
										std::to_string(*elevations.rbegin()), // start with last element
										comma_fold);
  else
	itsCapabilities = std::accumulate(std::next(elevations.begin()), elevations.end(),
									  std::to_string(*elevations.begin()), // start with first element
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
fmi:hbm:iceconcentration: surface - 160, levels: 0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100,150,200,300,400
test:t2m: pressure - 100, levels: 300,500,700,850,925,1000 (harmonie_skandinavia_painepinta)
test:t2m: model - 109, levels: 6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65 (harmonie_skandinavia_mallipinta)
test:t2m: pressure - 100, levels: 100,150,200,250,300,400,500,600,700,800,850,925,1000 (gfs_painepinta)
test:t2m: pressure - 100, levels: 50,100,200,250,300,400,500,600,700,850,925,1000 (hirlam_painepinta)
test:t2m: pressure - 100, levels: 250,300,400,500,600,700,850,925,1000 (ecmwf_eurooppa_painepinta)
test:t2m: pressure - 160, levels: 0,5,10,15,20,25,30,35,40,45,50,55,60,65,70,75,80,85,90,95,100,150,200,300,400 (hbm)
test:t2m: model - 109, levels: 2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65 (hirlam_mallipinta)

	 */

  switch(level_type)
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
	};

  switch(level_type)
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
	};
}
  
bool WMSElevationDimension::isValidElevation(const int elevation) const
{
  return itsElevations.find(elevation) != itsElevations.end();
}

std::string WMSElevationDimension::getDefaultElevation() const
{
  if(itsElevations.size() > 0)
	{
	  if(itsLevelType == kFmiPressureLevel)
		return std::to_string(*itsElevations.rbegin());
	  else
		return std::to_string(*itsElevations.begin());
	}

  return "";
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
  // If itsCapabilities is empty or '0' or level type is kFmiNoLevelType(0) dont show elevation dimension
  if(itsCapabilities.empty() || itsCapabilities == "0" || itsLevelType == kFmiNoLevelType)
	return false;

  return true;  
}

bool WMSElevationDimension::isIdentical(const WMSElevationDimension& td) const
{
  return (td.itsCapabilities == itsCapabilities && td.itsUnitSymbol == itsUnitSymbol);
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
