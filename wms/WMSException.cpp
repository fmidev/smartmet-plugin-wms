#if 0  // ### REPLACED BY Spine::Exception

#include <WMSException.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{
WMSException::WMSException(ExceptionCode exceptionCode, const std::string& text)
    : exceptionCode(exceptionCode), whatString(text)
{
}

WMSException::WMSException(const std::string& text)
    : exceptionCode(ExceptionCode::VOID_EXCEPTION_CODE), whatString(text)
{
}

WMSException::~WMSException() throw() {}
// ----------------------------------------------------------------------
/*!
 * \brief Exception code as string
 *
 * InvalidFormat - Request contains a Format not offered by the server.
 * InvalidCRS - Request contains a CRS not offered by the server for one or more of the Layers in
 * the request.
 * LayerNotDefined - GetMap request is for a Layer not offered by the server, or GetFeatureInfo
 * request is for a Layer not shown on the map.
 * StyleNotDefined - Request is for a Layer in a Style not offered by the server.
 * LayerNotQueryable - GetFeatureInfo request is applied to a Layer which is not declared queryable.
 * InvalidPoint - GetFeatureInfo request contains invalid X or Y value.
 * CurrentUpdateSequence - Value of (optional) UpdateSequence parameter in GetCapabilities request
 * is equal to current value of service metadata update sequence number.
 * InvalidUpdateSequence - Value of (optional) UpdateSequence parameter in GetCapabilities request
 * is greater than current value of service metadata update sequence number.
 * MissingDimensionValue - Request does not include a sample dimension value, and the server did not
 * declare a default value for that dimension.
 * InvalidDimensionValue - Request contains an invalid sample dimension value.
 * OperationNotSupported - Request is for an optional operation that is not supported by the server.
 *
 */
// ----------------------------------------------------------------------
std::string WMSException::exceptionCodeString() const
{
  switch (exceptionCode)
  {
    case ExceptionCode::INVALID_FORMAT:
      return "InvalidFormat";

    case ExceptionCode::INVALID_CRS:
      return "InvalidCRS";

    case ExceptionCode::LAYER_NOT_DEFINED:
      return "LayerNotDefined";

    case ExceptionCode::STYLE_NOT_DEFINED:
      return "StyleNotDefined";

    case ExceptionCode::LAYER_NOT_QUERYABLE:
      return "LayerNotQueryable";

    case ExceptionCode::INVALID_POINT:
      return "InvalidPoint";

    case ExceptionCode::CURRENT_UPDATE_SEQUENCE:
      return "CurrentUpdateSequence";

    case ExceptionCode::INVALID_UPDATE_SEQUENCE:
      return "InvalidUpdateSequence";

    case ExceptionCode::MISSING_DIMENSION_VALUE:
      return "MissingDimensionValue";

    case ExceptionCode::INVALID_DIMENSION_VALUE:
      return "InvalidDimensionValue";

    case ExceptionCode::OPERATION_NOT_SUPPORTED:
      return "OperationNotSupported";

    default:
      return "InvalidExceptionCode";
  }
}

const char* WMSException::what() const throw() { return whatString.c_str(); }
WMSException::operator CTPP::CDT() const
{
  CTPP::CDT hash;

  if (exceptionCode != ExceptionCode::VOID_EXCEPTION_CODE)
    hash["exception_code"] = exceptionCodeString();

  hash["exception_text"] = whatString;

  return hash;
}

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

#endif
