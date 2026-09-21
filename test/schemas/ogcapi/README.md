# OGC API schemas

Unmodified copies of the OpenAPI schemas of **OGC API - Common - Part 2:
Geospatial Data** (OGC 20-024), used by `ValidateJsonSchema.py` to validate
the JSON responses of the OGC API - Tiles handler in the test suite.

Part 2 is formally still a draft and has no copy at schemas.opengis.net, but
its content is settled, so the schemas are taken from the OGC GitHub
repository:

    https://github.com/opengeospatial/ogcapi-common/tree/master/collections/openapi/schemas

Downloaded 2026-09-21 from commit 3828187a8f (2026-08-14).

| Directory | Contents |
|-----------|----------|
| `common-geodata/` | collections, collection description, extent (spatial, temporal, additional dimensions), grid, data type |
| `common-core/link.yaml` | link object |
| `records-core/` | contact, format, language, theme, roles, referenced by the collection description |

The `$ref` values are relative file paths; `ValidateJsonSchema.py` inlines
them before validating, so no `$ref` resolver is needed. Files not reachable
from the collection schemas (`dynamic/`, landing page, conformance) are not
vendored.

## Which schema validates what

| Schema | Validates |
|--------|-----------|
| `common-geodata/collections-UAD.yaml` | `GET /tiles/collections` |
| `common-geodata/collectionDesc-UAD.yaml` | `GET /tiles/collections/{id}` |

The `-UAD` variants require any dimension beyond `spatial` and `temporal`,
such as `vertical`, to follow the Uniform Additional Dimensions schema
(`additionalDimensionExtent.yaml`: an `interval` plus a `vrs`, `trs` or
`definition`). The plain `collectionDesc.yaml` accepts anything there.

## Local additions

`common-geodata/collections-UAD.yaml` is not an OGC file. OGC's
`collections.yaml` validates list items with the lenient
`collectionDesc.yaml`; the local file is identical except that it uses
`collectionDesc-UAD.yaml`, so the list gets the same strict check as the
single-collection response. It is marked `LOCAL FILE` in its header. No OGC
file is modified.
