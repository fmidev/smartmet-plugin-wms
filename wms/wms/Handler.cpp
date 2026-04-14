// ======================================================================
/*!
 * \brief SmartMet WMS HTTP handler implementation
 */
// ======================================================================
#include "Handler.h"
#include "../CaseInsensitiveComparator.h"
#include "../Hash.h"
#include "../JsonTools.h"
#include "../Layer.h"
#include "../Mime.h"
#include "../ParameterInfo.h"
#include "../Plugin.h"
#include "../Product.h"
#include "../State.h"
#include "../TextUtility.h"
#include "../ogc/StyleSelection.h"
#include "GetCapabilities.h"
#include "GetLegendGraphic.h"
#include "GetMap.h"
#include "RequestType.h"
#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif
#include <boost/timer/timer.hpp>
#include <boost/utility.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <giza/Svg.h>
#include <grid-files/common/ImageFunctions.h>
#include <grid-files/common/ImagePaint.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <macgyver/AnsiEscapeCodes.h>
#include <macgyver/Exception.h>
#include <spine/Convenience.h>
#include <spine/FmiApiKey.h>
#include <spine/HostInfo.h>
#include <spine/Json.h>
#include <spine/SmartMet.h>
#include <memory>
#include <stdexcept>

using namespace boost::placeholders;

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
using Dali::demimetype;
using Dali::mimeType;
using OGC::useStyle;
namespace
{
void check_remaining_wms_json(Json::Value &json, const std::string &name)
{
  std::vector<std::string> deletions{"abstract",
                                     "capabilities_start",
                                     "capabilities_end",
                                     "cascaded",
                                     "fixed_height",
                                     "fixed_width",
                                     "intervals",
                                     "keyword",
                                     "legend",
                                     "legend_url_layer",
                                     "no_subsets",
                                     "opaque",
                                     "queryable",
                                     "refs",
                                     "styles",

                                     // parameter specific inheritable extensions from ParameterInfo
                                     "forecasttype",
                                     "levelid",
                                     "forecastnumber",
                                     "geometryid",
                                     "levelid",
                                     "elevation_unit"};

  for (const auto &delete_name : deletions)
    static_cast<void>(Dali::JsonTools::remove(json, delete_name));

  if (!json.empty())
  {
    Json::StyledWriter writer;
    std::cout << fmt::format("{} Remaining WMS json for product {}:\n{}\n",
                             Spine::log_time_str(),
                             name,
                             writer.write(json))
              << std::flush;
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Init margins from config defaults if necessary
 */
// ----------------------------------------------------------------------
void init_wms_margins(Json::Value &json, int default_margin)
{
  if (default_margin == 0)
    return;

  Json::Value nulljson;
  auto xmargin = json.get("xmargin", nulljson);
  if (xmargin.isNull())
    json["xmargin"] = default_margin;
  auto ymargin = json.get("ymargin", nulljson);
  if (ymargin.isNull())
    json["ymargin"] = default_margin;
}

std::string get_json_element_value(const Json::Value &json, const std::string &keyStr)
{
  std::string ret;

  std::vector<std::string> keys;
  boost::algorithm::split(keys, keyStr, boost::is_any_of("."), boost::token_compress_on);

  const Json::Value *jsonPtr = &json;
  for (const auto &key : keys)
  {
    if (jsonPtr->isMember(key))
    {
      jsonPtr = &((*jsonPtr)[key]);
      if (key == keys.back())
        ret = jsonPtr->asString();
    }
  }

  return ret;
}

void rename_json_element(const Json::Value &json,
                         const std::string &keyStr,
                         const std::string &postfix)
{
  std::vector<std::string> keys;

  boost::algorithm::split(keys, keyStr, boost::is_any_of("."), boost::token_compress_on);

  Json::Value nulljson;
  const Json::Value *to = &json;
  bool found = false;
  for (const auto &key : keys)
  {
    found = false;
    if (to->isMember(key))
    {
      Json::Value::Members members = to->getMemberNames();
      to = &((*to)[key]);
      found = true;
    }
  }

  if (found)
  {
    auto *tototo = const_cast<Json::Value *>(to);
    *tototo = Json::Value(to->asString() + postfix);
  }
}

void relocate_producer_to_view(const std::string & /* layername */, Json::Value &root)
{
  // settings which should not be at the top level if different layers are merged
  // are moved to the view level unless the view already has the setting

  const std::vector<std::string> vars = {"producer",
                                         "source",
                                         "forecastType",
                                         "forecastNumber",
                                         "geometryId",
                                         "level",
                                         "elevation_unit",
                                         "pressure",
                                         "levelId",
                                         "timestep"};

  // Nothing to do? Looks like invalid config but we let other code do the checks

  Json::Value nulljson;
  if (!root.isMember("views"))
    return;

  auto &views = root["views"];
  if (views.isNull() || !views.isArray())
    return;

  // Move the settings from top level to views

  for (const auto &var : vars)
  {
    const auto &value = root.get(var, nulljson);
    if (!value.isNull())
    {
      for (auto &view : views)
      {
        auto view_value = view.get(var, nulljson);
        if (view_value.isNull())
          view[var] = value;  // add to each view where not set yet
      }
      Dali::JsonTools::remove(root, var);  // remove from top level
    }
  }
}

Json::Value merge_layers(const std::vector<Json::Value> &layers)
{
  Json::Value nulljson;

  unsigned int suffix = 1;

  for (const auto &layer : layers)
  {
    const auto &views = layer["views"];
    if (!views.isNull() && views.isArray())
    {
      for (auto &view : views)
      {
        std::string suffixString = "_" + Fmi::to_string(suffix);

        rename_json_element(view, "qid", suffixString);
        rename_json_element(view, "attributes.id", suffixString);

        const auto &viewlayers = view["layers"];
        if (!viewlayers.isNull() && viewlayers.isArray())
        {
          for (auto &viewlayer : viewlayers)
          {
            rename_json_element(viewlayer, "qid", suffixString);
            rename_json_element(viewlayer, "attributes.id", suffixString);

            const auto &viewsublayers = viewlayer["layers"];
            if (!viewsublayers.isNull() && viewsublayers.isArray())
            {
              for (auto &viewsublayer : viewsublayers)
              {
                rename_json_element(viewsublayer, "qid", suffixString);
                rename_json_element(viewsublayer, "attributes.id", suffixString);
              }
            }
          }
        }
        suffix++;
      }
    }
  }

  Json::Value ret;
  ret["views"] = Json::Value(Json::arrayValue);
  auto &retViews = ret["views"];

  for (const auto &layer : layers)
  {
    std::string layerId = get_json_element_value(layer, "attributes.id");

    for (const auto &prop :
         {"title", "abstract", "language", "projection", "margin", "xmargin", "ymargin",
          "time", "origintime", "timestep", "interval_start", "interval_end", "tz"})
    {
      const auto propStr = std::string(prop);
      if (!ret.isMember(propStr) && layer.isMember(propStr))
        ret[propStr] = layer[propStr];
    }

    // Merge styles
    auto &retStyles = ret["styles"];
    const auto &fromStyles = layer.get("styles", nulljson);
    if (!fromStyles.isNull() && fromStyles.isArray())
    {
      for (const auto &fromStyle : fromStyles)
        retStyles.append(fromStyle);
    }

    // Merge views, just append one after another
    auto fromViews = layer.get("views", nulljson);
    if (!fromViews.isNull() && fromViews.isArray())
    {
      for (const auto &fromView : fromViews)
        retViews.append(fromView);
    }

    // Merge defs: arrays are appended, objects are merged (first-seen wins), scalars use first-seen
    const auto &fromDefs = layer.get("defs", nulljson);
    if (!fromDefs.isNull() && fromDefs.isObject())
    {
      auto &retDefs = ret["defs"];
      for (const auto &key : fromDefs.getMemberNames())
      {
        const auto &fromVal = fromDefs[key];
        if (!retDefs.isMember(key))
        {
          retDefs[key] = fromVal;
        }
        else if (fromVal.isArray() && retDefs[key].isArray())
        {
          for (const auto &item : fromVal)
            retDefs[key].append(item);
        }
        else if (fromVal.isObject() && retDefs[key].isObject())
        {
          for (const auto &subkey : fromVal.getMemberNames())
            if (!retDefs[key].isMember(subkey))
              retDefs[key][subkey] = fromVal[subkey];
        }
      }
    }
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set Legend expiration time if so configured
 */
// ----------------------------------------------------------------------
void update_legend_expiration(const Dali::State &theState, int theExpirationTime)
{
  if (theExpirationTime > 0)
  {
    auto tmp = Fmi::SecondClock::universal_time() + Fmi::Seconds(theExpirationTime);
    theState.updateExpirationTime(tmp);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Print grid parameter information (debug helper)
 */
// ----------------------------------------------------------------------
void print_params(const SmartMet::Plugin::Dali::ParameterInfos &infos)
{
  if (infos.empty())
  {
    std::cout << "No grid parameters used in the product\n";
  }
  else
  {
    const std::string missing = "?";
    std::cout << "Parameter information:\n";
    for (const auto &info : infos)
    {
      std::cout << fmt::format("Parameter={} Producer={} Level={}\n",
                               info.parameter,
                               (info.producer ? *info.producer : missing),
                               (info.level ? Fmi::to_string(*info.level) : missing));
    }
  }
  std::cout << std::flush;
}

}  // namespace

// ----------------------------------------------------------------------
// Handler lifecycle
// ----------------------------------------------------------------------

Handler::Handler(const Dali::Config &daliConfig, Spine::JsonCache &jsonCache)
    : itsDaliConfig(daliConfig), itsJsonCache(jsonCache)
{
}

Handler::~Handler() = default;

void Handler::init(std::unique_ptr<Config> wmsConfig)
{
  itsWMSConfig = std::move(wmsConfig);
}

void Handler::shutdown()
{
  if (itsWMSConfig)
    itsWMSConfig->shutdown();
}

// ----------------------------------------------------------------------

std::string Handler::getExceptionFormat(const std::string &theFormat) const
{
  const auto &supported_formats = itsWMSConfig->supportedWMSExceptions();
  if (supported_formats.find(theFormat) == supported_formats.end())
    return "xml";

  if (theFormat == "application/json")
    return "json";

  return "xml";
}

std::string Handler::getCapabilityFormat(const std::string &theFormat) const
{
  const auto &supported_formats = itsWMSConfig->supportedWMSGetCapabilityFormats();
  if (supported_formats.find(theFormat) == supported_formats.end())
    return "xml";

  if (theFormat == "application/json")
    return "json";

  return "xml";
}

void Handler::formatWmsExceptionResponse(Fmi::Exception &wmsException,
                                         const std::string &theFormat,
                                         bool isdebug,
                                         Dali::State &theState,
                                         const Spine::HTTP::Request &theRequest,
                                         Spine::HTTP::Response &theResponse)
{
  try
  {
    auto format = getExceptionFormat(theFormat);

    CTPP::CDT hash;

    const Fmi::Exception *e = wmsException.getExceptionByParameterName(WMS_EXCEPTION_CODE);

    std::string exceptionCode;
    std::string exceptionText;

    if (e != nullptr)
    {
      exceptionCode = e->getParameterValue(WMS_EXCEPTION_CODE);
      if (!isdebug)
        exceptionText = e->getWhat();
      else
        exceptionText = e->getStackTrace();
    }
    else
    {
      exceptionCode = "InvalidExceptionCode";
      exceptionText = wmsException.getWhat();
    }

    hash["exception_code"] = exceptionCode;
    hash["exception_text"] = exceptionText;

    auto tmpl_name = "wms_exception_" + format;
    auto wms_exception_template = theState.getPlugin().getTemplate(tmpl_name);

    std::string msg;
    std::string log;
    wms_exception_template->process(hash, msg, log);

    theState.getPlugin().formatResponse(
        msg, getExceptionFormat(theFormat), theRequest, theResponse, theState.useTimer());
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Authenticate a query
 */
// ----------------------------------------------------------------------
bool Handler::authenticate(const Spine::HTTP::Request &theRequest) const
{
#ifdef WITHOUT_AUTHENTICATION
  return true;
#else
  if (itsDaliConfig.authenticate())
    return itsWMSConfig->validateGetMapAuthorization(theRequest);
  return true;
#endif
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS query (main entry point)
 */
// ----------------------------------------------------------------------
QueryStatus Handler::query(Spine::Reactor & /* theReactor */,
                           Dali::State &theState,
                           const Spine::HTTP::Request &theRequest,
                           Spine::HTTP::Response &theResponse)
{
  try
  {
    RequestType requestType = WMS::requestType(theRequest);

    // Handle common errors immediately

    if (requestType == RequestType::NOT_A_WMS_REQUEST)
    {
      Fmi::Exception ex(BCP, ERROR_NOT_WMS_REQUEST);
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, theRequest, theResponse);
    }

    // Handle GetCapabilities separately

    if (requestType == RequestType::GET_CAPABILITIES)
      return wmsGetCapabilitiesQuery(theState, theRequest, theResponse);

    // Authorize the request

    if (!authenticate(theRequest))
    {
      theResponse.setStatus(Spine::HTTP::Status::forbidden, true);
      return QueryStatus::FORBIDDEN;  // 403 FORBIDDEN
    }

    if (requestType == RequestType::GET_MAP)
      return wmsGetMapQuery(theState, theRequest, theResponse);

    if (requestType == RequestType::GET_LEGEND_GRAPHIC)
      return wmsGetLegendGraphicQuery(theState, theRequest, theResponse);

    if (requestType == RequestType::GET_FEATURE_INFO)
      return wmsGetFeatureInfoQuery(theState, theRequest, theResponse);

    Fmi::Exception ex(BCP, ERROR_NOT_WMS_REQUEST);
    ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS GetMap query
 */
// ----------------------------------------------------------------------
QueryStatus Handler::wmsGetMapQuery(Dali::State &theState,
                                    const Spine::HTTP::Request &theRequest,
                                    Spine::HTTP::Response &theResponse)
{
  try
  {
    // Establish debugging related variables

    const auto print_json = Spine::optional_bool(theRequest.getParameter("printjson"), false);
    const auto json_stage = Spine::optional_int(theRequest.getParameter("stage"), 0);
    const auto cnf_request = Spine::optional_string(theRequest.getParameter("format"), "") == "cnf";

    // WMS-functionality is handled by adjusting requests parameters accordingly
    // Make a copy here since incoming requests are const

    Dali::Product product;
    auto thisRequest = theRequest;

    // Catch other errors and handle them with handleWmsException
    try
    {
      GetMap wmsGetMapRequest(*itsWMSConfig);
      wmsGetMapRequest.parseHTTPRequest(theState.getPlugin().getQEngine(), thisRequest);

      theState.setName(*thisRequest.getParameter("LAYERS"));

      std::vector<std::string> names;
      boost::algorithm::split(names, theState.getName(), boost::is_any_of(","));

      auto json_layers = wmsGetMapRequest.jsons();
      auto styles = wmsGetMapRequest.styles();

      // Process the JSON layers

      bool has_many_layers = (json_layers.size() > 1);

      for (auto i = 0UL; i < json_layers.size(); i++)
      {
        auto &json = json_layers.at(i);
        const auto &style = styles.at(i);
        const auto &name = names.at(i);

        wmsPreprocessJSON(theState, thisRequest, name, json, cnf_request, json_stage);
        useStyle(json, style);
        if (has_many_layers)
          relocate_producer_to_view(name, json);
      }

      // Merge multiple layers into one

      Json::Value json;
      if (has_many_layers)
        json = merge_layers(json_layers);
      else
        json = json_layers.back();

      // Set defaults if some settings are missing

      init_wms_margins(json, itsWMSConfig->getMargin());

      // Debugging

      if (print_json)
      {
        Json::StyledWriter writer;
        std::cout << fmt::format("Expanded Spine::JSON:\n{}\n", writer.write(json));
      }

      // Special product
      if (cnf_request)
      {
        theResponse.setHeader("Content-Type", mimeType("cnf"));
        Json::StyledWriter writer;
        theResponse.setContent(writer.write(json));
        return QueryStatus::OK;
      }

      // Establish type forced by the query. Must not use 'thisRequest' here
      auto fmt = Spine::optional_string(theRequest.getParameter("format"), "image/svg+xml");
      theState.setType(demimetype(fmt));

      // And initialize the product specs from the JSON

      product.init(json, theState, itsDaliConfig);
      check_remaining_wms_json(json, theState.getName());

      product.check_errors(theRequest.getURI(), theState.getPlugin().itsWarnedURLs);

      // If the desired type is not defined in the JSON, the state object knows from earlier code
      // what format to output (HTTP request or default format), and we can not set the Product to
      // use it.

      if (product.type.empty())
        product.type = theState.getType();
    }
    catch (...)
    {
      Fmi::Exception ex(BCP, "Operation failed!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, thisRequest, theResponse);
    }

    const auto print_params_flag =
        Spine::optional_bool(theRequest.getParameter("printparams"), false);
    if (print_params_flag)
      print_params(product.getGridParameterInfo(theState));

    return wmsGenerateProduct(theState, thisRequest, theResponse, product);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS GetLegendGraphic query
 */
// ----------------------------------------------------------------------
QueryStatus Handler::wmsGetLegendGraphicQuery(Dali::State &theState,
                                              const Spine::HTTP::Request &theRequest,
                                              Spine::HTTP::Response &theResponse)
{
  try
  {
    // Establish debugging related variables

    const auto print_json = Spine::optional_bool(theRequest.getParameter("printjson"), false);
    const auto json_stage = Spine::optional_int(theRequest.getParameter("stage"), 0);
    const auto cnf_request = Spine::optional_string(theRequest.getParameter("format"), "") == "cnf";

    // WMS-functionality is handled by adjusting requests parameters accordingly
    // Make a copy here since incoming requests are const

    Dali::Product product;
    auto thisRequest = theRequest;

    // Catch other errors and handle them with handleWmsException
    try
    {
      Json::Value json;

      GetLegendGraphic wmsGetLegendGraphic(*itsWMSConfig);
      wmsGetLegendGraphic.parseHTTPRequest(theState.getPlugin().getQEngine(), thisRequest);
      json = wmsGetLegendGraphic.json();
      update_legend_expiration(theState, itsWMSConfig->getLegendGraphicSettings().expires);

      // Add variant information to the JSON so that variants can change legend titles etc

      const auto opt_sourcelayer = thisRequest.getParameter("SOURCELAYER");
      if (opt_sourcelayer)
      {
        auto shared_layer = itsWMSConfig->getLayer(*opt_sourcelayer);
        if (shared_layer)
          SmartMet::Spine::JSON::expand(json, shared_layer->getSubstitutions());
      }

      // Process the JSON

      std::string layer_name;
      wmsPreprocessJSON(theState, thisRequest, layer_name, json, cnf_request, json_stage);

      // Legends can have alternative styles as well
      auto styleOpt = theRequest.getParameter("STYLE");
      std::string styleName = (styleOpt && !styleOpt->empty() ? *styleOpt : "default");
      useStyle(json, styleName);

      // Debugging

      if (print_json)
      {
        Json::StyledWriter writer;
        std::cout << fmt::format("Expanded Spine::JSON:\n{}\n", writer.write(json));
      }

      // Special product
      if (cnf_request)
      {
        theResponse.setHeader("Content-Type", mimeType("cnf"));
        Json::StyledWriter writer;
        theResponse.setContent(writer.write(json));
        return QueryStatus::OK;
      }

      // Establish type forced by the query

      auto fmt = Spine::optional_string(theRequest.getParameter("format"), "application/svg+xml");
      theState.setType(demimetype(fmt));

      // And initialize the product specs from the JSON

      product.init(json, theState, itsDaliConfig);

      product.check_errors(theRequest.getURI(), theState.getPlugin().itsWarnedURLs);

      // If the desired type is not defined in the JSON, the state object knows from earlier code
      // what format to output (HTTP request or default format), and we can not set the Product to
      // use it.

      if (product.type.empty())
        product.type = theState.getType();

      wmsPrepareGetLegendGraphicQuery(theState, thisRequest, product);
    }
    catch (...)
    {
      Fmi::Exception ex(BCP, "Operation failed!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, thisRequest, theResponse);
    }

    return wmsGenerateProduct(theState, thisRequest, theResponse, product);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS GetFeatureInfo query
 *
 * This is a simplified version of wmsGetMapQuery, some calls have just been
 * removed.
 */
// ----------------------------------------------------------------------

QueryStatus Handler::wmsGetFeatureInfoQuery(Dali::State &theState,
                                            const Spine::HTTP::Request &theRequest,
                                            Spine::HTTP::Response &theResponse)
{
  try
  {
    // WMS-functionality is handled by adjusting requests parameters accordingly
    // Make a copy here since incoming requests are const

    Dali::Product product;
    auto thisRequest = theRequest;

    // Catch other errors and handle them with handleWmsException
    try
    {
      if (!theRequest.getParameter("QUERY_LAYERS"))
      {
        throw Fmi::Exception(BCP, "QUERY_LAYERS option has not been specified")
            .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
            .disableLogging();
      }

      if (!theRequest.getParameter("I") || !theRequest.getParameter("J"))
      {
        throw Fmi::Exception(BCP, "I and J must be specified")
            .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED)
            .disableLogging();
      }

      GetMap wmsGetMapRequest(*itsWMSConfig);
      wmsGetMapRequest.parseHTTPRequest(theState.getPlugin().getQEngine(), thisRequest);

      // Annoying standard requires LAYERS to be specified, and QUERY_LAYERS to list
      // only the layers we're interested in. Since we merge JSON settings, some
      // information is lost unless we store original layer names into the JSON.
      // A simpler solution is to just remove the layers not listed in QUERY_LAYERS
      // as well as the corresponding STYLES elements.

      theState.setName(*thisRequest.getParameter("LAYERS"));

      std::vector<std::string> names;
      boost::algorithm::split(names, theState.getName(), boost::is_any_of(","));

      auto json_layers = wmsGetMapRequest.jsons();
      auto styles = wmsGetMapRequest.styles();

      // Now we remove layers not listed in QUERY_LAYERS

      // TODO
      // keep_only_query_layers(names, json_layers, styles,
      // *thisRequest.getParameter("QUERY_LAYERS"));

      // Process the JSON layers

      bool has_many_layers = (json_layers.size() > 1);

      for (auto i = 0UL; i < json_layers.size(); i++)
      {
        auto &json = json_layers.at(i);
        const auto &style = styles.at(i);
        const auto &name = names.at(i);

        const bool cnf_request = false;
        const int json_stage = 0;
        wmsPreprocessJSON(theState, thisRequest, name, json, cnf_request, json_stage);
        useStyle(json, style);
        if (has_many_layers)
          relocate_producer_to_view(name, json);
      }

      // Merge multiple layers into one

      Json::Value json;
      if (has_many_layers)
        json = merge_layers(json_layers);
      else
        json = json_layers.back();

      // Establish type forced by the query. Must not use 'thisRequest' here
      // Note the parameter is INFO_FORMAT and not FORMAT as for GetMap requests.

      auto fmt = Spine::optional_string(theRequest.getParameter("INFO_FORMAT"), "application/json");
      theState.setType(demimetype(fmt));

      // And initialize the product specs from the JSON

      product.init(json, theState, itsDaliConfig);

      // If the desired type is not defined in the JSON, the state object knows from earlier code
      // what format to output (HTTP request or default format), and we can not set the Product to
      // use it.

      if (product.type.empty())
        product.type = theState.getType();
    }
    catch (...)
    {
      Fmi::Exception ex(BCP, "Operation failed!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, thisRequest, theResponse);
    }

    return wmsGenerateFeatureInfo(theState, thisRequest, theResponse, product);
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS GetCapabilities query
 */
// ----------------------------------------------------------------------
QueryStatus Handler::wmsGetCapabilitiesQuery(Dali::State &theState,
                                             const Spine::HTTP::Request &theRequest,
                                             Spine::HTTP::Response &theResponse)
{
  try
  {
    std::string format = Spine::optional_string(theRequest.getParameter("format"), "xml");

    theState.updateExpirationTime(itsWMSConfig->getCapabilitiesExpirationTime());
    auto tmpl = theState.getPlugin().getTemplate("wms_get_capabilities_" +
                                                 getCapabilityFormat(format));
    auto msg =
        GetCapabilities::response(tmpl, theRequest, theState.getPlugin().getQEngine(), *itsWMSConfig);
    theState.getPlugin().formatResponse(msg, format, theRequest, theResponse, theState.useTimer());
    theState.updateExpirationTime(itsWMSConfig->getCapabilitiesExpirationTime());
    theState.updateModificationTime(itsWMSConfig->getCapabilitiesModificationTime());
    return QueryStatus::OK;
  }
  catch (const Fmi::Exception &wmsException)
  {
    Fmi::Exception ex(
        BCP,
        "Error in generating GetCapabilities response! " + std::string(wmsException.what()),
        nullptr);
    if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }
  catch (...)
  {
    Fmi::Exception ex(BCP, "Error in generating GetCapabilities response!", nullptr);
    if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }
}

// ----------------------------------------------------------------------
/*!
 * \brief Postprocess a successfully handled GetMap/GetLegendGraphic product
 */
// ----------------------------------------------------------------------
QueryStatus Handler::wmsGenerateProduct(Dali::State &theState,
                                        const Spine::HTTP::Request &theRequest,
                                        Spine::HTTP::Response &theResponse,
                                        Dali::Product &theProduct)
{
  // Calculate hash for the product

  std::size_t product_hash = 0;
  try
  {
    product_hash = theProduct.hash_value(theState);
  }
  catch (const Fmi::Exception &exception)
  {
    auto layers = theRequest.getParameter("LAYERS");

    std::string msg;
    if (layers)
      msg = " for layer '" + *layers + "'";  // The LAYERS option may be missing

    Fmi::Exception ex(
        BCP, "Error in calculating hash_value" + msg + "! " + exception.what(), nullptr);
    ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
    return handleWmsException(ex, theState, theRequest, theResponse);
  }

  // We always return valid ETags

  if (product_hash != Fmi::bad_hash)
  {
    theResponse.setHeader("ETag", fmt::sprintf("\"%x\"", product_hash));

    // If request was an ETag request, we're done already

    if (theRequest.getHeader("X-Request-ETag"))
    {
      theResponse.setHeader("Content-Type", mimeType(theProduct.type));
      theResponse.setStatus(Spine::HTTP::Status::no_content);
      return QueryStatus::OK;
    }
  }

  auto obj = theState.getPlugin().findInImageCache(product_hash);
  if (obj && !theProduct.animation.enabled)
  {
    theResponse.setHeader("Content-Type", mimeType(theProduct.type));
    theResponse.setContent(obj);
    return QueryStatus::OK;
  }

  // GeoTiff: bypass CTPP/SVG pipeline entirely and return raw grid data
  if (theProduct.type == "geotiff")
  {
    auto bytes = theProduct.generateGeoTiff(theState);
    auto buffer = std::make_shared<std::string>(std::move(bytes));
    theState.getPlugin().insertInImageCache(product_hash, buffer);
    theResponse.setHeader("Content-Type", mimeType("geotiff"));
    theResponse.setContent(buffer);
    return QueryStatus::OK;
  }

  // DataTile: bypass CTPP/SVG pipeline and return RGBA-encoded float data PNG
  if (theProduct.type == "datatile")
  {
    auto bytes = theProduct.generateDataTile(theState);
    auto buffer = std::make_shared<std::string>(std::move(bytes));
    theState.getPlugin().insertInImageCache(product_hash, buffer);
    theResponse.setHeader("Content-Type", mimeType("datatile"));
    theResponse.setContent(buffer);
    return QueryStatus::OK;
  }

  if (!theProduct.svg_tmpl)
    theProduct.svg_tmpl = itsDaliConfig.defaultTemplate(theProduct.type);

  auto tmpl = theState.getPlugin().getTemplate(*theProduct.svg_tmpl);

  // Build the response CDT
  RequestType requestType = WMS::requestType(theRequest);
  CTPP::CDT hash(CTPP::CDT::HASH_VAL);
  if (requestType == RequestType::GET_LEGEND_GRAPHIC)
    hash["legend"] = "true";

  // ####################### NO ANIMATION ############################################

  if (!theProduct.animation.enabled)
  {
    try
    {
      theProduct.generate(hash, theState);
    }
    catch (...)
    {
      Fmi::Exception e(BCP, "Failed to generate product", nullptr);
      e.addParameter("URI", theRequest.getURI());
      e.addParameter("ClientIP", theRequest.getClientIP());
      e.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));
      const bool check_token = true;
      auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
      e.addParameter("Apikey", (apikey ? *apikey : std::string("-")));
      e.printError();
    }

    // Build the template
    std::string output;
    std::string log;
    try
    {
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "Template processing finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }
      tmpl->process(hash, output, log);
    }
    catch (...)
    {
      Fmi::Exception ex(
          BCP, "Error in processing the template '" + *theProduct.svg_tmpl + "'!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, theRequest, theResponse);
    }

    const auto print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);
    if (print_hash)
      std::cout << fmt::format("Generated CDT:\n{}\n", hash.RecursiveDump());

    theState.getPlugin().formatResponse(
        output, theProduct.type, theRequest, theResponse, theState.useTimer(), theProduct, product_hash);

    return QueryStatus::OK;
  }

  // ####################### ANIMATION ENABLED #######################################

  if (theProduct.animation.enabled)
  {
    theState.animation_enabled = true;

    int animation_timesteps = theProduct.animation.timesteps;
    int data_timestep = theProduct.animation.data_timestep;
    int animation_loopsteps = theProduct.animation.loopsteps;
    int frameCount = animation_timesteps * animation_loopsteps;
    uint width = *theProduct.width;
    uint height = *theProduct.height;
    std::vector<std::shared_ptr<CImage>> images;

    for (int tt = 0; tt < animation_timesteps; tt++)
    {
      theState.animation_timestep = tt;
      theState.animation_timesteps = animation_timesteps;

      for (int loop = 0; loop < animation_loopsteps; loop++)
      {
        CTPP::CDT hash(CTPP::CDT::HASH_VAL);
        // if (requestType == RequestType::GET_LEGEND_GRAPHIC)
        //   hash["legend"] = "true";

        theState.animation_loopstep = loop;
        theState.animation_loopsteps = animation_loopsteps;

        try
        {
          theProduct.generate(hash, theState);
        }
        catch (...)
        {
        }

        std::string output;
        std::string log;
        try
        {
          std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
          if (theState.useTimer())
          {
            std::string report = "Template processing finished in %t sec CPU, %w sec real\n";
            mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
          }
          tmpl->process(hash, output, log);
        }
        catch (...)
        {
          Fmi::Exception ex(
              BCP, "Error in processing the template '" + *theProduct.svg_tmpl + "'!", nullptr);
          if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
            ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
          return handleWmsException(ex, theState, theRequest, theResponse);
        }

        const auto print_hash = Spine::optional_bool(theRequest.getParameter("printhash"), false);
        if (print_hash)
          std::cout << fmt::format("Generated CDT:\n{}\n", hash.RecursiveDump());

        // Converting SVG to ARGB-image:
        uint *vectorImage = Giza::Svg::toargb(output);
        if (vectorImage)
        {
          std::shared_ptr<CImage> sImage(new CImage(width, height, vectorImage));
          images.push_back(sImage);
        }

        /* Saving animation frame for debugging

        char fname[100];
        sprintf(fname, "/tmp/WMS_animation_%d_%llu_%u_%u.svg", getpid(), getTime(),tt,loop);
        FILE *file = fopen(fname,"w");
        fprintf(file,"%s",output.c_str());
        fclose(file);
        */
      }

      if (animation_timesteps > 1)
      {
        // This is time animation so we have to changing the time of the layers.
        // There is probably an easier way to do this.

        for (const auto &view : theProduct.views.views)
        {
          for (auto &layer : view->layers.layers)
          {
            auto tm = layer->getValidTime();
            tm += Fmi::Minutes(data_timestep);
            layer->setValidTime(tm);

            for (auto &ilayer : layer->layers.layers)
              ilayer->setValidTime(tm);
          }
        }
      }
    }

    // *** Creating the animation file:

    std::vector<int> timeVect;
    std::vector<uint *> image(frameCount, nullptr);  // does this leak memory???
    for (int tt = 0; tt < animation_timesteps; tt++)
    {
      for (int t = 0; t < animation_loopsteps; t++)
      {
        uint idx = tt * animation_loopsteps + t;
        const auto &cimage = images[idx];
        image[idx] = cimage->pixel;
        if (animation_loopsteps > 0)
          timeVect.push_back(theProduct.animation.loopstep_interval);
      }
      timeVect.push_back(theProduct.animation.timestep_interval);
    }

    char fname[100];
    sprintf(fname, "/tmp/WMS_animation_%d_%lu.webp", getpid(), getTime());
    webp_anim_save_ARGB(fname, image.data(), width, height, frameCount, timeVect);
    images.clear();

    theResponse.setHeader("Content-Type", "image/webp");

    std::string content;
    std::ifstream in(fname);
    if (!in)
      throw Fmi::Exception(BCP, "Failed to open '" + std::string(fname) + "' for reading!");

    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    theResponse.setContent(content);

    // Removing the animation file
    remove(fname);
  }

  return QueryStatus::OK;
}

// ----------------------------------------------------------------------
/*!
 * \brief Process a GetFeatureInfo query
 */
// ----------------------------------------------------------------------

QueryStatus Handler::wmsGenerateFeatureInfo(Dali::State &theState,
                                            const Spine::HTTP::Request &theRequest,
                                            Spine::HTTP::Response &theResponse,
                                            Dali::Product &theProduct)
{
  // We do not cache the results
  // std::size_t product_hash = 0;

  CTPP::CDT info(CTPP::CDT::HASH_VAL);
  try
  {
    // WMS 1.3.0 says the parameters are i and j, but we use x,y in code
    info["x"] = Fmi::stod(*theRequest.getParameter("i"));
    info["y"] = Fmi::stod(*theRequest.getParameter("j"));

    info["features"] = CTPP::CDT(CTPP::CDT::HASH_VAL);

    theProduct.getFeatureInfo(info, theState);
    // std::cout << fmt::format("Generated CDT:\n{}\n", info.RecursiveDump());

    auto tmpl_name = "wms_get_feature_info_" + theState.getType();
    auto tmpl = theState.getPlugin().getTemplate(tmpl_name);

    std::string output;
    std::string log;
    try
    {
      tmpl->process(info, output, log);
      // std::cout << "Response: " << output << "\n";
      theState.getPlugin().formatResponse(output,
                                         theState.getType(),  // not theProduct.type!
                                         theRequest,
                                         theResponse,
                                         theState.useTimer(),
                                         theProduct,
                                         Fmi::bad_hash);
    }
    catch (...)
    {
      Fmi::Exception ex(BCP, "Error in processing the template '" + tmpl_name + "'!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, theRequest, theResponse);
    }
  }
  catch (...)
  {
    Fmi::Exception e(BCP, "Failed to generate FeatureInfo", nullptr);
    e.addParameter("URI", theRequest.getURI());
    e.addParameter("ClientIP", theRequest.getClientIP());
    e.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));
    const bool check_token = true;
    auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
    e.addParameter("Apikey", (apikey ? *apikey : std::string("-")));
    e.printError();
  }

  return QueryStatus::OK;
}

// ----------------------------------------------------------------------
/*!
 * \brief Alter the product to be a legend graphic query
 */
// ----------------------------------------------------------------------
void Handler::wmsPrepareGetLegendGraphicQuery(const Dali::State &theState,
                                              Spine::HTTP::Request &theRequest,
                                              Dali::Product &product) const
{
  auto layerOpt = theRequest.getParameter("LAYER");
  if (!layerOpt)
  {
    throw Fmi::Exception(BCP, "Layer must be defined in GetLegendGraphic request")
        .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
  }
  const auto &layerName = *layerOpt;

  // Style is optional.
  auto styleOpt = theRequest.getParameter("STYLE");
  std::string styleName = (styleOpt && !styleOpt->empty() ? *styleOpt : "default");

  // Default language from configuration file
  std::string language = itsDaliConfig.defaultLanguage();

  // Language overwritten from product file
  if (product.language)
    language = *product.language;

  // Finally language overwritten from URL-parameter
  auto languageParam = theRequest.getParameter("LANGUAGE");
  if (languageParam)
    language = *languageParam;

  itsWMSConfig->getLegendGraphic(layerName, styleName, product, theState, language);

  // getLegendGraphic-function sets width and height, but if width & height is given in
  // request override values
  std::string xsize = Fmi::to_string(*product.width);
  std::string ysize = Fmi::to_string(*product.height);
  if (theRequest.getParameter("WIDTH"))
  {
    xsize = *(theRequest.getParameter("projection.xsize"));
    product.width = Fmi::stoi(xsize);
  }
  if (theRequest.getParameter("HEIGHT"))
  {
    ysize = *(theRequest.getParameter("projection.ysize"));
    product.height = Fmi::stoi(ysize);
  }
  theRequest.setParameter("projection.xsize", xsize);
  theRequest.setParameter("projection.ysize", ysize);
}

// ----------------------------------------------------------------------
/*!
 * \brief Preprocess product JSON
 */
// ----------------------------------------------------------------------
void Handler::wmsPreprocessJSON(Dali::State &theState,
                                const Spine::HTTP::Request &theRequest,
                                const std::string &theName,
                                Json::Value &theJson,
                                bool isCnfRequest,
                                int theStage)
{
  // Define the customer. Note that parseHTTPRequest may have set a customer
  // for the layer, so this needs to be done after parsing the request

  auto customer =
      Spine::optional_string(theRequest.getParameter("customer"), itsDaliConfig.defaultCustomer());

  if (customer.empty())
    throw Fmi::Exception(BCP, ERROR_NO_CUSTOMER)
        .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);

  theState.setCustomer(customer);

  // Preprocess

  std::string customer_root =
      (itsDaliConfig.rootDirectory(theState.useWms()) + "/customers/" + customer);

  std::string layers_root = customer_root + "/layers/";

  if (!isCnfRequest || (theStage == 0 || theStage > 1))
    Spine::JSON::preprocess(
        theJson, itsDaliConfig.rootDirectory(theState.useWms()), layers_root, itsJsonCache);

  if (!isCnfRequest || (theStage == 0 || theStage > 2))
    Spine::JSON::dereference(theJson);

  // Handle variants before query string parameters

  auto variants = Dali::JsonTools::remove(theJson, "variants");
  if (!theName.empty() && !variants.isNull())
  {
    bool found = false;
    for (auto &variant : variants)
    {
      std::string name;
      Dali::JsonTools::remove_string(name, variant, "name");
      if (name == theName)
      {
        std::map<std::string, Json::Value> substitutes;
        const auto members = variant.getMemberNames();
        for (const auto &member : members)
          substitutes.insert({member, variant[member]});
        SmartMet::Spine::JSON::expand(theJson, substitutes);
        found = true;
        break;
      }
    }
    if (!found)
      throw Fmi::Exception(BCP, "Desired WMS layer variant not found")
          .addParameter("name", theName);
  }

  if (!isCnfRequest || (theStage == 0 || theStage > 3))
  {
    // Do querystring replacements, but remove dangerous substitutions first
    auto params = Dali::Plugin::extractValidParameters(theRequest.getParameterMap());
    Spine::JSON::expand(theJson, params, "", false);
  }
}

QueryStatus Handler::handleWmsException(Fmi::Exception &exception,
                                        Dali::State &theState,
                                        const Spine::HTTP::Request &theRequest,
                                        Spine::HTTP::Response &theResponse)
{
  // Console logging
  exception.addParameter("URI", theRequest.getURI());
  exception.addParameter("ClientIP", theRequest.getClientIP());
  exception.addParameter("HostName", Spine::HostInfo::getHostName(theRequest.getClientIP()));

  const bool check_token = true;
  auto apikey = Spine::FmiApiKey::getFmiApiKey(theRequest, check_token);
  exception.addParameter("Apikey", (apikey ? *apikey : std::string("-")));

  auto quiet = theRequest.getParameter("quiet");
  if (!quiet || (*quiet == "0" || *quiet == "false"))
    exception.printError();

  CaseInsensitiveComparator cicomp;
  ExceptionFormat exceptionFormat = ExceptionFormat::XML;
  std::string exceptionFormatStr =
      Spine::optional_string(theRequest.getParameter("EXCEPTIONS"), "XML");
  if (cicomp(exceptionFormatStr, "INIMAGE"))
    exceptionFormat = ExceptionFormat::INIMAGE;
  else if (cicomp(exceptionFormatStr, "BLANK"))
    exceptionFormat = ExceptionFormat::BLANK;
  else if (cicomp(exceptionFormatStr, "JSON"))
    exceptionFormat = ExceptionFormat::JSON;

  std::string mapFormat = Spine::optional_string(theRequest.getParameter("format"), "xml");

  bool isdebug = Spine::optional_bool(theRequest.getParameter("debug"), false);

  if (exceptionFormat != ExceptionFormat::INIMAGE && exceptionFormat != ExceptionFormat::BLANK)
  {
    if (exceptionFormat == ExceptionFormat::JSON)
      mapFormat = "application/json";

    formatWmsExceptionResponse(exception, mapFormat, isdebug, theState, theRequest, theResponse);
    // Note: a simple throw is not sufficient, the exception object may not have been thrown
    throw exception;
  }

  std::optional<std::string> width = theRequest.getParameter("WIDTH");
  std::optional<std::string> height = theRequest.getParameter("HEIGHT");

  Json::Value json = getExceptionJson(exception.what(),
                                      mapFormat,
                                      exceptionFormat,
                                      Fmi::stoi(width ? *width : "0"),
                                      Fmi::stoi(height ? *height : "0"));

  try
  {
    Dali::Product product;
    // Initialize the product specs from the JSON
    product.init(json, theState, itsDaliConfig);

    product.check_errors(theRequest.getURI(), theState.getPlugin().itsWarnedURLs);

    if (!product.svg_tmpl)
      product.svg_tmpl = itsDaliConfig.defaultTemplate(product.type);

    auto tmpl = theState.getPlugin().getTemplate(*product.svg_tmpl);

    // Build the response CDT
    CTPP::CDT hash(CTPP::CDT::HASH_VAL);

    product.generate(hash, theState);

    // Build the template
    std::string output;
    std::string log;
    try
    {
      std::unique_ptr<boost::timer::auto_cpu_timer> mytimer;
      if (theState.useTimer())
      {
        std::string report = "Template processing finished in %t sec CPU, %w sec real\n";
        mytimer = std::make_unique<boost::timer::auto_cpu_timer>(2, report);
      }
      tmpl->process(hash, output, log);
    }
    catch (...)
    {
      Fmi::Exception ex(
          BCP, "Error in processing the template '" + *product.svg_tmpl + "'!", nullptr);
      if (ex.getExceptionByParameterName(WMS_EXCEPTION_CODE) == nullptr)
        ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      // Render a simple text error response
      auto format = getExceptionFormat(mapFormat);
      theResponse.setHeader("Content-Type", mimeType(format));
      theResponse.setContent(ex.what());
      throw ex;
    }
    theState.getPlugin().formatResponse(
        output, product.type, theRequest, theResponse, theState.useTimer(), product, Fmi::bad_hash);

    return QueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value Handler::getExceptionJson(const std::string &description,
                                      const std::string &mapFormat,
                                      ExceptionFormat format,
                                      unsigned int width,
                                      unsigned int height)
{
  std::string jsonStr;

  std::string errorString = "Error: " + description;
  SmartMet::Plugin::Dali::text_style_t textStyle;
  textStyle.fontsize = "26";
  SmartMet::Plugin::Dali::text_dimension_t textDimension =
      SmartMet::Plugin::Dali::getTextDimension(errorString, textStyle);

  unsigned int pictureWidth = (width > 0 ? width : textDimension.width + 20);
  unsigned int pictureHeight = (height > 0 ? height : textDimension.height + 10);

  jsonStr = "{";
  jsonStr += "\"title\": \"Error\",\n";
  jsonStr += "\"language\": \"fi\",\n";
  if (mapFormat.find("png") != std::string::npos)
    jsonStr += "\"type\": \"png\",\n";
  else if (mapFormat.find("webp") != std::string::npos)
    jsonStr += "\"type\": \"webp\",\n";
  else if (mapFormat.find("pdf") != std::string::npos)
    jsonStr += "\"type\": \"pdf\",\n";
  else
    jsonStr += "\"type\": \"svg\",\n";
  jsonStr += "\"projection\":\n";
  jsonStr += "{\n";
  jsonStr += "   \"crs\": \"data\",\n";
  jsonStr += "   \"xsize\": " + Fmi::to_string(pictureWidth) + ",\n";
  jsonStr += "   \"ysize\": " + Fmi::to_string(pictureHeight) + "\n";
  jsonStr += "},\n";
  jsonStr += "\"views\": [\n";
  jsonStr += "    {\n";
  jsonStr += "        \"qid\": \"v1\",\n";
  jsonStr += "        \"attributes\":\n";
  jsonStr += "        {\n";
  jsonStr += "            \"id\": \"view1\"\n";
  jsonStr += "        },\n";
  jsonStr += "        \"layers\": [\n";
  if (format == ExceptionFormat::INIMAGE)
  {
    jsonStr += "{\n";
    jsonStr += "	   \"qid\": \"tagi\",\n";
    jsonStr += "	   \"tag\": \"g\",\n";
    jsonStr += "	   \"layer_type\": \"tag\"\n";
    jsonStr += "},\n";
    jsonStr += "{";
    jsonStr += "     \"tag\": \"text\",\n";
    jsonStr += "     \"cdata\":\n";
    jsonStr += "     {\n";
    jsonStr += ("           \"en\": \"" + errorString + "\",\n");
    jsonStr += ("           \"fi\": \"" + errorString + "\"\n");
    jsonStr += "     },\n";
    jsonStr += "     \"attributes\":\n";
    jsonStr += "     {\n";
    jsonStr += "           \"x\": \"5\",\n";
    jsonStr += "           \"y\": \"" + Fmi::to_string(textDimension.height) + "\",\n";
    jsonStr += "           \"font-family\": \"" + textStyle.fontfamily + "\",\n";
    jsonStr += "           \"font-weight\": \"" + textStyle.fontweight + "\",\n";
    jsonStr += "           \"font-size\": \"" + textStyle.fontsize + "\",\n";
    jsonStr += "           \"text-anchor\": \"start\"\n";
    jsonStr += "      }\n";
    jsonStr += "}\n";
  }
  jsonStr += "         ]\n";
  jsonStr += "    }\n";
  jsonStr += "       ]\n";
  jsonStr += "}\n";

  return OGC::Layer::parseJsonString(jsonStr);
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet
