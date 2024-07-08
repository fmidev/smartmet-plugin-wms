#include "WMSGridDataLayer.h"
#include <boost/move/make_unique.hpp>
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/identification/GridDef.h>
#include <macgyver/Exception.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
std::vector<Fmi::DateTime> get_ptime_vector(const std::set<std::string>& contentTimeList)
{
  std::vector<Fmi::DateTime> ret;
  ret.reserve(contentTimeList.size());

  for (const auto& time : contentTimeList)
    ret.push_back(Fmi::date_time::from_time_t(utcTimeToTimeT(time)));

  return ret;
}

time_t even_timesteps(const std::set<std::string>& contentTimeList)
{
  try
  {
    if (contentTimeList.size() < 2)
      return 0;

    time_t prevTime = 0;
    time_t step = 0;

    for (const auto& time : contentTimeList)
    {
      time_t tt = utcTimeToTimeT(time);
      if (prevTime != 0)
      {
        time_t s = tt - prevTime;
        if (step != 0 && s != step)
          return 0;

        step = s;
      }
      prevTime = tt;
    }

    return step;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Failed to update querydata layer metadata!");
  }
}

WMSGridDataLayer::WMSGridDataLayer(const WMSConfig& config,
                                   std::string producer,
                                   std::string parameter,
                                   uint geometryId,
                                   std::string elevation_unit)
    : WMSLayer(config),
      itsGridEngine(config.gridEngine()),
      itsProducer(producer),
      itsParameter(parameter),
      itsGeometryId(geometryId),
      itsElevationUnit(elevation_unit)
{
}

