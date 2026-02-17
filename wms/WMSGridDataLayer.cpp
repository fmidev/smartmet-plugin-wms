#include "WMSGridDataLayer.h"
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/identification/GridDef.h>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{
  void apply_timestep(std::list<Fmi::DateTime>& timelist, std::optional<int> timestep)
  {
    if(!timestep || timestep <= 0)
      return;

    timelist.remove_if([&](const Fmi::DateTime& t) {
        // Anchor to UTC midnight of that day
	// auto midnight = floor<Fmi::Days>(t);
	auto midnight = Fmi::DateTime(t.date(), Fmi::Minutes(0));

        auto since_midnight = (t - midnight).minutes();
        return (since_midnight % *timestep) != 0;
    });
  }
}

WMSGridDataLayer::WMSGridDataLayer(const WMSConfig& config,
                                   std::string producer,
                                   std::string parameter,
                                   int forecastType,
                                   uint geometryId,
                                   int levelId,
                                   std::string elevation_unit)
    : WMSLayer(config),
      itsGridEngine(config.gridEngine()),
      itsProducer(std::move(producer)),
      itsParameter(std::move(parameter)),
      itsForecastType(forecastType),
      itsGeometryId(geometryId),
      itsLevelId(levelId),
      itsElevationUnit(std::move(elevation_unit))
{
}

