# OGC XML schemas

Unmodified copies of the XML schemas used to validate the WMS and WMTS
GetCapabilities responses in the test suite, mirrored from
https://schemas.opengis.net/ and https://www.w3.org/ on 2026-09-21.

| Directory / file | Validates or is imported by |
|------------------|-----------------------------|
| `wms/1.3.0/` | WMS 1.3.0 GetCapabilities |
| `wmts/1.0/` | WMTS 1.0.0 GetCapabilities (and the other WMTS documents it includes) |
| `ows/1.1.0/` | OWS Common 1.1, imported by WMTS |
| `gml/3.1.1/` | GML 3.1.1, imported by the WMTS GetFeatureInfo schema |
| `xlink.xsd`, `xml.xsd` | W3C schemas imported by the above |

`catalog.xml` is an OASIS XML catalog that rewrites the schemaLocation URLs
to these local files, so xmllint validates without network access:

    XML_CATALOG_FILES=schemas/xsd/catalog.xml xmllint --noout --nonet \
        --schema schemas/xsd/wmts/1.0/wmtsGetCapabilities_response.xsd output/wmts_getcapabilities.get

The `validate-xml-schemas` Makefile target does this for the WMS and WMTS
GetCapabilities expectations and actual outputs. `VERSION` records the mirror
date; `make update-schemas` re-mirrors the trees and `make check-schemas`,
run by the test targets, warns when a schema's Last-Modified date upstream is
newer than the mirror. The INSPIRE
ExtendedCapabilities element is stripped from the WMS document before
validation because the INSPIRE schemas are not vendored.
