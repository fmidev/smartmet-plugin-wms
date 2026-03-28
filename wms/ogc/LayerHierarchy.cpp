#include "LayerHierarchy.h"
#include <boost/make_shared.hpp>
#include <boost/regex.hpp>
#include <macgyver/StringConversion.h>
#include <cmath>
#include <limits>
#include <memory>
#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif

namespace SmartMet
{
namespace Plugin
{
namespace OGC
{
namespace
{

// ----------------------------------------------------------------------
// DEBUGGING
// ----------------------------------------------------------------------

void printHierarchy(const std::string& prefix,
                    unsigned int level,
                    std::ostream& ost,
                    const LayerHierarchy& lh)
{
  ost << prefix << lh.name << "(L#" << level << (lh.sublayers.empty() ? "-Leaf)" : ") ")
      << (lh.geographicBoundingBox ? "1" : "0") << (lh.projectedBoundingBox ? "1" : "0")
      << (lh.timeDimension ? "1" : "0") << (lh.elevationDimension ? "1" : "0") << '\n';
  for (const auto& item : lh.sublayers)
    printHierarchy(prefix + " ", level + 1, ost, *item);
}

// ----------------------------------------------------------------------
// LAYER TREE CONSTRUCTION
// ----------------------------------------------------------------------

void expand_layer(LayerHierarchy& lh)
{
  if (!lh.timeDimension)
    return;
  const auto& td = lh.timeDimension->getLayer()->getTimeDimensions();
  if (!td)
    return;
  const auto& origintimes = td->getOrigintimes();
  if (origintimes.size() <= 1)
    return;
  for (const auto& ot : origintimes)
  {
    lh.sublayers.push_back(boost::make_shared<LayerHierarchy>(lh.name));
    auto& rt = *lh.sublayers.back();
    rt.parent = &lh;
    rt.baseInfoLayer = lh.baseInfoLayer;
    rt.geographicBoundingBox = lh.geographicBoundingBox;
    rt.projectedBoundingBox = lh.projectedBoundingBox;
    rt.elevationDimension = lh.elevationDimension;
    rt.timeDimension = lh.timeDimension;
    rt.reference_time = ot;
  }
}

void add_sublayers(LayerHierarchy& lh,
                   const std::map<std::string, const LayerProxy*>& named_layers,
                   std::set<std::string>& processed_layers,
                   LayerHierarchy::HierarchyType hierarchy_type)
{
  if (!lh.reference_time)
  {
    std::string pfx = lh.name + ":";
    for (auto pos = named_layers.lower_bound(pfx); pos != named_layers.end(); ++pos)
    {
      if (!boost::algorithm::starts_with(pos->first, pfx))
        break;
      if (processed_layers.count(pos->first))
        continue;
      processed_layers.insert(pos->first);
      lh.sublayers.push_back(boost::make_shared<LayerHierarchy>(pos->first));
      lh.sublayers.back()->parent = &lh;
      add_sublayers(*lh.sublayers.back(), named_layers, processed_layers, hierarchy_type);
    }
  }
  if (lh.sublayers.empty())
  {
    processed_layers.insert(lh.name);
    const LayerProxy& p = *named_layers.at(lh.name);
    lh.baseInfoLayer = p;
    lh.geographicBoundingBox = p;
    lh.projectedBoundingBox = p;
    lh.timeDimension = p;
    lh.elevationDimension = p;
    if (hierarchy_type == LayerHierarchy::HierarchyType::recursivetimes)
      expand_layer(lh);
  }
}

// ----------------------------------------------------------------------
// NAMESPACE FILTERING
// ----------------------------------------------------------------------

bool looks_like_pattern(const std::string& s)
{
  return boost::algorithm::starts_with(s, "/") && boost::algorithm::ends_with(s, "/");
}
bool match_namespace_pattern(const std::string& name, const std::string& pattern)
{
  if (name == pattern)
    return true;
  if (!looks_like_pattern(pattern))
    return boost::algorithm::istarts_with(name, pattern + ":");
  const std::string re_str = pattern.substr(1, pattern.size() - 2);
  return boost::regex_search(name, boost::regex(re_str, boost::regex::icase));
}

// ----------------------------------------------------------------------
// LEAF COLLECTION
// ----------------------------------------------------------------------

std::list<const LayerHierarchy*> get_leaf_layers(const LayerHierarchy& lh)
{
  std::list<const LayerHierarchy*> out;
  if (lh.sublayers.empty())
  {
    out.push_back(&lh);
    return out;
  }
  for (const auto& c : lh.sublayers)
  {
    auto sub = get_leaf_layers(*c);
    out.insert(out.end(), sub.begin(), sub.end());
  }
  return out;
}

// ----------------------------------------------------------------------
// LAYER ATTRIBUTE KEYS
//
// Each "key" is a canonical string representation of one layer attribute
// (geo bbox, time dim, elev dim, projected bbox).  Equal strings mean
// equal values.  We use these strings in the "already declared" search
// tree (InheritedState) to decide whether to suppress output.
// ----------------------------------------------------------------------

constexpr double EPS = 1e-9;

std::string geo_key(const LayerHierarchy& lh)
{
  if (!lh.geographicBoundingBox)
    return "";
  auto info = lh.geographicBoundingBox->getLayer()->getGeographicBoundingBoxInfo();
  if (!info || !info->Exists("ex_geographic_bounding_box"))
    return "";
  auto& bb = (*info)["ex_geographic_bounding_box"];
  auto r = [](double v) { return std::round(v * 1e9) / 1e9; };
  return std::to_string(r(bb["west_bound_longitude"].GetFloat())) + "," +
         std::to_string(r(bb["east_bound_longitude"].GetFloat())) + "," +
         std::to_string(r(bb["south_bound_latitude"].GetFloat())) + "," +
         std::to_string(r(bb["north_bound_latitude"].GetFloat()));
}

std::string time_key(const LayerHierarchy& lh)
{
  if (!lh.timeDimension)
    return "";
  auto info = lh.timeDimension->getLayer()->getTimeDimensionInfo(false, {}, {}, {});
  if (!info || !info->Exists("time_dimension"))
    return "";
  // Use the raw CTPP dump as a stable key
  return (*info)["time_dimension"].RecursiveDump();
}

std::string elev_key(const LayerHierarchy& lh)
{
  if (!lh.elevationDimension)
    return "";
  auto info = lh.elevationDimension->getLayer()->getElevationDimensionInfo();
  if (!info || !info->Exists("elevation_dimension"))
    return "";
  return (*info)["elevation_dimension"].RecursiveDump();
}

// For projected bbox, produce a map crs_id -> "minx,miny,maxx,maxy"
// so we can suppress individual CRS entries already declared above.
using ProjMap = std::map<std::string, std::string>;

ProjMap proj_map(const LayerHierarchy& lh)
{
  ProjMap out;
  if (!lh.projectedBoundingBox)
    return out;
  auto info = lh.projectedBoundingBox->getLayer()->getProjectedBoundingBoxInfo();
  if (!info)
    return out;

  auto r = [](double v) { return std::round(v * 1e9) / 1e9; };

  // CRS entries without bbox (AUTO2 etc.)
  if (info->Exists("crs"))
  {
    auto& arr = (*info)["crs"];
    for (std::size_t i = 0; i < arr.Size(); ++i)
      out[arr[i].GetString()] = "__present__";
  }
  // Override with actual bbox values
  if (info->Exists("bounding_box"))
  {
    auto& arr = (*info)["bounding_box"];
    for (std::size_t i = 0; i < arr.Size(); ++i)
    {
      auto& bb = arr[i];
      if (!bb.Exists("crs"))
        continue;
      std::string crs = bb["crs"].GetString();
      out[crs] = std::to_string(r(bb["minx"].GetFloat())) + "," +
                 std::to_string(r(bb["miny"].GetFloat())) + "," +
                 std::to_string(r(bb["maxx"].GetFloat())) + "," +
                 std::to_string(r(bb["maxy"].GetFloat()));
    }
  }
  return out;
}

// ----------------------------------------------------------------------
// GROUPING ALGORITHM
//
// For a set of direct children (leaf or sub-namespace), we group them
// by shared attributes in priority order:
//   1. geographic bbox
//   2. time dimension
//   3. elevation dimension
//   4. projected bbox (per CRS)
//
// Each group shares those attributes and can declare them once at the
// group's virtual parent level.  Within a group, further splits happen
// on the remaining attributes.
//
// Implementation: we don't actually create new hierarchy nodes.
// Instead, during output generation we collect the "group header"
// (shared attributes) and emit it once before the group's children.
// We pass it down as InheritedState so children suppress matching
// entries.
// ----------------------------------------------------------------------

// InheritedState: what has already been declared by ancestors.
// Keyed by canonical strings so lookup is O(log n).
struct InheritedState
{
  std::string geoKey;           // empty = not yet declared
  std::string timeKey;          // empty = not yet declared
  std::string elevKey;          // empty = not yet declared
  ProjMap projDeclared;         // crs_id -> bbox_key already declared
  SharedLayer timeDimLayer;  // for actual CTPP output
  SharedLayer elevDimLayer;  // for actual CTPP output
};

// ----------------------------------------------------------------------
// GROUP DESCRIPTOR
//
// Describes a set of sibling layers that share the same geo/time/elev
// values and for which we emit a virtual group node declaring those
// shared values once.
// ----------------------------------------------------------------------

struct LayerGroup
{
  std::string geoKey;
  std::string timeKey;
  std::string elevKey;
  // The representative layer for each shared attribute
  const LayerHierarchy* geoSource = nullptr;
  const LayerHierarchy* timeSource = nullptr;
  const LayerHierarchy* elevSource = nullptr;
  // Per-CRS entries shared by all members
  ProjMap sharedProj;
  // Members of this group
  std::vector<const LayerHierarchy*> members;
};

// Group a flat list of child nodes by geo+time+elev, then find the
// projected bbox entries common to ALL members of each group.
std::vector<LayerGroup> group_children(
    const std::list<boost::shared_ptr<LayerHierarchy>>& sublayers)
{
  // Collect (geo,time,elev) tuples
  std::map<std::tuple<std::string, std::string, std::string>, LayerGroup> groups;

  for (const auto& child : sublayers)
  {
    // For intermediate children, aggregate keys across all their leaves
    // by using the most common value (for time/elev) or unanimous value
    // (for geo/proj).
    auto leaves = get_leaf_layers(*child);

    // geo: unanimous only
    std::string g;
    bool geo_unanimous = true;
    for (const auto* leaf : leaves)
    {
      std::string k = geo_key(*leaf);
      if (g.empty())
        g = k;
      else if (k != g)
      {
        geo_unanimous = false;
        break;
      }
    }
    std::string child_geo = geo_unanimous ? g : "";

    // time: most common across leaves
    std::map<std::string, int> time_counts;
    for (const auto* leaf : leaves)
      time_counts[time_key(*leaf)]++;
    std::string child_time;
    int best = 0;
    for (const auto& [k, v] : time_counts)
      if (v > best)
      {
        best = v;
        child_time = k;
      }

    // elev: most common across leaves
    std::map<std::string, int> elev_counts;
    for (const auto* leaf : leaves)
      elev_counts[elev_key(*leaf)]++;
    std::string child_elev;
    best = 0;
    for (const auto& [k, v] : elev_counts)
      if (v > best)
      {
        best = v;
        child_elev = k;
      }

    auto key = std::make_tuple(child_geo, child_time, child_elev);
    auto& grp = groups[key];
    grp.geoKey = child_geo;
    grp.timeKey = child_time;
    grp.elevKey = child_elev;
    if (!child_geo.empty() && !grp.geoSource)
      grp.geoSource = child.get();
    if (!child_time.empty() && !grp.timeSource)
      grp.timeSource = child.get();
    if (!child_elev.empty() && !grp.elevSource)
      grp.elevSource = child.get();
    grp.members.push_back(child.get());
  }

  // For each group, find projected bbox entries unanimous across all members
  std::vector<LayerGroup> result;
  for (auto& [key, grp] : groups)
  {
    // Seed shared proj from first member
    bool first = true;
    for (const auto* member : grp.members)
    {
      // Collect the unanimous proj map for this member's subtree
      ProjMap member_pm;
      auto leaves = get_leaf_layers(*member);
      bool pm_first = true;
      for (const auto* leaf : leaves)
      {
        ProjMap leaf_pm = proj_map(*leaf);
        if (pm_first)
        {
          member_pm = leaf_pm;
          pm_first = false;
        }
        else
        {
          std::vector<std::string> remove;
          for (const auto& [crs, val] : member_pm)
          {
            auto it = leaf_pm.find(crs);
            if (it == leaf_pm.end() || it->second != val)
              remove.push_back(crs);
          }
          for (const auto& c : remove)
            member_pm.erase(c);
        }
      }

      if (first)
      {
        grp.sharedProj = member_pm;
        first = false;
      }
      else
      {
        std::vector<std::string> remove;
        for (const auto& [crs, val] : grp.sharedProj)
        {
          auto it = member_pm.find(crs);
          if (it == member_pm.end() || it->second != val)
            remove.push_back(crs);
        }
        for (const auto& c : remove)
          grp.sharedProj.erase(c);
      }
    }
    result.push_back(std::move(grp));
  }
  return result;
}

// ----------------------------------------------------------------------
// GETCAPABILITIES OUTPUT — forward declaration
// ----------------------------------------------------------------------

void add_layer_info(bool multiple_intervals,
                    CTPP::CDT& ctpp,
                    const LayerHierarchy& lh,
                    const std::string& language,
                    const std::string& defaultLanguage,
                    const std::optional<Fmi::DateTime>& starttime,
                    const std::optional<Fmi::DateTime>& endtime,
                    const std::optional<Fmi::DateTime>& reference_time,
                    const InheritedState& inh);

// Emit geo bbox CDT if not already declared
void emit_geo(CTPP::CDT& capa, const LayerHierarchy& src, const InheritedState& inh)
{
  std::string k = geo_key(src);
  if (k.empty() || k == inh.geoKey)
    return;
  auto info = src.geographicBoundingBox->getLayer()->getGeographicBoundingBoxInfo();
  if (info)
    capa.MergeCDT(*info);
}

// Emit time dimension CDT if not already declared
void emit_time(CTPP::CDT& capa,
               const LayerHierarchy& src,
               bool multiple_intervals,
               const std::optional<Fmi::DateTime>& starttime,
               const std::optional<Fmi::DateTime>& endtime,
               const std::optional<Fmi::DateTime>& reference_time,
               bool sublayer_is_reference_time_layer,
               const InheritedState& inh)
{
  if (!src.timeDimension)
    return;
  std::string k = time_key(src);
  if (!k.empty() && k == inh.timeKey)
    return;

  auto wmslayer = src.timeDimension->getLayer();
  std::optional<CTPP::CDT> time_dim;
  if (sublayer_is_reference_time_layer)
    time_dim = wmslayer->getReferenceDimensionInfo();
  else if (src.reference_time)
    time_dim =
        wmslayer->getTimeDimensionInfo(multiple_intervals, starttime, endtime, *src.reference_time);
  else
    time_dim =
        wmslayer->getTimeDimensionInfo(multiple_intervals, starttime, endtime, reference_time);

  if (time_dim)
  {
    capa.MergeCDT(*time_dim);
    auto iv = wmslayer->getIntervalDimensionInfo();
    if (iv)
      capa.MergeCDT(*iv);
  }
}

// Emit elevation dimension CDT if not already declared
void emit_elev(CTPP::CDT& capa, const LayerHierarchy& src, const InheritedState& inh)
{
  if (!src.elevationDimension)
    return;
  std::string k = elev_key(src);
  if (!k.empty() && k == inh.elevKey)
    return;
  auto info = src.elevationDimension->getLayer()->getElevationDimensionInfo();
  if (info)
    capa.MergeCDT(*info);
}

// Emit projected bbox delta (only CRS entries not already in inh.projDeclared)
void emit_proj_delta(CTPP::CDT& capa, const ProjMap& pm, const InheritedState& inh)
{
  if (pm.empty())
    return;

  CTPP::CDT delta_crs(CTPP::CDT::ARRAY_VAL);
  CTPP::CDT delta_bbox(CTPP::CDT::ARRAY_VAL);

  for (const auto& [crs_id, val] : pm)
  {
    auto it = inh.projDeclared.find(crs_id);
    if (it != inh.projDeclared.end() && it->second == val)
      continue;

    delta_crs.PushBack(crs_id);
    if (val != "__present__")
    {
      // Parse "minx,miny,maxx,maxy"
      std::vector<double> nums;
      std::istringstream ss(val);
      std::string tok;
      while (std::getline(ss, tok, ','))
        nums.push_back(std::stod(tok));
      if (nums.size() == 4)
      {
        CTPP::CDT bb(CTPP::CDT::HASH_VAL);
        bb["crs"] = crs_id;
        bb["minx"] = nums[0];
        bb["miny"] = nums[1];
        bb["maxx"] = nums[2];
        bb["maxy"] = nums[3];
        delta_bbox.PushBack(bb);
      }
    }
  }

  if (delta_crs.Size() > 0)
  {
    CTPP::CDT p(CTPP::CDT::HASH_VAL);
    p["crs"] = delta_crs;
    if (delta_bbox.Size() > 0)
      p["bounding_box"] = delta_bbox;
    capa.MergeCDT(p);
  }
}

// ----------------------------------------------------------------------
// VIRTUAL GROUP NODE OUTPUT
//
// When a group has >1 member and shares geo/time/elev/proj attributes,
// we emit those shared attributes into an anonymous (title-only)
// intermediate layer and recurse with an updated InheritedState.
// If a group has exactly 1 member, no virtual node is needed.
// ----------------------------------------------------------------------

void emit_group(bool multiple_intervals,
                CTPP::CDT& ctpp,
                const LayerGroup& grp,
                const std::string& language,
                const std::string& defaultLanguage,
                const std::optional<Fmi::DateTime>& starttime,
                const std::optional<Fmi::DateTime>& endtime,
                const std::optional<Fmi::DateTime>& reference_time,
                const InheritedState& inh)
{
  // Build the child inherited state for members of this group
  InheritedState child_inh = inh;

  bool need_virtual_node =
      (grp.members.size() > 1) && (!grp.geoKey.empty() || !grp.timeKey.empty() ||
                                   !grp.elevKey.empty() || !grp.sharedProj.empty());

  CTPP::CDT* target = &ctpp;  // where to push children
  CTPP::CDT virtual_node(CTPP::CDT::HASH_VAL);

  if (need_virtual_node)
  {
    // Emit shared attributes into virtual_node
    if (grp.geoSource && !grp.geoKey.empty() && grp.geoKey != inh.geoKey)
    {
      emit_geo(virtual_node, *grp.geoSource, inh);
      child_inh.geoKey = grp.geoKey;
    }
    if (grp.timeSource && !grp.timeKey.empty() && grp.timeKey != inh.timeKey)
    {
      bool ref_time_children = false;  // virtual nodes never have reference_time sublayers directly
      emit_time(virtual_node,
                *grp.timeSource,
                multiple_intervals,
                starttime,
                endtime,
                reference_time,
                ref_time_children,
                inh);
      child_inh.timeKey = grp.timeKey;
      child_inh.timeDimLayer =
          grp.timeSource->timeDimension ? grp.timeSource->timeDimension->getLayer() : nullptr;
    }
    if (grp.elevSource && !grp.elevKey.empty() && grp.elevKey != inh.elevKey)
    {
      emit_elev(virtual_node, *grp.elevSource, inh);
      child_inh.elevKey = grp.elevKey;
      child_inh.elevDimLayer = grp.elevSource->elevationDimension
                                   ? grp.elevSource->elevationDimension->getLayer()
                                   : nullptr;
    }
    if (!grp.sharedProj.empty())
    {
      emit_proj_delta(virtual_node, grp.sharedProj, inh);
      for (const auto& [crs, val] : grp.sharedProj)
        child_inh.projDeclared[crs] = val;
    }

    bool has_content = virtual_node.Exists("ex_geographic_bounding_box") ||
                       virtual_node.Exists("time_dimension") ||
                       virtual_node.Exists("elevation_dimension") || virtual_node.Exists("crs");

    if (has_content)
    {
      // Use title of first member's common prefix as the group title
      // (just the namespace prefix — don't add a queryable layer name)
      virtual_node["title"] = grp.members.front()->name;
      virtual_node["queryable"] = 0;
      virtual_node["sublayers"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
      ctpp.PushBack(virtual_node);
      // Children go into the virtual node's sublayers
      target = &(ctpp[ctpp.Size() - 1]["sublayers"]);
    }
    else
    {
      need_virtual_node = false;
    }
  }

  // Recurse into members
  for (const auto* member : grp.members)
    add_layer_info(multiple_intervals,
                   *target,
                   *member,
                   language,
                   defaultLanguage,
                   starttime,
                   endtime,
                   reference_time,
                   child_inh);
}

// ----------------------------------------------------------------------
// MAIN OUTPUT FUNCTION
// ----------------------------------------------------------------------

void add_layer_info(bool multiple_intervals,
                    CTPP::CDT& ctpp,
                    const LayerHierarchy& lh,
                    const std::string& language,
                    const std::string& defaultLanguage,
                    const std::optional<Fmi::DateTime>& starttime,
                    const std::optional<Fmi::DateTime>& endtime,
                    const std::optional<Fmi::DateTime>& reference_time,
                    const InheritedState& inh)
{
  CTPP::CDT capa(CTPP::CDT::HASH_VAL);

  bool sublayer_is_reference_time_layer =
      (!lh.sublayers.empty() && lh.sublayers.back()->reference_time);

  // ---- base info ----
  if (lh.sublayers.empty() && lh.baseInfoLayer)
  {
    auto baseInfo = lh.baseInfoLayer->getLayer()->getLayerBaseInfo(language, defaultLanguage);
    if (baseInfo)
    {
      if (lh.reference_time)
      {
        std::string rt = lh.reference_time->to_iso_string();
        (*baseInfo)["name"] = (*baseInfo)["name"].GetString() + ":origintime_" + rt;
        (*baseInfo)["title"] = (*baseInfo)["title"].GetString() + " origintime " + rt;
      }
      capa = *baseInfo;
      auto styles = lh.baseInfoLayer->getLayer()->getStyleInfo(language, defaultLanguage);
      if (styles)
        capa.MergeCDT(*styles);
    }
  }
  else
  {
    capa["title"] = lh.name;
    capa["queryable"] = 0;
  }

  // ---- geographic bounding box ----
  if (lh.geographicBoundingBox)
    emit_geo(capa, lh, inh);

  // ---- time dimension ----
  if (lh.timeDimension)
    emit_time(capa,
              lh,
              multiple_intervals,
              starttime,
              endtime,
              reference_time,
              sublayer_is_reference_time_layer,
              inh);

  // ---- elevation dimension ----
  if (lh.elevationDimension)
    emit_elev(capa, lh, inh);

  // ---- projected bbox ----
  if (lh.projectedBoundingBox)
  {
    ProjMap pm = proj_map(lh);
    emit_proj_delta(capa, pm, inh);
  }

  // ---- recurse ----
  if (lh.sublayers.empty())
  {
    ctpp.PushBack(capa);
    return;
  }

  // For intermediate nodes, group direct children and emit groups
  capa["sublayers"] = CTPP::CDT(CTPP::CDT::ARRAY_VAL);
  ctpp.PushBack(capa);
  CTPP::CDT& sublayer_target = ctpp[ctpp.Size() - 1]["sublayers"];

  // Build updated InheritedState for children based on what we just emitted
  InheritedState child_inh = inh;
  if (lh.geographicBoundingBox)
  {
    std::string k = geo_key(lh);
    if (!k.empty() && k != inh.geoKey)
      child_inh.geoKey = k;
  }
  if (lh.timeDimension)
  {
    std::string k = time_key(lh);
    if (!k.empty() && k != inh.timeKey)
    {
      child_inh.timeKey = k;
      child_inh.timeDimLayer = lh.timeDimension->getLayer();
    }
  }
  if (lh.elevationDimension)
  {
    std::string k = elev_key(lh);
    if (!k.empty() && k != inh.elevKey)
    {
      child_inh.elevKey = k;
      child_inh.elevDimLayer = lh.elevationDimension->getLayer();
    }
  }
  if (lh.projectedBoundingBox)
  {
    ProjMap pm = proj_map(lh);
    for (const auto& [crs, val] : pm)
      child_inh.projDeclared[crs] = val;
  }

  // Group children and emit, possibly with virtual group nodes
  auto groups = group_children(lh.sublayers);

  if (groups.size() == 1 && groups[0].members.size() == lh.sublayers.size())
  {
    // All children in one group — emit group's shared attributes as a
    // virtual node only if there is something new to declare
    emit_group(multiple_intervals,
               sublayer_target,
               groups[0],
               language,
               defaultLanguage,
               starttime,
               endtime,
               reference_time,
               child_inh);
  }
  else
  {
    // Multiple groups — emit each group (possibly with virtual nodes)
    for (const auto& grp : groups)
      emit_group(multiple_intervals,
                 sublayer_target,
                 grp,
                 language,
                 defaultLanguage,
                 starttime,
                 endtime,
                 reference_time,
                 child_inh);
  }
}

}  // namespace

// ----------------------------------------------------------------------
// CONSTRUCTORS
// ----------------------------------------------------------------------

LayerHierarchy::LayerHierarchy(std::string n) : name(std::move(n)), parent(nullptr) {}

#ifndef WITHOUT_AUTHENTICATION
LayerHierarchy::LayerHierarchy(const std::map<std::string, LayerProxy>& layerMap,
                                     const std::optional<std::string>& wms_namespace,
                                     HierarchyType hierarchy_type,
                                     bool reveal_hidden,
                                     const std::optional<std::string>& apikey,
                                     bool auth,
                                     Engine::Authentication::Engine* authEngine)
    : name("__root__"), show_hidden(reveal_hidden), authenticate(auth)
#else
LayerHierarchy::LayerHierarchy(const std::map<std::string, LayerProxy>& layerMap,
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
void LayerHierarchy::processLayers(const std::map<std::string, LayerProxy>& layerMap,
                                      const std::optional<std::string>& wms_namespace,
                                      HierarchyType hierarchy_type,
                                      const std::optional<std::string>& apikey,
                                      Engine::Authentication::Engine* authEngine)
#else
void LayerHierarchy::processLayers(const std::map<std::string, LayerProxy>& layerMap,
                                      const std::optional<std::string>& wms_namespace,
                                      HierarchyType hierarchy_type)
#endif
{
  std::map<std::string, const LayerProxy*> named_layers;
  std::set<std::string> top_level_layers;

  for (const auto& item : layerMap)
  {
    const auto& layer_name = item.first;
#ifndef WITHOUT_AUTHENTICATION
    const std::string wmsService = "wms";
    if (apikey && authenticate)
      if (authEngine == nullptr || !authEngine->authorize(*apikey, layer_name, wmsService))
        continue;
#endif
    if (item.second.getLayer()->isHidden() && !show_hidden)
      continue;
    if (wms_namespace && !match_namespace_pattern(layer_name, *wms_namespace))
      continue;

    if (hierarchy_type == HierarchyType::flat)
    {
      auto h = boost::make_shared<LayerHierarchy>(layer_name);
      const LayerProxy& p = item.second;
      h->baseInfoLayer = p;
      h->geographicBoundingBox = p;
      h->projectedBoundingBox = p;
      h->timeDimension = p;
      h->elevationDimension = p;
      sublayers.push_back(h);
    }
    else
    {
      std::vector<std::string> tokens;
      boost::algorithm::split(tokens, layer_name, boost::is_any_of(":"), boost::token_compress_on);
      std::string nm;
      for (const auto& t : tokens)
      {
        if (!nm.empty())
          nm += ":";
        nm += t;
        named_layers[nm] = &item.second;
        if (nm.find(':') == std::string::npos)
          top_level_layers.insert(nm);
      }
    }
  }

  if (hierarchy_type == HierarchyType::flat)
    return;

  std::set<std::string> processed_layers;
  for (const auto& nm : top_level_layers)
  {
    auto h = boost::make_shared<LayerHierarchy>(nm);
    add_sublayers(*h, named_layers, processed_layers, hierarchy_type);
    sublayers.push_back(h);
  }
}

// ----------------------------------------------------------------------
// getCapabilities
// ----------------------------------------------------------------------

CTPP::CDT LayerHierarchy::getCapabilities(
    bool multiple_intervals,
    const std::string& language,
    const std::string& defaultLanguage,
    const std::optional<Fmi::DateTime>& starttime,
    const std::optional<Fmi::DateTime>& endtime,
    const std::optional<Fmi::DateTime>& reference_time) const
{
  CTPP::CDT capa(CTPP::CDT::ARRAY_VAL);
  InheritedState inh;
  add_layer_info(multiple_intervals,
                 capa,
                 *this,
                 language,
                 defaultLanguage,
                 starttime,
                 endtime,
                 reference_time,
                 inh);
  return capa;
}

std::ostream& operator<<(std::ostream& ost, const LayerHierarchy& lh)
{
  printHierarchy("", 1, ost, lh);
  return ost;
}

}  // namespace OGC
}  // namespace Plugin
}  // namespace SmartMet