bool WMSGridDataLayer::updateLayerMetaData()
{
  try
  {
    if (!itsGridEngine || !itsGridEngine->isEnabled())
      return false;

    auto contentServer = itsGridEngine->getContentServer_sptr();

    // printf("WMSPARAM %s %s  %d\n",itsProducer.c_str(),itsParameter.c_str(),itsGeometryId);

    T::ParamKeyType parameterKeyType = T::ParamKeyTypeValue::FMI_NAME;
    std::string parameterKey = itsParameter;
    T::ParamLevelId parameterLevelId = itsLevelId;
    T::ParamLevel minLevel = 0;
    T::ParamLevel maxLevel = 1000000000;
    T::ForecastType forecastType = itsForecastType;
    T::ForecastNumber forecastNumber = -1;
    std::string startTime = "19000101T000000";
    std::string endTime = "23000101T000000";
    std::string producer = itsProducer;
    std::string fparam;
    std::string lastParam;
    std::set<int> functionGeometries;

    if (!itsParameter.empty())
    {
      // Finding parameter information (it might be inside a function)

      char buf[2000];
      strcpy(buf, itsParameter.c_str());
      char* startpoint = buf;
      char* pp = buf;
      while (*pp != '\0' && *pp != '}')
      {
        if (*pp == '{')
        {
          startpoint = pp + 1;
        }

        pp++;
      }
      *pp = '\0';

      // printf("PP [%s]\n",startpoint);

      // Function might have several parameters.

      std::vector<std::string> fp;
      splitString(startpoint, ';', fp);

      for (auto it = fp.begin(); it != fp.end(); ++it)
      {
        const char* ps = it->c_str();

        if (ps[0] >= 'A')
          lastParam = *it;

        if (!producer.empty())
        {
          // The produce is not empty. Let's try to find a parameter with the same producer.
          std::vector<std::string> p;
          splitString(ps, ':', p);

          if (p.size() >= 3 && !p[2].empty())
          {
            functionGeometries.insert(toInt32(p[2]));
          }

          if (p.size() >= 2 && strcasecmp(p[1].c_str(), producer.c_str()) == 0)
          {
            fparam = *it;
          }
        }
      }

      if (fparam.empty())
      {
        // No parameter with the given producer found. Let's use the last parameter.
        fparam = lastParam;
      }

      std::string param = itsGridEngine->getParameterString(producer, fparam);
      std::vector<std::string> p;
      splitString(param, ':', p);

      parameterKey = p[0];

      if (p.size() >= 2 && !p[1].empty())
        producer = p[1];

      if (p.size() >= 3 && !p[2].empty() && functionGeometries.size() < 2)
        itsGeometryId = toInt32(p[2]);

      if (p.size() >= 4 && !p[3].empty())
        parameterLevelId = toInt32(p[3]);

      if (p.size() >= 5 && !p[4].empty())
      {
        minLevel = toInt32(p[4]);
        maxLevel = minLevel;
      }

      if (p.size() >= 6 && !p[5].empty())
        forecastType = toInt32(p[5]);

      if (p.size() >= 7 && !p[6].empty())
        forecastNumber = toInt32(p[6]);
    }

    // printf("PRODUCER
    // [%s][%s][%s][%d]\n",producer.c_str(),itsParameter.c_str(),fparam.c_str(),itsGeometryId);

    T::ProducerInfo producerInfo;
    if (contentServer->getProducerInfoByName(0, producer, producerInfo) != 0)
      return true;

    T::GenerationInfoList generationInfoList;
    if (contentServer->getGenerationInfoListByProducerId(
            0, producerInfo.mProducerId, generationInfoList) != 0 ||
        generationInfoList.getLength() == 0)
      return true;

    std::set<int> validGeometries;

    Identification::FmiGeometryGroupDef geometryGroupDef;
    if (Identification::gridDef.getFmiGeometryGroupDef(
            producerInfo.mName.c_str(), 1, geometryGroupDef))
    {
      for (auto aIt = geometryGroupDef.mGeometryIdList.begin();
           aIt != geometryGroupDef.mGeometryIdList.end();
           ++aIt)
      {
        validGeometries.insert(*aIt);
      }
      itsGeometryId = -1;
    }

    // printf("VALIDGEOMS %ld   FUNC %ld
    // geomId=%d\n",validGeometries.size(),functionGeometries.size(),itsGeometryId);

    if (validGeometries.empty() && !functionGeometries.empty())
      validGeometries = functionGeometries;

    std::map<Fmi::DateTime, std::shared_ptr<WMSTimeDimension>> newTimeDimensions;
    for (unsigned int i = 0; i < generationInfoList.getLength(); i++)
    {
      T::GenerationInfo* generationInfo = generationInfoList.getGenerationInfoByIndex(i);

      // generation must exist and be marked complete (1)
      if (generationInfo == nullptr || generationInfo->mStatus != 1)
        continue;

      // printf("-- GENERATION %s %u  (geom=%d fparam=%s  itsParam=%s
      // geoms=%ld)\n",generationInfo->mName.c_str(),generationInfo->mGenerationId,itsGeometryId,fparam.c_str(),itsParameter.c_str(),validGeometries.size());
      if (itsGeometryId <= 0 && validGeometries.empty())
      {
        std::set<T::GeometryId> geometryIdList;
        if (contentServer->getContentGeometryIdListByGenerationId(
                0, generationInfo->mGenerationId, geometryIdList) != 0)
          return true;

        if (geometryIdList.empty())
          continue;

        itsGeometryId = *geometryIdList.begin();
        validGeometries.insert(itsGeometryId);
      }

      if (validGeometries.empty())
        validGeometries.insert(itsGeometryId);

      int tmpGeometryId = itsGeometryId;
      if (tmpGeometryId <= 0 && !validGeometries.empty())
        tmpGeometryId = *validGeometries.begin();

      if (tmpGeometryId > 0)
      {
        auto def = Identification::gridDef.getGrib2DefinitionByGeometryId(tmpGeometryId);
        if (def)
        {
          T::Coordinate topLeft;
          T::Coordinate topRight;
          T::Coordinate bottomLeft;
          T::Coordinate bottomRight;
          if (def->getGridLatLonArea(topLeft, topRight, bottomLeft, bottomRight))
          {
            geographicBoundingBox.xMin = std::min(topLeft.x(), bottomLeft.x());
            geographicBoundingBox.xMax = std::max(topRight.x(), bottomRight.x());
            geographicBoundingBox.yMin = std::min(bottomLeft.y(), bottomRight.y());
            geographicBoundingBox.yMax = std::max(topLeft.y(), topRight.y());
          }
        }
      }

      // printf("#### VALIDGEOMS %ld   FUNC %ld  geomId=%d
      // PARAM=%s\n",validGeometries.size(),functionGeometries.size(),itsGeometryId,parameterKey.c_str());

      std::set<std::string> contentTimeList;

      if (itsParameter > "")
      {
        // Trying to find content records with the given parameter name.

        T::ContentInfoList contentInfoList;
        if (contentServer->getContentListByParameterAndGenerationId(0,
                                                                    generationInfo->mGenerationId,
                                                                    parameterKeyType,
                                                                    parameterKey,
                                                                    parameterLevelId,
                                                                    minLevel,
                                                                    maxLevel,
                                                                    forecastType,
                                                                    forecastNumber,
                                                                    itsGeometryId,
                                                                    startTime,
                                                                    endTime,
                                                                    0,
                                                                    contentInfoList) != 0)
          return true;

        uint len = contentInfoList.getLength();
        // printf("CONTENTINFOLIST %u\n",len);
        if (len == 0)
        {
          // Parameter name can be an alias name. Trying to find it from the parameter mappings.

          QueryServer::ParameterMapping_vec mappings;
          itsGridEngine->getParameterMappings(producerInfo.mName,
                                              parameterKey,
                                              tmpGeometryId,
                                              parameterLevelId,
                                              minLevel,
                                              false,
                                              mappings);

          // printf("MAPPINGS %ld\n",mappings.size());
          if (mappings.empty())
          {
            // Trying to find parameter mappings without levels.

            itsGridEngine->getParameterMappings(
                producerInfo.mName, parameterKey, tmpGeometryId, true, mappings);

            // printf("# MAPPINGS %ld\n",mappings.size());
            if (mappings.empty())
            {
              // Trying to find parameter mappings without geometry.
              itsGridEngine->getParameterMappings(producerInfo.mName, parameterKey, true, mappings);
            }
            // printf("## MAPPINGS %ld\n",mappings.size());
          }

          if (!mappings.empty())
          {
            parameterKey = mappings[0].mParameterKey;
            parameterLevelId = mappings[0].mParameterLevelId;
          }

          if (contentServer->getContentListByParameterAndGenerationId(0,
                                                                      generationInfo->mGenerationId,
                                                                      parameterKeyType,
                                                                      parameterKey,
                                                                      parameterLevelId,
                                                                      minLevel,
                                                                      maxLevel,
                                                                      forecastType,
                                                                      forecastNumber,
                                                                      itsGeometryId,
                                                                      startTime,
                                                                      endTime,
                                                                      0,
                                                                      contentInfoList) != 0)
            return true;

          len = contentInfoList.getLength();
        }

        // Picking content times:
        for (uint t = 0; t < len; t++)
        {
          T::ContentInfo* info = contentInfoList.getContentInfoByIndex(t);
          if (validGeometries.find(info->mGeometryId) != validGeometries.end())
          {
            if (contentTimeList.find(info->getForecastTime()) == contentTimeList.end())
              contentTimeList.insert(std::string(info->getForecastTime()));
          }
        }

        // Picking levels:
        std::set<int> levelList;
        int step = 0;
        if (itsElevationUnit == "m")
        {
          // The product is using metric levels (=> will be counted from pressure/hybrid levels)

          // We have to list all available levels, because the elevation check cannot
          // validate level ranges.

          step = 50;
          for (uint t = 50; t <= 15000; t = t + step)
            levelList.insert(t);
        }
        else if (parameterLevelId > 0)
        {
          // Original levels according to levelType
          for (uint t = 0; t < len; t++)
          {
            T::ContentInfo* info = contentInfoList.getContentInfoByIndex(t);
            levelList.insert(info->mParameterLevel);
          }
        }

        // Add elevation information if there is more than one level.
        if (!elevationDimension && levelList.size() > 1)
        {
          Identification::LevelDef levelDef;
          if (Identification::gridDef.getFmiLevelDef(parameterLevelId, levelDef))
          {
            std::string unit_symbol = itsElevationUnit;
            if (unit_symbol.empty())
              unit_symbol = levelDef.mUnits;

            elevationDimension = std::make_shared<WMSElevationDimension>(
                levelDef.mName, levelDef.mLevelId, unit_symbol, levelList);
          }
        }
      }
      else
      {
        if (contentServer->getContentTimeListByGenerationAndGeometryId(
                0, generationInfo->mGenerationId, itsGeometryId, contentTimeList) != 0)
          return true;
      }

      if (!contentTimeList.empty())
      {
        std::shared_ptr<WMSTimeDimension> timeDimension;

        // timesteps
        std::list<Fmi::DateTime> timesteps;
        for (const auto& stime : contentTimeList)
          timesteps.push_back(toTimeStamp(stime));

	apply_timestep(timesteps, timestep);
	
        time_intervals intervals = get_intervals(timesteps);

        if (!intervals.empty())
          timeDimension = std::make_shared<IntervalTimeDimension>(intervals);
        else
          timeDimension = std::make_shared<StepTimeDimension>(timesteps);

        if (timeDimension)
          newTimeDimensions.insert(
              std::make_pair(toTimeStamp(generationInfo->mAnalysisTime), timeDimension));
      }
    }

    if (!newTimeDimensions.empty())
      timeDimensions = std::make_shared<WMSTimeDimensions>(newTimeDimensions);
    else
      timeDimensions = nullptr;

    metadataTimestamp = Fmi::SecondClock::universal_time();

    return true;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update grid layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
