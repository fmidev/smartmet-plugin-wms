#include "GridDataLayer.h"
#include <grid-files/common/GeneralFunctions.h>
#include <grid-files/identification/GridDef.h>
#include <macgyver/Exception.h>
#include <vector>

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
// Grid content query settings of the layer
struct GridQuery
{
  T::ParamKeyType parameterKeyType = T::ParamKeyTypeValue::FMI_NAME;
  std::string parameterKey;
  T::ParamLevelId parameterLevelId = 0;
  T::ParamLevel minLevel = 0;
  T::ParamLevel maxLevel = 1000000000;
  T::ForecastType forecastType = 0;
  T::ForecastNumber forecastNumber = -1;
  std::string startTime = "19000101T000000";
  std::string endTime = "23000101T000000";
  std::string producer;
  std::set<int> functionGeometries;
  std::set<std::string> producers;
};

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

        auto since_midnight = (t - midnight).total_minutes();
        return (since_midnight % *timestep) != 0;
    });
  }

// Select the function parameter to use for the metadata, collecting the geometries and the
// other producers used by the function
std::string select_function_parameter(const char* startpoint, GridQuery& query)
{
  std::string fparam;
  std::string lastParam;

  // Function might have several parameters.

  std::vector<std::string> fp;
  splitString(startpoint, ';', fp);

  for (auto& it : fp)
  {
    const char* ps = it.c_str();

    if (ps[0] >= 'A')
      lastParam = it;

    if (query.producer.empty())
      continue;

    // The produce is not empty. Let's try to find a parameter with the same producer.

    std::vector<std::string> p;
    splitString(ps, ':', p);

    if (p.size() >= 3 && !p[2].empty())
    {
      query.functionGeometries.insert(toInt32(p[2]));
    }

    if (p.size() >= 2)
    {
      if (strcasecmp(p[1].c_str(), query.producer.c_str()) == 0)
      {
        fparam = it;
      }
      else
      {
        // Parameter/Function uses a producer that is not the main producer. This
        // might cause problems if all producers do not have same reference times.

        query.producers.insert(p[1]);
      }
    }
  }

  if (fparam.empty())
  {
    // No parameter with the given producer found. Let's use the last parameter.
    fparam = lastParam;
  }
  return fparam;
}

// Set the query settings from a parameter string
void apply_parameter_string(const std::string& param, GridQuery& query, int& geometryId)
{
  std::vector<std::string> p;
  splitString(param, ':', p);

  query.parameterKey = p[0];

  if (p.size() >= 2 && !p[1].empty())
    query.producer = p[1];

  if (p.size() >= 3 && !p[2].empty() && query.functionGeometries.size() < 2)
    geometryId = toInt32(p[2]);

  if (p.size() >= 4 && !p[3].empty())
    query.parameterLevelId = toInt32(p[3]);

  if (p.size() >= 5 && !p[4].empty())
  {
    query.minLevel = toInt32(p[4]);
    query.maxLevel = query.minLevel;
  }

  if (p.size() >= 6 && !p[5].empty())
    query.forecastType = toInt32(p[5]);

  if (p.size() >= 7 && !p[6].empty())
    query.forecastNumber = toInt32(p[6]);
}

// Finding parameter information (it might be inside a function)
void parse_parameter(const Engine::Grid::Engine& gridEngine,
                     const std::string& parameter,
                     GridQuery& query,
                     int& geometryId)
{
  // The parameter may be overridden in the request, hence no fixed size buffer
  std::vector<char> bufVec(parameter.begin(), parameter.end());
  bufVec.push_back('\0');
  char* buf = bufVec.data();
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

  std::string fparam = select_function_parameter(startpoint, query);

  std::string param = gridEngine.getParameterString(query.producer, fparam);
  apply_parameter_string(param, query, geometryId);
}

// Checking if the producer have the same reference times as the main producer.
bool same_reference_times(T::GenerationInfoList& generationInfoList,
                          T::GenerationInfoList& genInfoList)
{
  if (genInfoList.getLength() != generationInfoList.getLength())
    return false;

  bool refTimesAreEqual = true;
  for (unsigned int i = 0; i < generationInfoList.getLength(); i++)
  {
    T::GenerationInfo* g1 = generationInfoList.getGenerationInfoByIndex(i);
    if (refTimesAreEqual && !genInfoList.getGenerationInfoByAnalysisTime(g1->mAnalysisTime))
      refTimesAreEqual = false;
  }
  return refTimesAreEqual;
}

