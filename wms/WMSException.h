#pragma once

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

enum class WmsExceptionFormat
{
  XML,
  JSON,
  INIMAGE,
  BLANK
};

#define ERROR_NOT_WMS_REQUEST "Not a WMS request!"
#define ERROR_GETFEATUREINFO_NOT_SUPPORTED "GetFeatureInfo operation not supported!"
#define ERROR_NO_CUSTOMER "Customer setting is empty!"
