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
std::vector<boost::posix_time::ptime> get_ptime_vector(const std::set<std::string>& contentTimeList)
{
  std::vector<boost::posix_time::ptime> ret;
  ret.reserve(contentTimeList.size());

  for (const auto& time : contentTimeList)
    ret.push_back(boost::posix_time::from_time_t(utcTimeToTimeT(time)));

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
                                   uint geometryId)
    : WMSLayer(config),
      itsGridEngine(config.gridEngine()),
      itsProducer(std::move(producer)),
      itsParameter(std::move(parameter)),
      itsGeometryId(geometryId)
{
}

bool WMSGridDataLayer::updateLayerMetaData()
{
  try
  {
    if (!itsGridEngine || !itsGridEngine->isEnabled())
      return false;

    auto contentServer = itsGridEngine->getContentServer_sptr();

    T::ProducerInfo producerInfo;
    if (contentServer->getProducerInfoByName(0, itsProducer, producerInfo) != 0)
      return true;

    T::GenerationInfoList generationInfoList;
    if (contentServer->getGenerationInfoListByProducerId(
            0, producerInfo.mProducerId, generationInfoList) != 0 ||
        generationInfoList.getLength() == 0)
      return true;

    std::map<boost::posix_time::ptime, boost::shared_ptr<WMSTimeDimension>> newTimeDimensions;
    for (unsigned int i = 0; i < generationInfoList.getLength(); i++)
    {
      T::GenerationInfo* generationInfo = generationInfoList.getGenerationInfoByIndex(i);

      if (generationInfo == nullptr)
        continue;

      if (itsGeometryId <= 0)
      {
        std::set<T::GeometryId> geometryIdList;
        if (contentServer->getContentGeometryIdListByGenerationId(
                0, generationInfo->mGenerationId, geometryIdList) != 0 ||
            geometryIdList.empty())
          return true;

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
        std::string param = itsGridEngine->getParameterString(itsProducer, itsParameter);
        std::vector<std::string> p;
        splitString(param, ':', p);

        T::ParamKeyType parameterKeyType = T::ParamKeyTypeValue::FMI_NAME;
        std::string parameterKey = p[0];
        T::ParamLevelId parameterLevelId = 0;
        T::ParamLevel minLevel = 0;
        T::ParamLevel maxLevel = 1000000000;
        T::ForecastType forecastType = 1;
        T::ForecastNumber forecastNumber = -1;
        std::string startTime = "19000101T000000";
        std::string endTime = "23000101T000000";

        if (p.size() >= 3 && p[2] > "")
        {
          itsGeometryId = toInt32(p[2]);
        }

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
        for (uint t = 0; t < len; t++)
        {
          T::ContentInfo* info = contentInfoList.getContentInfoByIndex(t);
          if (contentTimeList.find(info->getForecastTime()) == contentTimeList.end())
            contentTimeList.insert(std::string(info->getForecastTime()));
        }
      }
      else
      {
        if (contentServer->getContentTimeListByGenerationAndGeometryId(
                0, generationInfo->mGenerationId, itsGeometryId, contentTimeList) != 0)
          return true;
      }

      boost::shared_ptr<WMSTimeDimension> timeDimension;
      // timesteps
      std::list<boost::posix_time::ptime> timesteps;
      for (const auto& stime : contentTimeList)
        timesteps.push_back(toTimeStamp(stime));
      time_intervals intervals = get_intervals(timesteps);
      if (!intervals.empty())
        timeDimension = boost::make_shared<IntervalTimeDimension>(intervals);
      else
        timeDimension = boost::make_shared<StepTimeDimension>(timesteps);

      /*
  time_t step = even_timesteps(contentTimeList);
  if (step > 0)
  {
    // time interval
    boost::posix_time::time_duration timestep = boost::posix_time::seconds(step);
    timeDimension =
        boost::make_shared<IntervalTimeDimension>(toTimeStamp(*(contentTimeList.begin())),
                                                  toTimeStamp(*(--contentTimeList.end())),
                                                  timestep);
  }
  else
  {
    // timesteps
    std::list<boost::posix_time::ptime> times;
    for (const auto& stime : contentTimeList)
      times.push_back(toTimeStamp(stime));
    timeDimension = boost::make_shared<StepTimeDimension>(times);
  }
      */
      if (timeDimension)
        newTimeDimensions.insert(
            std::make_pair(toTimeStamp(generationInfo->mAnalysisTime), timeDimension));
    }

    if (!newTimeDimensions.empty())
      timeDimensions = boost::make_shared<WMSTimeDimensions>(newTimeDimensions);
    else
      timeDimensions = nullptr;

    metadataTimestamp = boost::posix_time::second_clock::universal_time();

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