bool WMSGridDataLayer::updateLayerMetaData()
{
  try
  {
    if (!itsGridEngine || !itsGridEngine->isEnabled())
      return false;

    auto contentServer = itsGridEngine->getContentServer_sptr();

    //printf("WMSPARAM %s %s  %d\n",itsProducer.c_str(),itsParameter.c_str(),itsGeometryId);

    T::ParamKeyType parameterKeyType = T::ParamKeyTypeValue::FMI_NAME;
    std::string parameterKey = itsParameter;
    T::ParamLevelId parameterLevelId = 0;
    T::ParamLevel minLevel = 0;
    T::ParamLevel maxLevel = 1000000000;
    T::ForecastType forecastType = 1;
    T::ForecastNumber forecastNumber = -1;
    std::string startTime = "19000101T000000";
    std::string endTime = "23000101T000000";
    std::string producer = itsProducer;
    std::string fparam;

    if (itsParameter > "")
    {
      // Finding parameter information (it might be inside a function)

      char buf[2000];
      strcpy(buf,itsParameter.c_str());

      char *startpoint = buf;
      char *pp = buf;
      while (*pp != '\0' &&  *pp != '}')
      {
        if (*pp == '{')
          startpoint = pp+1;

        pp++;
      }
      *pp = '\0';

      //printf("PP [%s]\n",startpoint);

      // Function might have several parameters. Let's pick one that is not a number.

      std::vector<std::string> fp;
      splitString(startpoint, ';', fp);

      for (auto it = fp.begin(); it != fp.end()  &&  fparam.empty(); ++it)
      {
        const char *ps = it->c_str();
        if (ps[0] >= 'A')
          fparam = *it;
      }


      std::string param = itsGridEngine->getParameterString(producer, fparam);
      std::vector<std::string> p;
      splitString(param, ':', p);

      parameterKey = p[0];

      if (p.size() >= 2 && p[1] > "")
        producer = p[1];

      if (p.size() >= 3 && p[2] > "")
        itsGeometryId = toInt32(p[2]);

      if (p.size() >= 4 && p[3] > "")
        parameterLevelId = toInt32(p[3]);

      if (p.size() >= 5 && p[4] > "")
      {
        minLevel = toInt32(p[4]);
        maxLevel = minLevel;
      }

      if (p.size() >= 6 && p[5] > "")
        forecastType = toInt32(p[5]);

      if (p.size() >= 7 && p[6] > "")
        forecastNumber = toInt32(p[6]);
    }

    //printf("PRODUCER [%s][%s][%s][%d]\n",producer.c_str(),itsParameter.c_str(),fparam.c_str(),itsGeometryId);

    T::ProducerInfo producerInfo;
    if (contentServer->getProducerInfoByName(0, producer, producerInfo) != 0)
      return true;

    T::GenerationInfoList generationInfoList;
    if (contentServer->getGenerationInfoListByProducerId(
            0, producerInfo.mProducerId, generationInfoList) != 0 ||
        generationInfoList.getLength() == 0)
      return true;

    std::map<Fmi::DateTime, std::shared_ptr<WMSTimeDimension>> newTimeDimensions;
    for (unsigned int i = 0; i < generationInfoList.getLength(); i++)
    {
      T::GenerationInfo* generationInfo = generationInfoList.getGenerationInfoByIndex(i);

      // generation must exist and be marked complete (1)
      if (generationInfo == nullptr || generationInfo->mStatus != 1)
        continue;

      //printf("-- GENERATION %s  (geom=%d fparam=%s)\n",generationInfo->mName.c_str(),itsGeometryId,fparam.c_str());
      if (itsGeometryId <= 0)
      {
        std::set<T::GeometryId> geometryIdList;
        if (contentServer->getContentGeometryIdListByGenerationId(
                0, generationInfo->mGenerationId, geometryIdList) != 0)
          return true;

        if (geometryIdList.empty())
          continue;

        itsGeometryId = *geometryIdList.begin();
      }

      if (itsGeometryId > 0)
      {
        auto def = Identification::gridDef.getGrib2DefinitionByGeometryId(itsGeometryId);
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
        if (len == 0)
        {
          // Parameter name can be an alias name. Trying to find it from the parameter mappings.

          QueryServer::ParameterMapping_vec mappings;
          itsGridEngine->getParameterMappings(producerInfo.mName,parameterKey,itsGeometryId,parameterLevelId,minLevel,false,mappings);

          if (mappings.size() == 0)
          {
            // We did not find any parameter mappings with the given levels. Let's try without levels.
            itsGridEngine->getParameterMappings(producerInfo.mName,parameterKey,itsGeometryId,true,mappings);
          }

          if (mappings.size() == 0)
            return true;

          parameterKey = mappings[0].mParameterKey;
          parameterLevelId = mappings[0].mParameterLevelId;

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
          if (contentTimeList.find(info->getForecastTime()) == contentTimeList.end())
            contentTimeList.insert(std::string(info->getForecastTime()));
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
          for (uint t=50; t<=15000; t=t+step)
            levelList.insert(t);
        }
        else
        if (parameterLevelId > 0)
        {
          // Original levels according to levelType
          for (uint t = 0; t < len; t++)
          {
            T::ContentInfo* info = contentInfoList.getContentInfoByIndex(t);
            levelList.insert(info->mParameterLevel);
          }
        }

        // Add elevation information if there is more than one level.
        if (!elevationDimension  &&  levelList.size() > 1)
        {
          Identification::LevelDef levelDef;
          if (Identification::gridDef.getFmiLevelDef(parameterLevelId,levelDef))
          {
            std::string unit_symbol = itsElevationUnit;
            if (unit_symbol.empty())
              unit_symbol = levelDef.mUnits;

            WMSElevationDimension elev(levelDef.mName,levelDef.mLevelId,unit_symbol,levelList,step);
            elevationDimension = std::make_shared<WMSElevationDimension>(elev);
          }
        }
      }
      else
      {
        if (contentServer->getContentTimeListByGenerationAndGeometryId(
                0, generationInfo->mGenerationId, itsGeometryId, contentTimeList) != 0)
          return true;
      }

      std::shared_ptr<WMSTimeDimension> timeDimension;

      // timesteps
      std::list<Fmi::DateTime> timesteps;
      for (const auto& stime : contentTimeList)
        timesteps.push_back(toTimeStamp(stime));

      time_intervals intervals = get_intervals(timesteps);

      if (!intervals.empty())
        timeDimension = std::make_shared<IntervalTimeDimension>(intervals);
      else
        timeDimension = std::make_shared<StepTimeDimension>(timesteps);

      if (timeDimension)
        newTimeDimensions.insert(
            std::make_pair(toTimeStamp(generationInfo->mAnalysisTime), timeDimension));
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
