#pragma once

#include <exception>
#include <ostream>
#include <string>
#include <vector>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/optional.hpp>
#include <ctpp2/CDT.hpp>

#define WMS_EXCEPTION_CODE "WmsExceptionCode"

#define WMS_INVALID_FORMAT "InvalidFormat"
#define WMS_INVALID_CRS "InvalidCRS"
#define WMS_LAYER_NOT_DEFINED "LayerNotDefined"
#define WMS_STYLE_NOT_DEFINED "StyleNotDefined"
#define WMS_LAYER_NOT_QUERYABLE "LayerNotQueryable"
#define WMS_INVALID_POINT "InvalidPoint"
#define WMS_CURRENT_UPDATE_SEQUENCE "CurrentUpdateSequence"
#define WMS_INVALID_UPDATE_SEQUENCE "InvalidUpdateSequence"
#define WMS_MISSING_DIMENSION_VALUE "MissingDimensionValue"
#define WMS_INVALID_DIMENSION_VALUE "InvalidDimensionValue"
#define WMS_OPERATION_NOT_SUPPORTED "OperationNotSupported"
#define WMS_VOID_EXCEPTION_CODE "InvalidExceptionCode"

#if 0  // ### REPLACED BY SmartMet::Spine::Exception

namespace SmartMet
{
namespace Plugin
{
namespace WMS
{


enum ExceptionCode
{
  INVALID_FORMAT,
  INVALID_CRS,
  LAYER_NOT_DEFINED,
  STYLE_NOT_DEFINED,
  LAYER_NOT_QUERYABLE,
  INVALID_POINT,
  CURRENT_UPDATE_SEQUENCE,
  INVALID_UPDATE_SEQUENCE,
  MISSING_DIMENSION_VALUE,
  INVALID_DIMENSION_VALUE,
  OPERATION_NOT_SUPPORTED,
  VOID_EXCEPTION_CODE
};

class WMSException : public std::exception
{
 public:
  WMSException(ExceptionCode exceptionCode, const std::string& text);
  WMSException(const std::string& text);
  virtual ~WMSException() throw();

  std::string exceptionCodeString() const;
  virtual const char* what() const throw();

  operator CTPP::CDT() const;

  inline ExceptionCode getExceptionCode() const { return exceptionCode; }
 private:
  ExceptionCode exceptionCode;
  std::string whatString;
};

}  // namespace WMS
}  // namespace Plugin
}  // namespace SmartMet

#endif
