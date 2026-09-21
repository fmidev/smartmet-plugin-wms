# OGC API schemas

Copies of the OpenAPI schemas published by OGC, used by
`ValidateJsonSchema.py` to validate the JSON responses of the OGC API - Tiles
handler in the test suite.

OGC API - Common Part 2 (Geospatial Data) is still a draft and has no schemas
at schemas.opengis.net. The approved OGC API - Tiles 1.0 standard ships the
Common Part 2 collection schemas under `common-geodata/`, so they are taken
from there:

    https://schemas.opengis.net/ogcapi/tiles/part1/1.0/openapi/schemas/

Downloaded 2026-09-21.

| File | Validates |
|------|-----------|
| `schemas/common-geodata/collections.yaml` | `GET /tiles/collections` |
| `schemas/common-geodata/collectionInfo.yaml` | `GET /tiles/collections/{id}` |
| `schemas/common-geodata/extent.yaml`, `extent-uad.yaml` | the `extent` member (spatial, temporal and additional dimensions such as `vertical`) |
| `schemas/common-geodata/dataType.yaml`, `common-core/link.yaml` | referenced by the above |

The files live under `schemas/` here because one of them refers to
`../../schemas/common-geodata/dataType.yaml`, which assumes the OGC
`openapi/schemas/` layout. The `$ref` values are relative file paths;
`ValidateJsonSchema.py` inlines them before validating, so no `$ref`
resolver is needed.

## Local modifications

`extent-uad.yaml` carries one patch, marked `LOCAL PATCH` in the file. The
original applies `additionalProperties` inside an `allOf` branch that declares
no `properties`, so under JSON Schema semantics the additional-dimension rules
(`interval` plus `crs`/`trs`/`vrs` required) are also applied to `spatial` and
`temporal`, and every valid spatial extent is rejected. The patch declares
`spatial` and `temporal` in that branch so the rules only apply to the extra
dimensions such as `vertical`. The upstream file in the ogcapi-tiles GitHub
repository still has the flaw as of the download date.
