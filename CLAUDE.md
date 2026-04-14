# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

The WMS/Dali plugin for SmartMet Server. It implements OGC Web Map Service (WMS 1.3.0), WMTS, and OGC API - Tiles, plus a richer non-standard "Dali" interface for meteorological map product rendering. Produces SVG, PNG, WebP, PDF, GeoJSON, KML, GeoTIFF, and Mapbox Vector Tiles (MVT).

## Build and test commands

```bash
make                    # Build wms.so plugin
make test               # Run all ~488 integration tests (needs Redis + test databases)
make test-dali          # Run only Dali tests (non-WMS/WMTS/tiles)
make test-wms           # Run only WMS tests
make test-wmts          # Run only WMTS tests
make test-tiles         # Run only OGC API Tiles tests
make format             # clang-format all source files
```

### Running specific tests

Override the test variable for the relevant group:

```bash
cd test
make test DALI_TESTS="input/autoclass.get"
make test WMS_TESTS="input/wms_getcapabilities.get"
make test WMTS_TESTS="input/wmts_getcapabilities.get"
make test TILES_TESTS="input/tiles_landing.get"
```

### Accepting changed test output

After running tests, inspect `test/failures/` for actual outputs, then:

```bash
make update-outputs     # Copies all failures/*.get to output/*.get
```

Individual acceptance: `cp test/failures/foo.get test/output/foo.get`

### Test infrastructure

Tests are integration tests: `PluginTest.cpp` starts a SmartMet Reactor with the plugin loaded, `TestRunner.sh` feeds HTTP requests from `test/input/*.get` (and `.post`) via named pipes, and `CompareImages.pl` compares actual output against `test/output/` expected files. Comparison is format-aware: images are compared via ImageMagick, SVG is rasterized then compared, XML uses xmllint, JSON uses jq, MVT uses protoc decode.

Tests to skip are listed in `test/input/.testignore`.

Unit tests exist in `test/unit/` (Boost.Test, currently label placement algorithms only).

## Source layout

```
wms/              # Core Dali rendering: Product, View, Layer base, all layer types,
                  #   State, Config, rendering helpers, color painters
wms/wms/          # WMS protocol: Handler, GetMap, GetCapabilities, GetFeatureInfo,
                  #   GetLegendGraphic, RequestType dispatch
wms/ogc/          # OGC layer metadata: wraps Dali layers for GetCapabilities,
                  #   time/elevation dimensions, legend caching
wms/wmts/         # WMTS handler (GetTile, GetCapabilities)
wms/tiles/        # OGC API - Tiles 1.0 REST handler
tmpl/             # CTPP2 templates: svg.tmpl, geojson.tmpl, kml.tmpl, topojson.tmpl
cnf/              # Configuration
docs/             # Documentation and reference images
test/             # Integration test suite
```

Protobuf: `wms/vector_tile.proto` is compiled during build to `vector_tile.pb.{h,cc}` for MVT encoding.

## Architecture

### Rendering pipeline

```
HTTP request
  -> Plugin::requestHandler() routes by URL path (/wms, /wmts, /tiles, /dali)
    -> Handler parses request into JSON product definition
      -> Product::init() parses JSON, creates Views and Layers via LayerFactory
        -> Product::generate() builds CTPP2 CDT tree
          -> Each Layer::generate() produces SVG elements
        -> CTPP2 template processes CDT into SVG string
          -> formatResponse() converts SVG to final format (PNG/WebP/PDF via Giza/Cairo)
            -> Result cached by content hash, ETag set
```

WMS GetMap requests are translated into Dali product JSON internally -- the WMS protocol is a thin wrapper around the Dali engine.

### Key classes

- **Plugin** (`Plugin.h`): Entry point. Routes requests, owns handlers and caches (image, file, JSON, stylesheet).
- **State** (`State.h`): Per-request context carrying engine references, caches, producer/time selection. Passed through the entire rendering stack.
- **Product** (`Product.h`): Top-level rendering unit. Contains views, defs (shared SVG resources), output format settings.
- **View** (`View.h`): A single map panel within a product. Contains layers and projection.
- **Layer** (`Layer.h`): Abstract base for all renderable layer types (~30 subclasses). Key virtuals: `init()`, `generate()`, `getFeatureInfo()`, `hash_value()`.
- **LayerFactory** (`LayerFactory.h`): Creates layer instances from JSON `layer_type` field.
- **WMS::Handler** (`wms/Handler.h`): Dispatches WMS operations by `REQUEST` parameter.
- **WMS::Config** (`wms/Config.h`): WMS layer registry and GetCapabilities metadata.

### Engine dependencies

Required engines (loaded at runtime via SmartMet Server):
- **Querydata** -- forecast model data
- **Grid** -- grid data (GRIB/NetCDF via Redis)
- **Contour** -- isoline/isoband generation
- **GIS** -- PostGIS features, projections, DEM
- **OSM** -- OpenStreetMap background data
- **Geonames** -- location/place name lookup

Optional engines: **Observation**, **Avi** (aviation), **Authentication**

### Product definitions

Products are JSON files organized as `{root}/customers/{customer}/products/{product}.json`. Layers can be defined inline or referenced via `"json:path/to/layer.json"`. A product contains views, each view contains layers. Layer types include: `isoband`, `isoline`, `isolabel`, `arrow`, `symbol`, `number`, `circle`, `stream`, `map`, `postGIS`, `location`, `time`, `background`, `legend`, `osm`, `raster`, `heatmap`, `weather_fronts`, `wind_rose`, and others.

Dali requests: `?customer=X&product=Y&time=...&type=png`

### OGC layer system

The `wms/ogc/` directory contains a separate layer hierarchy that wraps Dali layers for OGC metadata (GetCapabilities). OGC layer types: `GridDataLayer`, `QueryDataLayer`, `PostGISLayer`, `ObservationLayer`, `NonTemporalLayer`. These handle time/elevation dimensions, style enumeration, and legend graphic generation.

### Caching

Multi-level: in-memory image cache (hash-keyed), filesystem cache for templates/JSON, ETag-based HTTP caching. Cache keys computed via `hash_value()` methods throughout the Product/View/Layer hierarchy.

## External dependencies

`REQUIRES = gdal jsoncpp cairo fmt librsvg ctpp2 configpp webp`

SmartMet libraries linked: `grid-content`, `timeseries`, `spine`, `newbase`, `macgyver`, `gis`, `giza`, `locus`. Also: `heatmap`, `protobuf`, `boost_thread`, `boost_regex`, `boost_iostreams`.
