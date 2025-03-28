#include "WMSLayerHierarchy.h"
#include <boost/regex.hpp>
#include <macgyver/StringConversion.h>
#include <memory>
#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
namespace
{

// ----------------------------------------------------------------------
// HELPER FUNCTIONS FOR PROCESSING THE WMS LAYERS INTO A DATA STRUCTURE
// ----------------------------------------------------------------------

// Expand layer info into separate ones for each origintime if there are more than one
void expand_layer(WMSLayerHierarchy& lh)
{
  if (!lh.timeDimension)
    return;

  const std::shared_ptr<WMSTimeDimensions>& td = lh.timeDimension->getLayer()->getTimeDimensions();
  if (!td)
    return;

  const std::vector<Fmi::DateTime>& origintimes = td->getOrigintimes();

  if (origintimes.size() <= 1)
    return;

  // Create one layer for each origintime
  for (const auto& ot : origintimes)
  {
    lh.sublayers.push_back(boost::make_shared<WMSLayerHierarchy>(lh.name));
    WMSLayerHierarchy& reference_time_layer = *lh.sublayers.back();
    reference_time_layer.parent = &lh;
    reference_time_layer.baseInfoLayer = lh.baseInfoLayer;
    reference_time_layer.geographicBoundingBox = lh.geographicBoundingBox;
    reference_time_layer.projectedBoundingBox = lh.projectedBoundingBox;
    reference_time_layer.elevationDimension = lh.elevationDimension;
    reference_time_layer.timeDimension = lh.timeDimension;
    reference_time_layer.reference_time = ot;
  }
}

// Add sublayers recursively
void add_sublayers(WMSLayerHierarchy& lh,
                   const std::map<std::string, const WMSLayerProxy*>& named_layers,
                   std::set<std::string>& processed_layers,
                   WMSLayerHierarchy::HierarchyType hierarchy_type)
{
  if (!lh.reference_time)
  {
    std::string layer_name_prefix = lh.name + ":";

    auto pos = named_layers.lower_bound(layer_name_prefix);

    for (; pos != named_layers.end(); ++pos)
    {
      const auto& layer_name = pos->first;
      if (boost::algorithm::starts_with(layer_name, layer_name_prefix))
      {
        if (processed_layers.find(layer_name) != processed_layers.end())
          continue;
        processed_layers.insert(layer_name);
        lh.sublayers.push_back(boost::make_shared<WMSLayerHierarchy>(layer_name));
        lh.sublayers.back()->parent = &lh;
        add_sublayers(*lh.sublayers.back(), named_layers, processed_layers, hierarchy_type);
      }
      else
      {
        break;
      }
    }
  }

  // Leaf layers do not have sublayers
  if (lh.sublayers.empty())
  {
    processed_layers.insert(lh.name);
    const WMSLayerProxy& layer_to_use = *named_layers.at(lh.name);
    lh.baseInfoLayer = layer_to_use;
    lh.geographicBoundingBox = layer_to_use;
    lh.projectedBoundingBox = layer_to_use;
    lh.timeDimension = layer_to_use;
    lh.elevationDimension = layer_to_use;
    if (hierarchy_type == WMSLayerHierarchy::HierarchyType::recursivetimes)
      expand_layer(lh);
  }
}

// ----------------------------------------------------------------------
// HELPER FUNCTIONS FOR GENERATING A GETCAPABILITIES RESPONSE FROM THE INTERNAL DATA STRUCTURE
// ----------------------------------------------------------------------

/*
 * namespace patterns look like "/..../"
 */

bool looks_like_pattern(const std::string& pattern)
{
  return (boost::algorithm::starts_with(pattern, "/") && boost::algorithm::ends_with(pattern, "/"));
}

/*
 * Apply namespace filtering as in GeoServer with regex extension
 */

bool match_namespace_pattern(const std::string& name, const std::string& pattern)
{
  if (name == pattern)
    return true;

  if (!looks_like_pattern(pattern))
    return boost::algorithm::istarts_with(name, pattern + ":");

  // Strip surrounding slashes first
  const std::string re_str = pattern.substr(1, pattern.size() - 2);
  const boost::regex re(re_str, boost::regex::icase);

  return boost::regex_search(name, re);
}

// Test if two layer hierarchies have identical elements of the given type (bbox, dimension etc)
// TODO: What we actually should do is extract common elements to the root level

bool is_identical_geo_bbox(const WMSLayerHierarchy& lh1, const WMSLayerHierarchy& lh2)
{
  SharedWMSLayer layer1;
  SharedWMSLayer layer2;
  if (lh1.geographicBoundingBox)
    layer1 = lh1.geographicBoundingBox->getLayer();
  if (lh2.geographicBoundingBox)
    layer2 = lh2.geographicBoundingBox->getLayer();

  return ((layer1 && layer2 && layer1->identicalGeographicBoundingBox(*layer2)) ||
          (!layer1 && !layer2));
}

bool is_identical_proj_bbox(const WMSLayerHierarchy& lh1, const WMSLayerHierarchy& lh2)
{
  SharedWMSLayer layer1;
  SharedWMSLayer layer2;
  if (lh1.projectedBoundingBox)
    layer1 = lh1.projectedBoundingBox->getLayer();
  if (lh2.projectedBoundingBox)
    layer2 = lh2.projectedBoundingBox->getLayer();

  return ((layer1 && layer2 && layer1->identicalProjectedBoundingBox(*layer2)) ||
          (!layer1 && !layer2));
}

bool is_identical_time_dim(const WMSLayerHierarchy& lh1, const WMSLayerHierarchy& lh2)
{
  SharedWMSLayer layer1;
  SharedWMSLayer layer2;
  if (lh1.timeDimension)
    layer1 = lh1.timeDimension->getLayer();
  if (lh2.timeDimension)
    layer2 = lh2.timeDimension->getLayer();

  return ((layer1 && layer2 && layer1->identicalTimeDimension(*layer2)) || (!layer1 && !layer2));
}

bool is_identical_elev_dim(const WMSLayerHierarchy& lh1, const WMSLayerHierarchy& lh2)
{
  SharedWMSLayer layer1;
  SharedWMSLayer layer2;
  if (lh1.elevationDimension)
    layer1 = lh1.elevationDimension->getLayer();
  if (lh2.elevationDimension)
    layer2 = lh2.elevationDimension->getLayer();

  return ((layer1 && layer2 && layer1->identicalElevationDimension(*layer2)) ||
          (!layer1 && !layer2));
}

bool is_identical(const WMSLayerHierarchy& lh1,
                  const WMSLayerHierarchy& lh2,
                  WMSLayerHierarchy::ElementType type)
{
  switch (type)
  {
    case WMSLayerHierarchy::ElementType::geo_bbox:
      return is_identical_geo_bbox(lh1, lh2);
    case WMSLayerHierarchy::ElementType::proj_bbox:
      return is_identical_proj_bbox(lh1, lh2);
    case WMSLayerHierarchy::ElementType::time_dim:
      return is_identical_time_dim(lh1, lh2);
    case WMSLayerHierarchy::ElementType::elev_dim:
      return is_identical_elev_dim(lh1, lh2);
  }
  return false;
}

std::list<const WMSLayerHierarchy*> get_leaf_layers(const WMSLayerHierarchy& lh)
{
  std::list<const WMSLayerHierarchy*> leaf_layers;

  if (lh.sublayers.empty())
    leaf_layers.push_back(&lh);
  else
  {
    for (const auto& item : lh.sublayers)
    {
      std::list<const WMSLayerHierarchy*> list_to_add = get_leaf_layers(*item);
      if (!list_to_add.empty())
        leaf_layers.insert(leaf_layers.end(), list_to_add.begin(), list_to_add.end());
    }
  }

  return leaf_layers;
}

void set_layer_elements(WMSLayerHierarchy& lh)
{
  std::list<const WMSLayerHierarchy*> leaf_layers = get_leaf_layers(lh);

  if (leaf_layers.empty())
    return;

  const WMSLayerHierarchy& first_item = *leaf_layers.front();
  bool identicalGeographicBoundingBox = true;
  bool identicalProjectedBoundingBox = true;
  bool identicalTimeDimension = true;
  bool identicalElevationDimension = true;

  for (const auto& item : leaf_layers)
  {
    if (!is_identical(first_item, *item, WMSLayerHierarchy::ElementType::geo_bbox))
      identicalGeographicBoundingBox = false;
    if (!is_identical(first_item, *item, WMSLayerHierarchy::ElementType::proj_bbox))
      identicalProjectedBoundingBox = false;
    if (!is_identical(first_item, *item, WMSLayerHierarchy::ElementType::time_dim))
      identicalTimeDimension = false;
    if (!is_identical(first_item, *item, WMSLayerHierarchy::ElementType::elev_dim))
      identicalElevationDimension = false;
  }

  if (identicalGeographicBoundingBox)
    lh.geographicBoundingBox = first_item.geographicBoundingBox;
  if (identicalProjectedBoundingBox)
    lh.projectedBoundingBox = first_item.projectedBoundingBox;
  if (identicalTimeDimension)
    lh.timeDimension = first_item.timeDimension;
  if (identicalElevationDimension)
    lh.elevationDimension = first_item.elevationDimension;

  for (const auto& item : lh.sublayers)
    set_layer_elements(*item);
}

void add_layer_info(bool multiple_intervals,
                    CTPP::CDT& ctpp,
                    const WMSLayerHierarchy& lh,
                    const std::string& language,
                    const std::string& defaultLanguage,
                    const std::optional<std::string>& starttime,
                    const std::optional<std::string>& endtime,
                    const std::optional<std::string>& reference_time,
                    unsigned int level)
{
  CTPP::CDT capa(CTPP::CDT::HASH_VAL);

  bool sublayer_is_reference_time_layer =
      (!lh.sublayers.empty() && lh.sublayers.back()->reference_time);
  std::optional<CTPP::CDT> baseInfo;
  if (lh.sublayers.empty() && lh.baseInfoLayer)
  {
    baseInfo = lh.baseInfoLayer->getLayer()->getLayerBaseInfo(language, defaultLanguage);
    std::string name = (*baseInfo)["name"].GetString();
    std::string title = (*baseInfo)["title"].GetString();
    if (lh.reference_time)
    {
      std::string ref_time = lh.reference_time->to_iso_string();
      name += (":origintime_" + ref_time);
      title += (" origintime " + ref_time);
      (*baseInfo)["name"] = name;
      (*baseInfo)["title"] = title;
    }
  }

  if (baseInfo)
  {
    capa = *baseInfo;
    std::optional<CTPP::CDT> styles =
        lh.baseInfoLayer->getLayer()->getStyleInfo(language, defaultLanguage);
    if (styles)
      capa.MergeCDT(*styles);
  }
  else
  {
    // From spec: "If the layer has a Title but no Name, then that layer is only a category title
    // for all the layers nested within."
    capa["title"] = lh.name;
    capa["queryable"] = 0;
  }

  if (lh.geographicBoundingBox)
  {
    if (!lh.parent || !is_identical(lh, *lh.parent, WMSLayerHierarchy::ElementType::geo_bbox))
    {
      // Add supported geographic bounding box
      std::optional<CTPP::CDT> geo_bbox =
          lh.geographicBoundingBox->getLayer()->getGeographicBoundingBoxInfo();
      if (geo_bbox)
        capa.MergeCDT(*geo_bbox);
    }
  }
  if (lh.projectedBoundingBox)
  {
    if (!lh.parent || !is_identical(lh, *lh.parent, WMSLayerHierarchy::ElementType::proj_bbox))
    {
      // Add supported CRSs
      std::optional<CTPP::CDT> projected_bbox =
          lh.projectedBoundingBox->getLayer()->getProjectedBoundingBoxInfo();
      if (projected_bbox)
        capa.MergeCDT(*projected_bbox);
    }
  }

  if (lh.timeDimension)
  {
    // Add supported time dimension
    std::optional<CTPP::CDT> time_dim;
    auto wmslayer = lh.timeDimension->getLayer();
    if (sublayer_is_reference_time_layer)
    {
      // If sublayers are reference time layers, parent shows only referece time elemenent
      time_dim = wmslayer->getReferenceDimensionInfo();
    }
    else if (lh.reference_time)
    {
      // Reference time layer
      std::string ref_time = Fmi::to_iso_string(*lh.reference_time);
      time_dim = wmslayer->getTimeDimensionInfo(multiple_intervals, starttime, endtime, ref_time);
    }
    else
    {
      if (!lh.parent || !is_identical(lh, *lh.parent, WMSLayerHierarchy::ElementType::time_dim))
        time_dim =
            wmslayer->getTimeDimensionInfo(multiple_intervals, starttime, endtime, reference_time);
    }

    if (time_dim)
    {
      capa.MergeCDT(*time_dim);
      auto interval_dim = wmslayer->getIntervalDimensionInfo();
      if (interval_dim)
        capa.MergeCDT(*interval_dim);
    }
  }

  if (lh.elevationDimension)
  {
    if (!lh.parent || !is_identical(lh, *lh.parent, WMSLayerHierarchy::ElementType::elev_dim))
    {
      // Add supported elevation dimension
      std::optional<CTPP::CDT> elev_dim =
          lh.elevationDimension->getLayer()->getElevationDimensionInfo();
      if (elev_dim)
        capa.MergeCDT(*elev_dim);
    }
  }

  if (lh.sublayers.empty())
    ctpp.PushBack(capa);
  else
  {
    // Add sublayers
    capa["sublayers"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
    ctpp.PushBack(capa);
    unsigned int nextLevel = level + 1;
    for (const auto& item : lh.sublayers)
      add_layer_info(multiple_intervals,
                     capa["sublayers"],
                     *item,
                     language,
                     defaultLanguage,
                     starttime,
                     endtime,
                     reference_time,
                     nextLevel);
  }
}

}  // namespace

// ----------------------------------------------------------------------
// CONSTRUCTING A HIERARCHY
// ----------------------------------------------------------------------

WMSLayerHierarchy::WMSLayerHierarchy(std::string n) : name(std::move(n)), parent(nullptr) {}

#ifndef WITHOUT_AUTHENTICATION
WMSLayerHierarchy::WMSLayerHierarchy(const std::map<std::string, WMSLayerProxy>& layerMap,
                                     const std::optional<std::string>& wms_namespace,
                                     HierarchyType hierarchy_type,
                                     bool reveal_hidden,
                                     const std::optional<std::string>& apikey,
                                     bool auth,
                                     Engine::Authentication::Engine* authEngine)
    : name("__root__"), show_hidden(reveal_hidden), authenticate(auth)
#else
WMSLayerHierarchy::WMSLayerHierarchy(const std::map<std::string, WMSLayerProxy>& layerMap,
                                     const std::optional<std::string>& wms_namespace,
                                     HierarchyType hierarchy_type,
                                     bool reveal_hidden)
    : name("__root__"), show_hidden(reveal_hidden), authenticate(false)
#endif
{
#ifndef WITHOUT_AUTHENTICATION
  processLayers(layerMap, wms_namespace, hierarchy_type, apikey, authEngine);
#else
  processLayers(layerMap, wms_namespace, hierarchy_type);
#endif
}

#ifndef WITHOUT_AUTHENTICATION
void WMSLayerHierarchy::processLayers(const std::map<std::string, WMSLayerProxy>& layerMap,
                                      const std::optional<std::string>& wms_namespace,
                                      HierarchyType hierarchy_type,
                                      const std::optional<std::string>& apikey,
                                      Engine::Authentication::Engine* authEngine)
#else
void WMSLayerHierarchy::processLayers(const std::map<std::string, WMSLayerProxy>& layerMap,
                                      const std::optional<std::string>& wms_namespace,
                                      HierarchyType hierarchy_type)
#endif
{
  std::map<std::string, const WMSLayerProxy*> named_layers;

  std::set<std::string> top_level_layers;
  for (const auto& item : layerMap)
  {
    const auto& layer_name = item.first;

#ifndef WITHOUT_AUTHENTICATION
    // If authentication is requested, skip the layer if authentication fails
    const std::string wmsService = "wms";
    if (apikey && authenticate)
      if (authEngine == nullptr || !authEngine->authorize(*apikey, layer_name, wmsService))
        continue;
#endif
    // Skip hidden layers
    if (item.second.getLayer()->isHidden() && !show_hidden)
      continue;

    if (wms_namespace && !match_namespace_pattern(layer_name, *wms_namespace))
      continue;

    if (hierarchy_type == HierarchyType::flat)
    {
      auto hierarchy = boost::make_shared<WMSLayerHierarchy>(layer_name);
      const WMSLayerProxy& layer_to_use = item.second;
      hierarchy->baseInfoLayer = layer_to_use;
      hierarchy->geographicBoundingBox = layer_to_use;
      hierarchy->projectedBoundingBox = layer_to_use;
      hierarchy->timeDimension = layer_to_use;
      hierarchy->elevationDimension = layer_to_use;
      sublayers.push_back(hierarchy);
    }
    else
    {
      // Sort out name hierarchy
      std::vector<std::string> tokens;
      boost::algorithm::split(tokens, layer_name, boost::is_any_of(":"), boost::token_compress_on);

      std::string name;
      for (const auto& token_item : tokens)
      {
        if (!name.empty())
          name += ":";
        name += token_item;
        named_layers[name] = &item.second;

        if (name.find(':') == std::string::npos)
          top_level_layers.insert(name);
      }
    }
  }

  if (hierarchy_type == HierarchyType::flat)
    return;

  std::set<std::string> processed_layers;
  for (const auto& name : top_level_layers)
  {
    // Add top layer and its sublayers
    auto hierarchy = boost::make_shared<WMSLayerHierarchy>(name);
    add_sublayers(*hierarchy, named_layers, processed_layers, hierarchy_type);
    set_layer_elements(*hierarchy);
    sublayers.push_back(hierarchy);
  }
}

// ----------------------------------------------------------------------
// GENERATING GETCAPABILITIES
// ----------------------------------------------------------------------

CTPP::CDT WMSLayerHierarchy::getCapabilities(bool multiple_intervals,
                                             const std::string& language,
                                             const std::string& defaultLanguage,
                                             const std::optional<std::string>& starttime,
                                             const std::optional<std::string>& endtime,
                                             const std::optional<std::string>& reference_time) const
{
  // Return array of individual layer capabilities
  CTPP::CDT capa(CTPP::CDT::ARRAY_VAL);

  // std::cout << "CAPA:\n " << *this << std::endl;
  add_layer_info(multiple_intervals,
                 capa,
                 *this,
                 language,
                 defaultLanguage,
                 starttime,
                 endtime,
                 reference_time,
                 1);

  return capa;
}

// ----------------------------------------------------------------------
// HELPER FUNCTIONS FOR DEBUGGING
// ----------------------------------------------------------------------

void printHierarchy(const std::string& prefix,
                    unsigned int level,
                    std::ostream& ost,
                    const WMSLayerHierarchy& lh)
{
  ost << prefix << lh.name << "(L#" << level << (lh.sublayers.empty() ? "-Leaf)" : ") ")
      << (lh.geographicBoundingBox ? "1" : "0") << (lh.projectedBoundingBox ? "1" : "0")
      << (lh.timeDimension ? "1" : "0") << (lh.elevationDimension ? "1" : "0") << std::endl;

  unsigned int nextLevel = level + 1;
  for (const auto& item : lh.sublayers)
    printHierarchy(prefix + " ", nextLevel, ost, *item);
}

std::ostream& operator<<(std::ostream& ost, const WMSLayerHierarchy& lh)
{
  printHierarchy("", 1, ost, lh);

  return ost;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
