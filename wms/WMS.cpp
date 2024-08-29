// ======================================================================
/*!
 * \brief SmartMet WMS plugin implementation
 */
// ======================================================================

#include "CaseInsensitiveComparator.h"
#include "Hash.h"
#include "JsonTools.h"
#include "Mime.h"
#include "ParameterInfo.h"
#include "Plugin.h"
#include "Product.h"
#include "State.h"
#include "StyleSelection.h"
#include "TextUtility.h"
#include "WMSConfig.h"
#include "WMSGetCapabilities.h"
#include "WMSGetLegendGraphic.h"
#include "WMSGetMap.h"
#include "WMSRequestType.h"
#ifndef WITHOUT_AUTHENTICATION
#include <engines/authentication/Engine.h>
#endif
#include <boost/timer/timer.hpp>
#include <boost/utility.hpp>
#include <ctpp2/CDT.hpp>
#include <fmt/format.h>
#include <fmt/printf.h>
#include <giza/Svg.h>
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
namespace Dali
{
namespace
{
void check_remaining_wms_json(Json::Value &json, const std::string &name)
{
  std::vector<std::string> deletions{
      "abstract",
      "cascaded",
      "fixed_height",
      "fixed_width",
      "intervals",
      "keyword",
      "legend_url_layer",
      "no_subsets",
      "opaque",
      "queryable",
      "refs",
      "styles",
  };

  for (const auto &delete_name : deletions)
    static_cast<void>(JsonTools::remove(json, delete_name));

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

Json::Value merge_layers(const std::vector<Json::Value> &layers)
{
  Json::Value nulljson;

  unsigned int suffix = 1;

  for (const auto &layer : layers)
  {
    const auto &views = layer["views"];
    if (!views.isNull() && views.isArray())
    {
      for (const auto &view : views)
      {
        // Rename qid and attributes.id of the view
        std::string suffixString = Fmi::to_string(suffix);
        rename_json_element(view, "qid", suffixString);
        rename_json_element(view, "attributes.id", suffixString);

        // Rename qid and attributes.id of view's layers
        const auto &viewlayers = view["layers"];
        for (const auto &viewlayer : viewlayers)
        {
          rename_json_element(viewlayer, "qid", suffixString);
          rename_json_element(viewlayer, "attributes.id", suffixString);

          // Rename qid and attributes.id of view's sublayers
          const auto &viewsublayers = viewlayer["layers"];
          for (const auto &viewsublayer : viewsublayers)
          {
            rename_json_element(viewsublayer, "qid", suffixString);
            rename_json_element(viewsublayer, "attributes.id", suffixString);
          }
        }
        suffix++;
      }
    }
  }

  // Merge layers. Set layer #0 to the bottom and merge rest of layers on top of that.
  Json::Value ret = layers[0];
  Json::Value &retViews = ret["views"];

  // Set retDefsStyles to point to merged layer's styles attribute
  // Set retDefsLayers to point to merged layer's layers vector
  Json::Value *retDefs = nullptr;
  if (ret.isMember("defs"))
  {
    retDefs = &(ret["defs"]);
    if (!retDefs->isMember("styles"))
      (*retDefs)["styles"] = Json::Value(Json::objectValue);
    if (!retDefs->isMember("layers"))
      (*retDefs)["layers"] = Json::Value(Json::arrayValue);
  }
  else
  {
    ret["defs"] = Json::Value(Json::objectValue);
    retDefs = &(ret["defs"]);
    (*retDefs)["styles"] = Json::Value(Json::objectValue);
    (*retDefs)["layers"] = Json::Value(Json::arrayValue);
  }
  Json::Value &retDefsStyles = (*retDefs)["styles"];
  Json::Value &retDefsLayers = (*retDefs)["layers"];

  // Styles on WMS layer (can be changed in URL)
  if (!ret.isMember("styles"))
    ret["styles"] = Json::Value(Json::arrayValue);
  Json::Value &retStyles = ret["styles"];

  if (!ret.isMember("refs"))
    ret["refs"] = Json::Value(Json::objectValue);
  Json::Value &retRefs = ret["refs"];

  // Store ids of layers in defs-section of first layer
  std::set<std::string> defsLayerIdSet;
  for (const auto &layer : retDefsLayers)
  {
    std::string layerId = get_json_element_value(layer, "attributes.id");
    if (!layerId.empty())
      defsLayerIdSet.insert(layerId);
  }

  // Iterate rest of the layers in defs-section
  for (unsigned int i = 1; i < layers.size(); i++)
  {
    const Json::Value &fromLayer = layers[i];
    // Merge defs
    if (fromLayer.isMember("defs"))
    {
      Json::Value fromDefs = fromLayer.get("defs", nulljson);
      // Merge layer styles
      if (fromDefs.isMember("styles"))
      {
        Json::Value::Members currentStyleNames = retDefsStyles.getMemberNames();
        std::set<std::string> currentStyleNameSet(currentStyleNames.begin(),
                                                  currentStyleNames.end());
        const Json::Value &fromDefsStyles = fromDefs["styles"];
        Json::Value::Members fromStyleMemberNames = fromDefsStyles.getMemberNames();
        for (const auto &stylename : fromStyleMemberNames)
        {
          // If style with same name does not exist add it
          if (currentStyleNameSet.find(stylename) == currentStyleNameSet.end())
            (retDefsStyles)[stylename] = fromDefsStyles[stylename];
        }
      }
      // Merge layers inside defs, if layer with the same id exist dont merge
      if (fromDefs.isMember("layers"))
      {
        const auto &defsLayers = fromDefs["layers"];
        for (const auto &layer : defsLayers)
        {
          std::string layerId = get_json_element_value(layer, "attributes.id");
          if (layerId.empty() || defsLayerIdSet.find(layerId) == defsLayerIdSet.end())
          {
            retDefsLayers.append(layer);
            defsLayerIdSet.insert(layerId);
          }
        }
      }
    }

    // Merge refs
    if (fromLayer.isMember("refs"))
    {
      Json::Value fromRefs = fromLayer.get("refs", nulljson);
      Json::Value::Members fromRefNames = fromRefs.getMemberNames();
      Json::Value::Members currentRefNames = retRefs.getMemberNames();
      std::set<std::string> currentRefNameSet(currentRefNames.begin(), currentRefNames.end());
      for (const auto &refname : fromRefNames)
      {
        // If ref with same name does not exist add it
        if (currentRefNameSet.find(refname) == currentRefNameSet.end())
          (retRefs)[refname] = fromRefs[refname];
      }
    }

    // Merge styles
    if (fromLayer.isMember("styles"))
    {
      Json::Value fromStyles = fromLayer.get("styles", nulljson);
      if (!fromStyles.isNull() && fromStyles.isArray())
      {
        for (const auto &fromStyle : fromStyles)
          retStyles.append(fromStyle);
      }
    }

    // Merge views, just append one after another
    auto fromViews = fromLayer.get("views", nulljson);
    if (!fromViews.isNull() && fromViews.isArray())
    {
      for (const auto &fromView : fromViews)
        retViews.append(fromView);
    }
  }

  return ret;
}

// ----------------------------------------------------------------------
/*!
 * \brief Set Legend expiration time if so configured
 */
// ----------------------------------------------------------------------

void update_legend_expiration(const State &theState, int theExpirationTime)
{
  if (theExpirationTime > 0)
  {
    auto tmp = Fmi::SecondClock::universal_time() + Fmi::Seconds(theExpirationTime);
    theState.updateExpirationTime(tmp);
  }
}

}  // namespace

std::string Dali::Plugin::getExceptionFormat(const std::string &theFormat) const
{
  const auto &supported_formats = itsWMSConfig->supportedWMSExceptions();
  if (supported_formats.find(theFormat) == supported_formats.end())
    return "xml";

  if (theFormat == "application/json")
    return "json";

  return "xml";
}

std::string Dali::Plugin::getCapabilityFormat(const std::string &theFormat) const
{
  const auto &supported_formats = itsWMSConfig->supportedWMSGetCapabilityFormats();
  if (supported_formats.find(theFormat) == supported_formats.end())
    return "xml";

  if (theFormat == "application/json")
    return "json";

  return "xml";
}

void Dali::Plugin::formatWmsExceptionResponse(Fmi::Exception &wmsException,
                                              const std::string &theFormat,
                                              bool isdebug,
                                              const Spine::HTTP::Request &theRequest,
                                              Spine::HTTP::Response &theResponse,
                                              bool usetimer)
{
  auto msg = parseWMSException(wmsException, theFormat, isdebug);
  formatResponse(msg, getExceptionFormat(theFormat), theRequest, theResponse, usetimer);
}

// ----------------------------------------------------------------------
/*!
 * \brief Parses WMS exception
 */
// ----------------------------------------------------------------------

std::string Dali::Plugin::parseWMSException(Fmi::Exception &wmsException,
                                            const std::string &theFormat,
                                            bool isdebug) const
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
    auto wms_exception_template = getTemplate(tmpl_name);