// A parameter (=> function) contains more than one producer. If producers
// do not have same reference_times then we should accept only the latest
// reference time.
void select_latest_generation(ContentServer::ServiceInterface& contentServer,
                              const std::set<std::string>& producers,
                              T::GenerationInfoList& generationInfoList,
                              T::GenerationInfoList& newGenerationInfoList)
{
  T::GenerationInfo* newGen = nullptr;
  for (const auto& producer : producers)
  {
    T::ProducerInfo prodInfo;
    if (contentServer.getProducerInfoByName(0, producer, prodInfo) != 0)
      continue;

    T::GenerationInfoList genInfoList;
    if (contentServer.getGenerationInfoListByProducerId(0, prodInfo.mProducerId, genInfoList) !=
            0 ||
        generationInfoList.getLength() <= 0)
      continue;

    if (same_reference_times(generationInfoList, genInfoList))
      continue;

    // Producers do not have same reference times. So, we should use only the latest
    // reference time.

    T::GenerationInfo* lastGen1 = generationInfoList.getLastGenerationInfoByAnalysisTime(1);

    if (!lastGen1)
      continue;

    if (!newGen)
    {
      newGen = lastGen1->duplicate();
      newGenerationInfoList.addGenerationInfo(newGen);
    }

    auto* lastGen2 = genInfoList.getLastGenerationInfoByAnalysisTime(1);
    if (lastGen2 && newGen->mAnalysisTime < lastGen2->mAnalysisTime)
      newGen->mAnalysisTime = lastGen2->mAnalysisTime;
  }
}

// Update the bounding box from the grid geometry
void update_bounding_box(int geometryId, Spine::BoundingBox& geographicBoundingBox)
{
  if (geometryId <= 0)
    return;

  auto def = Identification::gridDef.getGrib2DefinitionByGeometryId(geometryId);
  if (!def)
    return;

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

// Parameter name can be an alias name. Trying to find it from the parameter mappings.
void apply_parameter_mapping(const Engine::Grid::Engine& gridEngine,
                             const std::string& producerName,
                             int geometryId,
                             GridQuery& query)
{
  QueryServer::ParameterMapping_vec mappings;
  gridEngine.getParameterMappings(producerName,
                                  query.parameterKey,
                                  geometryId,
                                  query.parameterLevelId,
                                  query.minLevel,
                                  false,
                                  mappings);

  if (mappings.empty())
  {
    // Trying to find parameter mappings without levels.

    gridEngine.getParameterMappings(producerName, query.parameterKey, geometryId, true, mappings);

    if (mappings.empty())
    {
      // Trying to find parameter mappings without geometry.
      gridEngine.getParameterMappings(producerName, query.parameterKey, true, mappings);
    }
  }

  if (!mappings.empty())
  {
    query.parameterKey = mappings[0].mParameterKey;
    query.parameterLevelId = mappings[0].mParameterLevelId;
  }
}

// Fetch the content list of the parameter, returns false on failure
bool get_content_list(ContentServer::ServiceInterface& contentServer,
                      const T::GenerationInfo& generationInfo,
                      const GridQuery& query,
                      int geometryId,
                      T::ContentInfoList& contentInfoList)
{
  return (contentServer.getContentListByParameterAndGenerationId(0,
                                                                 generationInfo.mGenerationId,
                                                                 query.parameterKeyType,
                                                                 query.parameterKey,
                                                                 query.parameterLevelId,
                                                                 query.minLevel,
                                                                 query.maxLevel,
                                                                 query.forecastType,
                                                                 query.forecastNumber,
                                                                 geometryId,
                                                                 query.startTime,
                                                                 query.endTime,
                                                                 0,
                                                                 contentInfoList) == 0);
}

// Picking content times:
void pick_content_times(T::ContentInfoList& contentInfoList,
                        uint len,
                        const T::GenerationInfo& generationInfo,
                        const std::set<int>& validGeometries,
                        std::set<std::string>& contentTimeList)
{
  for (uint t = 0; t < len; t++)
  {
    T::ContentInfo* info = contentInfoList.getContentInfoByIndex(t);
    if (validGeometries.find(info->mGeometryId) == validGeometries.end())
      continue;

    // We should not accept forecast times that are earlier than the reference time.

    if (generationInfo.mAnalysisTime <= info->getForecastTime())
    {
      if (contentTimeList.find(info->getForecastTime()) == contentTimeList.end())
        contentTimeList.insert(std::string(info->getForecastTime()));
    }
  }
}

// Picking levels:
std::set<int> pick_levels(T::ContentInfoList& contentInfoList,
                          uint len,
                          const std::string& elevationUnit,
                          T::ParamLevelId parameterLevelId)
{
  std::set<int> levelList;
  int step = 0;
  if (elevationUnit == "m")
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
  return levelList;
}

// Time dimension of the content times
std::shared_ptr<TimeDimension> make_time_dimension(const std::set<std::string>& contentTimeList,
                                                   std::optional<int> timestep)
{
  std::shared_ptr<TimeDimension> timeDimension;

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

  return timeDimension;
}

// Add elevation information if there is more than one level.
void add_elevation_dimension(std::shared_ptr<ElevationDimension>& elevationDimension,
                             T::ParamLevelId parameterLevelId,
                             const std::string& elevationUnit,
                             const std::set<int>& levelList)
{
  if (elevationDimension || levelList.size() <= 1)
    return;

  Identification::LevelDef levelDef;
  if (Identification::gridDef.getFmiLevelDef(parameterLevelId, levelDef))
  {
    std::string unit_symbol = elevationUnit;
    if (unit_symbol.empty())
      unit_symbol = levelDef.mUnits;

    elevationDimension = std::make_shared<ElevationDimension>(
        levelDef.mName, levelDef.mLevelId, unit_symbol, levelList);
  }
}

// Valid geometries of the producer geometry group, or of the function parameters
std::set<int> valid_geometries(const std::string& producerName,
                               const std::set<int>& functionGeometries,
                               int& geometryId)
{
  std::set<int> validGeometries;
  Identification::FmiGeometryGroupDef geometryGroupDef;
  if (Identification::gridDef.getFmiGeometryGroupDef(producerName.c_str(), 1, geometryGroupDef))
  {
    for (int& aIt : geometryGroupDef.mGeometryIdList)
    {
      validGeometries.insert(aIt);
    }
    geometryId = -1;
  }

  if (validGeometries.empty() && !functionGeometries.empty())
    validGeometries = functionGeometries;

  return validGeometries;
}

enum class GeometryStatus
{
  Ok,
  Missing,
  Failed
};

// Select the first geometry of the generation
GeometryStatus select_generation_geometry(ContentServer::ServiceInterface& contentServer,
                                          const T::GenerationInfo& generationInfo,
                                          int& geometryId)
{
  std::set<T::GeometryId> geometryIdList;
  if (contentServer.getContentGeometryIdListByGenerationId(
          0, generationInfo.mGenerationId, geometryIdList) != 0)
    return GeometryStatus::Failed;

  if (geometryIdList.empty())
    return GeometryStatus::Missing;

  geometryId = *geometryIdList.begin();
  return GeometryStatus::Ok;
}

// Select the geometries to use for the generation
GeometryStatus select_geometry(ContentServer::ServiceInterface& contentServer,
                               const T::GenerationInfo& generationInfo,
                               int& geometryId,
                               std::set<int>& validGeometries,
                               int& tmpGeometryId)
{
  if (geometryId <= 0 && validGeometries.empty())
  {
    const auto status = select_generation_geometry(contentServer, generationInfo, geometryId);
    if (status != GeometryStatus::Ok)
      return status;
    validGeometries.insert(geometryId);
  }

  if (validGeometries.empty())
    validGeometries.insert(geometryId);

  tmpGeometryId = geometryId;
  if (tmpGeometryId <= 0 && !validGeometries.empty())
    tmpGeometryId = *validGeometries.begin();

  return GeometryStatus::Ok;
}

// Add the time dimension of the generation
void add_time_dimension(std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>>& newTimeDimensions,
                        const std::set<std::string>& contentTimeList,
                        const T::GenerationInfo& generationInfo,
                        std::optional<int> timestep)
{
  auto timeDimension = make_time_dimension(contentTimeList, timestep);

  if (timeDimension)
    newTimeDimensions.insert(
        std::make_pair(toTimeStamp(generationInfo.mAnalysisTime), timeDimension));
}
}  // namespace

GridDataLayer::GridDataLayer(const LayerConfig& config,
                                   std::string producer,
                                   std::string parameter,
                                   int forecastType,
                                   uint geometryId,
                                   int levelId,
                                   std::string elevation_unit)
    : Layer(config),
      itsGridEngine(config.gridEngine()),
      itsProducer(std::move(producer)),
      itsParameter(std::move(parameter)),
      itsForecastType(forecastType),
      itsGeometryId(geometryId),
      itsLevelId(levelId),
      itsElevationUnit(std::move(elevation_unit))
{
}

