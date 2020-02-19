#include "WMSGridDataLayer.h"
#include <boost/move/make_unique.hpp>
#include <spine/Exception.h>
#include <grid-files/identification/GridDef.h>
#include <grid-files/common/GeneralFunctions.h>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{



time_t even_timesteps(std::set<std::string>& contentTimeList)
{
  try
  {
    if (contentTimeList.size() < 2)
      return 0;

    time_t prevTime = 0;
    time_t step = 0;

    for (auto it = contentTimeList.begin(); it != contentTimeList.end(); ++it)
    {
      time_t tt = utcTimeToTimeT(*it);
      if (prevTime != 0)
      {
        time_t s = tt-prevTime;
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
    throw Spine::Exception::Trace(BCP, "Failed to update querydata layer metadata!");
  }
}




WMSGridDataLayer::WMSGridDataLayer(const WMSConfig& config, const std::string& producer,uint geometryId)
    : WMSLayer(config), itsGridEngine(config.gridEngine()), itsProducer(producer),itsGeometryId(geometryId)
{
}




void WMSGridDataLayer::updateLayerMetaData()
{
  try
  {
    auto contentServer = itsGridEngine->getContentServer_sptr();

    T::ProducerInfo producerInfo;
    if (contentServer->getProducerInfoByName(0,itsProducer,producerInfo) != 0)
      return;


    T::GenerationInfoList generationInfoList;
    if (contentServer->getGenerationInfoListByProducerId(0,producerInfo.mProducerId,generationInfoList) != 0 || generationInfoList.getLength() == 0)
      return;

    T::GenerationInfo *generationInfo = generationInfoList.getLastGenerationInfoByProducerId(producerInfo.mProducerId);
    if (generationInfo == nullptr)
      return;

    if (itsGeometryId <= 0)
    {
      std::set<T::GeometryId> geometryIdList;
      if (contentServer->getContentGeometryIdListByGenerationId(0,generationInfo->mGenerationId,geometryIdList) != 0 || geometryIdList.size() == 0)
        return;

      itsGeometryId = *geometryIdList.begin();
    }

    if (itsGeometryId > 0)
    {
      GRIB2::GridDef_ptr def = Identification::gridDef.getGrib2DefinitionByGeometryId(itsGeometryId);
      if (def != nullptr)
      {
        T::Coordinate topLeft,topRight,bottomLeft,bottomRight;
        if (def->getGridLatLonArea(topLeft,topRight,bottomLeft,bottomRight))
        {
          geographicBoundingBox.xMin = std::min(topLeft.x(), bottomLeft.x());
          geographicBoundingBox.xMax = std::max(topRight.x(), bottomRight.x());
          geographicBoundingBox.yMin = std::min(bottomLeft.y(), bottomRight.y());
          geographicBoundingBox.yMax = std::max(topLeft.y(),topRight.y());
        }
      }
    }


    std::set<std::string> contentTimeList;
    if (contentServer->getContentTimeListByGenerationAndGeometryId(0,generationInfo->mGenerationId,itsGeometryId,contentTimeList) != 0)
      return;


    time_t step = even_timesteps(contentTimeList);
    if (step > 0)
    {
      // interval
      boost::posix_time::time_duration timestep = boost::posix_time::seconds(step);

      auto newTimeDimension = boost::movelib::make_unique<IntervalTimeDimension>(toTimeStamp(*(contentTimeList.begin())), toTimeStamp(*(--contentTimeList.end())),timestep);
      timeDimension = std::move(newTimeDimension);
    }
    else
    {
      // timesteps
      auto newTimeDimension = boost::movelib::make_unique<StepTimeDimension>();
      for (auto it = contentTimeList.begin(); it != contentTimeList.end(); ++it)
        newTimeDimension->addTimestep(toTimeStamp(*it));

      timeDimension = std::move(newTimeDimension);
    }
    metadataTimestamp = boost::posix_time::second_clock::universal_time();
  }
  catch (...)
  {
    throw Spine::Exception::Trace(BCP, "Failed to update querydata layer metadata!");
  }
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