    std::string tmpl;
    std::string log;
    wms_exception_template->process(hash, tmpl, log);
    return tmpl;
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

bool Dali::Plugin::authenticate(const Spine::HTTP::Request &theRequest) const
{
#ifdef WITHOUT_AUTHENTICATION
  return true;
#else
  if (itsConfig.authenticate())
    return itsWMSConfig->validateGetMapAuthorization(theRequest);
  return true;
#endif
}

// ----------------------------------------------------------------------
/*!
 * \brief Perform a WMS query
 */
// ----------------------------------------------------------------------

WMSQueryStatus Dali::Plugin::wmsQuery(Spine::Reactor & /* theReactor */,
                                      State &theState,
                                      const Spine::HTTP::Request &theRequest,
                                      Spine::HTTP::Response &theResponse)
{
  try
  {
    WMS::WMSRequestType requestType = WMS::wmsRequestType(theRequest);

    // Handle common errors immediately

    if (requestType == WMS::WMSRequestType::NOT_A_WMS_REQUEST)
    {
      Fmi::Exception ex(BCP, ERROR_NOT_WMS_REQUEST);
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);
      return handleWmsException(ex, theState, theRequest, theResponse);
    }

    if (requestType == WMS::WMSRequestType::GET_FEATURE_INFO)
    {
      Fmi::Exception ex(BCP, ERROR_GETFEATUREINFO_NOT_SUPPORTED);
      ex.addParameter(WMS_EXCEPTION_CODE, WMS_OPERATION_NOT_SUPPORTED);
      return handleWmsException(ex, theState, theRequest, theResponse);
    }

    // Handle GetCapabilities separately

    if (requestType == WMS::WMSRequestType::GET_CAPABILITIES)
      return wmsGetCapabilitiesQuery(theState, theRequest, theResponse);

    // Authorize the request

    if (!authenticate(theRequest))
    {
      theResponse.setStatus(Spine::HTTP::Status::forbidden, true);
      return WMSQueryStatus::FORBIDDEN;  // 403 FORBIDDEN
    }

    if (requestType == WMS::WMSRequestType::GET_MAP)
      return wmsGetMapQuery(theState, theRequest, theResponse);

    if (requestType == WMS::WMSRequestType::GET_LEGEND_GRAPHIC)
      return wmsGetLegendGraphicQuery(theState, theRequest, theResponse);

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

WMSQueryStatus Dali::Plugin::wmsGetMapQuery(State &theState,
                                            const Spine::HTTP::Request &theRequest,
                                            Spine::HTTP::Response &theResponse)
{
  try
  {
    // Establish debugging related variables

    const auto print_params = Spine::optional_bool(theRequest.getParameter("printparams"), false);
    const auto print_json = Spine::optional_bool(theRequest.getParameter("printjson"), false);
    const auto json_stage = Spine::optional_int(theRequest.getParameter("stage"), 0);
    const auto cnf_request = Spine::optional_string(theRequest.getParameter("format"), "") == "cnf";

    // WMS-functionality is handled by adjusting requests parameters accordingly
    // Make a copy here since incoming requests are const

    Product product;
    auto thisRequest = theRequest;

    // Catch other errors and handle them with handleWmsException
    try
    {
      WMS::WMSGetMap wmsGetMapRequest(*itsWMSConfig);
      wmsGetMapRequest.parseHTTPRequest(*itsQEngine, thisRequest);

      theState.setName(*thisRequest.getParameter("LAYERS"));

      std::vector<std::string> names;
      boost::algorithm::split(names, theState.getName(), boost::is_any_of(","));

      auto json_layers = wmsGetMapRequest.jsons();
      auto styles = wmsGetMapRequest.styles();

      // Process the JSON layers

      for (auto i = 0UL; i < json_layers.size(); i++)
      {
        auto &json = json_layers.at(i);
        const auto &style = styles.at(i);
        const auto &name = names.at(i);

        wmsPreprocessJSON(theState, thisRequest, name, json, cnf_request, json_stage);
        SmartMet::Plugin::WMS::useStyle(json, style);
      }

      // Merge multiple layers into one

      Json::Value json;
      if (json_layers.size() == 1)
        json = json_layers.back();
      else
        json = merge_layers(json_layers);

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
        return WMSQueryStatus::OK;
      }

      // Establish type forced by the query. Must not use 'thisRequest' here
      auto fmt = Spine::optional_string(theRequest.getParameter("format"), "image/svg+xml");
      theState.setType(demimetype(fmt));

      // And initialize the product specs from the JSON

      product.init(json, theState, itsConfig);
      check_remaining_wms_json(json, theState.getName());

      product.check_errors(theRequest.getURI(), itsWarnedURLs);

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

    if (print_params)
      print(product.getGridParameterInfo(theState));

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

WMSQueryStatus Dali::Plugin::wmsGetLegendGraphicQuery(State &theState,
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

    Product product;
    auto thisRequest = theRequest;

    // Catch other errors and handle them with handleWmsException
    try
    {
      Json::Value json;

      WMS::WMSGetLegendGraphic wmsGetLegendGraphic(*itsWMSConfig);
      wmsGetLegendGraphic.parseHTTPRequest(*itsQEngine, thisRequest);
      json = wmsGetLegendGraphic.json();
      update_legend_expiration(theState, itsWMSConfig->getLegendGraphicSettings().expires);

      // Process the JSON

      std::string layer_name;
      wmsPreprocessJSON(theState, thisRequest, layer_name, json, cnf_request, json_stage);

      // Legends can have alternative styles as well
      auto styleOpt = theRequest.getParameter("STYLE");
      std::string styleName = (styleOpt && !styleOpt->empty() ? *styleOpt : "default");
      SmartMet::Plugin::WMS::useStyle(json, styleName);

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
        return WMSQueryStatus::OK;
      }

      // Establish type forced by the query

      auto fmt = Spine::optional_string(theRequest.getParameter("format"), "application/svg+xml");
      theState.setType(demimetype(fmt));

      // And initialize the product specs from the JSON

      product.init(json, theState, itsConfig);

      product.check_errors(theRequest.getURI(), itsWarnedURLs);

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
 * \brief Perform a WMS GetCapabilities query
 */
// ----------------------------------------------------------------------

WMSQueryStatus Dali::Plugin::wmsGetCapabilitiesQuery(State &theState,
                                                     const Spine::HTTP::Request &theRequest,
                                                     Spine::HTTP::Response &theResponse)
{
  try
  {
    std::string format = Spine::optional_string(theRequest.getParameter("format"), "xml");

    theState.updateExpirationTime(itsWMSConfig->getCapabilitiesExpirationTime());
    auto tmpl = getTemplate("wms_get_capabilities_" + getCapabilityFormat(format));
    auto msg = WMS::WMSGetCapabilities::response(tmpl, theRequest, *itsQEngine, *itsWMSConfig);
    formatResponse(msg, format, theRequest, theResponse, theState.useTimer());
    theState.updateExpirationTime(itsWMSConfig->getCapabilitiesExpirationTime());
    theState.updateModificationTime(itsWMSConfig->getCapabilitiesModificationTime());
    return WMSQueryStatus::OK;
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

WMSQueryStatus Dali::Plugin::wmsGenerateProduct(State &theState,
                                                const Spine::HTTP::Request &theRequest,
                                                Spine::HTTP::Response &theResponse,
                                                Product &theProduct)
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
      return WMSQueryStatus::OK;
    }
  }

  auto obj = itsImageCache->find(product_hash);

  if (obj)
  {
    theResponse.setHeader("Content-Type", mimeType(theProduct.type));
    theResponse.setContent(obj);
    return WMSQueryStatus::OK;
  }

  if (!theProduct.svg_tmpl)
    theProduct.svg_tmpl = itsConfig.defaultTemplate(theProduct.type);

  auto tmpl = getTemplate(*theProduct.svg_tmpl);

  // Build the response CDT
  WMS::WMSRequestType requestType = WMS::wmsRequestType(theRequest);
  CTPP::CDT hash(CTPP::CDT::HASH_VAL);
  if (requestType == WMS::WMSRequestType::GET_LEGEND_GRAPHIC)
    hash["legend"] = "true";

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

  formatResponse(output,
                 theProduct.type,
                 theRequest,
                 theResponse,
                 theState.useTimer(),
                 theProduct,
                 product_hash);
  return WMSQueryStatus::OK;
}

// ----------------------------------------------------------------------
/*!
 * \brief Alter the product to be a legend graphic query
 */
// ----------------------------------------------------------------------

void Dali::Plugin::wmsPrepareGetLegendGraphicQuery(const State &theState,
                                                   Spine::HTTP::Request &theRequest,
                                                   Product &product) const
{
  auto layerOpt = theRequest.getParameter("LAYER");
  if (!layerOpt)
  {
    throw Fmi::Exception(BCP, "Layer must be defined in GetLegendGraphic request")
        .addParameter(WMS_EXCEPTION_CODE, WMS_LAYER_NOT_DEFINED);
  }
  std::string layerName = *layerOpt;

  // Style is optional.
  auto styleOpt = theRequest.getParameter("STYLE");
  std::string styleName = (styleOpt && !styleOpt->empty() ? *styleOpt : "default");

  // Default language from configuration file
  std::string language = itsConfig.defaultLanguage();

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

void Dali::Plugin::wmsPreprocessJSON(State &theState,
                                     const Spine::HTTP::Request &theRequest,
                                     const std::string &theName,
                                     Json::Value &theJson,
                                     bool isCnfRequest,
                                     int theStage)
{
  // Define the customer. Note that parseHTTPRequest may have set a customer
  // for the layer, so this needs to be done after parsing the request

  auto customer =
      Spine::optional_string(theRequest.getParameter("customer"), itsConfig.defaultCustomer());

  if (customer.empty())
    throw Fmi::Exception(BCP, ERROR_NO_CUSTOMER)
        .addParameter(WMS_EXCEPTION_CODE, WMS_VOID_EXCEPTION_CODE);

  theState.setCustomer(customer);

  // Preprocess

  std::string customer_root =
      (itsConfig.rootDirectory(theState.useWms()) + "/customers/" + customer);

  std::string layers_root = customer_root + "/layers/";

  if (!isCnfRequest || (theStage == 0 || theStage > 1))
    Spine::JSON::preprocess(
        theJson, itsConfig.rootDirectory(theState.useWms()), layers_root, itsJsonCache);

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
    auto params = extractValidParameters(theRequest.getParameterMap());
    Spine::JSON::expand(theJson, params, "", false);
  }
}

WMSQueryStatus Dali::Plugin::handleWmsException(Fmi::Exception &exception,
                                                State &theState,
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

  WMS::CaseInsensitiveComparator cicomp;
  WmsExceptionFormat exceptionFormat = WmsExceptionFormat::XML;
  std::string exceptionFormatStr =
      Spine::optional_string(theRequest.getParameter("EXCEPTIONS"), "XML");
  if (cicomp(exceptionFormatStr, "INIMAGE"))
    exceptionFormat = WmsExceptionFormat::INIMAGE;
  else if (cicomp(exceptionFormatStr, "BLANK"))
    exceptionFormat = WmsExceptionFormat::BLANK;
  else if (cicomp(exceptionFormatStr, "JSON"))
    exceptionFormat = WmsExceptionFormat::JSON;

  std::string mapFormat = Spine::optional_string(theRequest.getParameter("format"), "xml");

  bool isdebug = Spine::optional_bool(theRequest.getParameter("debug"), false);

  if (exceptionFormat != WmsExceptionFormat::INIMAGE &&
      exceptionFormat != WmsExceptionFormat::BLANK)
  {
    if (exceptionFormat == WmsExceptionFormat::JSON)
      mapFormat = "application/json";
    formatWmsExceptionResponse(
        exception, mapFormat, isdebug, theRequest, theResponse, theState.useTimer());
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
    Product product;
    // Initialize the product specs from the JSON
    product.init(json, theState, itsConfig);

    product.check_errors(theRequest.getURI(), itsWarnedURLs);

    if (!product.svg_tmpl)
      product.svg_tmpl = itsConfig.defaultTemplate(product.type);

    auto tmpl = getTemplate(*product.svg_tmpl);

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
      formatWmsExceptionResponse(
          ex, mapFormat, isdebug, theRequest, theResponse, theState.useTimer());
      throw ex;
    }
    formatResponse(
        output, product.type, theRequest, theResponse, theState.useTimer(), product, Fmi::bad_hash);

    return WMSQueryStatus::OK;
  }
  catch (...)
  {
    throw Fmi::Exception::Trace(BCP, "Operation failed!");
  }
}

Json::Value Dali::Plugin::getExceptionJson(const std::string &description,
                                           const std::string &mapFormat,
                                           WmsExceptionFormat format,
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
  jsonStr += "   \"xsize\": " + std::to_string(pictureWidth) + ",\n";
  jsonStr += "   \"ysize\": " + std::to_string(pictureHeight) + "\n";
  jsonStr += "},\n";
  jsonStr += "\"views\": [\n";
  jsonStr += "    {\n";
  jsonStr += "        \"qid\": \"v1\",\n";
  jsonStr += "        \"attributes\":\n";
  jsonStr += "        {\n";
  jsonStr += "            \"id\": \"view1\"\n";
  jsonStr += "        },\n";
  jsonStr += "        \"layers\": [\n";
  if (format == WmsExceptionFormat::INIMAGE)
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
    jsonStr += "           \"y\": \"" + std::to_string(textDimension.height) + "\",\n";
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

  return SmartMet::Plugin::WMS::WMSLayer::parseJsonString(jsonStr);
}

}  // namespace Dali
}  // namespace Plugin
}  // namespace SmartMet