// ----------------------------------------------------------------------
/*!
 * \brief Collect the content times and update the elevation dimension of the parameter
 *
 * Returns false if the content server query fails.
 */
// ----------------------------------------------------------------------

bool GridDataLayer::getContentTimes(ContentServer::ServiceInterface& contentServer,
                                    const T::GenerationInfo& generationInfo,
                                    GridQuery& query,
                                    const std::string& producerName,
                                    int tmpGeometryId,
                                    const std::set<int>& validGeometries,
                                    std::set<std::string>& contentTimeList)
{
  if (!(itsParameter > ""))
    return (contentServer.getContentTimeListByGenerationAndGeometryId(
                0, generationInfo.mGenerationId, itsGeometryId, contentTimeList) == 0);

  // Trying to find content records with the given parameter name.

  T::ContentInfoList contentInfoList;
  if (!get_content_list(contentServer, generationInfo, query, itsGeometryId, contentInfoList))
    return false;

  uint len = contentInfoList.getLength();
  if (len == 0)
  {
    apply_parameter_mapping(*itsGridEngine, producerName, tmpGeometryId, query);

    if (!get_content_list(contentServer, generationInfo, query, itsGeometryId, contentInfoList))
      return false;

    len = contentInfoList.getLength();
  }

  pick_content_times(contentInfoList, len, generationInfo, validGeometries, contentTimeList);

  const std::set<int> levelList =
      pick_levels(contentInfoList, len, itsElevationUnit, query.parameterLevelId);

  add_elevation_dimension(elevationDimension, query.parameterLevelId, itsElevationUnit, levelList);
  return true;
}

bool GridDataLayer::updateLayerMetaData()
{
  try
  {
    if (!itsGridEngine || !itsGridEngine->isEnabled())
      return false;

    auto contentServer = itsGridEngine->getContentServer_sptr();

    // printf("WMSPARAM %s %s  %d\n",itsProducer.c_str(),itsParameter.c_str(),itsGeometryId);

    GridQuery query;
    query.parameterKey = itsParameter;
    query.parameterLevelId = itsLevelId;
    query.forecastType = itsForecastType;
    query.producer = itsProducer;

    if (!itsParameter.empty())
      parse_parameter(*itsGridEngine, itsParameter, query, itsGeometryId);

    T::ProducerInfo producerInfo;
    if (contentServer->getProducerInfoByName(0, query.producer, producerInfo) != 0)
      return true;

    T::GenerationInfoList generationInfoList;
    if (contentServer->getGenerationInfoListByProducerId(
            0, producerInfo.mProducerId, generationInfoList) != 0 ||
        generationInfoList.getLength() == 0)
      return true;

    T::GenerationInfoList newGenerationInfoList;
    if (!query.producers.empty())
      select_latest_generation(
          *contentServer, query.producers, generationInfoList, newGenerationInfoList);

    if (newGenerationInfoList.getLength() == 0)
      newGenerationInfoList = generationInfoList;

    std::set<int> validGeometries =
        valid_geometries(producerInfo.mName, query.functionGeometries, itsGeometryId);

    std::map<Fmi::DateTime, std::shared_ptr<TimeDimension>> newTimeDimensions;
    for (unsigned int i = 0; i < newGenerationInfoList.getLength(); i++)
    {
      T::GenerationInfo* generationInfo = newGenerationInfoList.getGenerationInfoByIndex(i);

      // generation must exist and be marked complete (1)
      if (generationInfo == nullptr || generationInfo->mStatus != 1)
        continue;

      int tmpGeometryId = 0;
      const auto status = select_geometry(
          *contentServer, *generationInfo, itsGeometryId, validGeometries, tmpGeometryId);
      if (status == GeometryStatus::Failed)
        return true;
      if (status == GeometryStatus::Missing)
        continue;

      update_bounding_box(tmpGeometryId, geographicBoundingBox);

      std::set<std::string> contentTimeList;

      if (!getContentTimes(*contentServer,
                           *generationInfo,
                           query,
                           producerInfo.mName,
                           tmpGeometryId,
                           validGeometries,
                           contentTimeList))
        return true;

      if (!contentTimeList.empty())
        add_time_dimension(newTimeDimensions, contentTimeList, *generationInfo, timestep);
    }

    if (!newTimeDimensions.empty())
      timeDimensions = std::make_shared<TimeDimensions>(newTimeDimensions);
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

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
