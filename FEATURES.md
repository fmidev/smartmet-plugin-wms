# smartmet-plugin-wms — Feature List

A structured inventory of capabilities provided by the SmartMet
WMS/Dali plugin. Use as a checklist when drafting release notes. When
new functionality is added, append the new entry under the matching
section (and bump the *Last updated* line at the bottom).

`smartmet-plugin-wms` (output: `wms.so`) is the rendering plugin of the
SmartMet Server. It serves OGC Web Map Service (WMS 1.3.0), WMTS, and
OGC API – Tiles, plus a richer non-standard **Dali** interface for
meteorological map products. Output formats include SVG, PNG, WebP,
PDF, GeoJSON, KML, GeoTIFF, Mapbox Vector Tiles, and RGBA-encoded
DataTiles for client-side weather animation.

---

## 1. Supported protocols and endpoints

A single plugin handles all four URL prefixes:

- **`/dali`** — Dali rich JSON product description (the native interface).
- **`/wms`** — OGC WMS 1.3.0.
- **`/wmts`** — OGC WMTS.
- **`/tiles`** — OGC API – Tiles 1.0 (REST).

The Dali engine is the underlying renderer; WMS/WMTS/Tiles translate
their requests into Dali products internally.

## 2. WMS protocol surface (`/wms`)

OGC WMS 1.3.0 compliance via `wms/wms/Handler.cpp`:

- **GetMap** — full image rendering with custom dimensions, layer
  stacks, time, elevation.
- **GetCapabilities** — service description with all configured
  layers, dimensions, CRS, and formats.
- **GetFeatureInfo** — point-in-image queries returning JSON or HTML.
- **GetLegendGraphic** — auto-generated legend images per layer.
- **All proj.4 / proj CRS** supported via GDAL.
- **Dimensions** — time, elevation, reference time (model run) and
  observation time intervals.
- **Authentication / authorisation hooks** when the upstream reactor
  enforces them.

## 3. WMTS protocol surface (`/wmts`)

- **GetTile** — tiled image requests.
- **GetCapabilities** — service description.
- **Configurable tile matrix sets** (`TileMatrix.cpp`).
- **OpenAPI definition** of the RESTful resources at `/wmts/api`, documentation
  page at `/wmts/api/docs`, listed with the kind `wmts` in the API catalog of
  the server (`/api`).

## 4. OGC API – Tiles surface (`/tiles`)

- **Landing page**, **conformance**, **collections**, **tilesets**,
  **tile** endpoints per OGC API – Tiles 1.0.
- Same rendering pipeline as WMS GetMap.
- **OpenAPI definition** at `/tiles/api` (linked from the landing page as
  `service-desc`), documentation page at `/tiles/api/docs` (`service-doc`),
  listed with the kind `tiles` in the API catalog of the server (`/api`).

## 5. Dali interface (`/dali`)

- **JSON product definition** — a single document describing views,
  layers, defs, styles, output format.
- **Server-side caching** — image cache, file cache, JSON cache,
  stylesheet cache.
- **ETag and conditional GETs** — content-hashed cache validation.
- **Debug endpoints** — `wms/wms/Debug.cpp` exposes internal state
  for development.

## 6. Layer types

~30 renderable layer classes, all derived from a common `Layer` base
and discovered via `LayerFactory`:

### Map / background
- **MapLayer**, **BackgroundLayer**, **FrameLayer**, **GraticuleLayer**.
- **TagLayer**, **NullLayer**, **GroupLayer**, **TranslationLayer** —
  structural / placeholder layers.

### Vector data
- **WKTLayer** — arbitrary geometries from WKT strings.
- **PostGISLayer** — features from a configured PostGIS database.
- **OSMLayer** — OpenStreetMap-derived overlays (optional, compiled out by default with `-DWITHOUT_OSM`).
- **WeatherObjects** + **WeatherFronts** — synoptic objects.

### Grid / forecast data
- **GridLayer** — outlines of the querydata grid cells.
- **IsobandLayer** / **IsolineLayer** / **IsolabelLayer** — contouring.
- **RasterLayer** — raw raster overlays.
- **ArrowLayer** — wind / vector arrows.
- **StreamLayer** — streamlines.
- **CircleLayer**, **NumberLayer**, **SymbolLayer** — point glyphs.
- **WindRoseLayer** — wind rose diagrams.
- **HovmoellerLayer** — Hovmöller diagram rendering.
- **IceMapLayer**, **CloudCeilingLayer**.
- **SatelliteLayer** — satellite imagery from the satellite engine.

### Observations
- **ObservationLayer** — generic obs renderer.
- **MetarLayer** — METAR station plots.
- **PresentWeatherObservationLayer** — present-weather symbols.
- **FinnishRoadObservationLayer** — road-weather station data.

### Annotations / decorations
- **LocationLayer**, **LegendLayer**, **CircleLabels**, **TimeLayer**.
- **GeoTiffLayer** — raw GeoTIFF output of grid data (no SVG).
- **AnimationEffects** — hide layers during given animation loop steps.

## 7. Output formats

Selected via the request's `format` (or `FORMAT` for WMS):

- **SVG** — `image/svg+xml` (the native pipeline).
- **PNG** — `image/png` (via Cairo / Giza rasterisation of the SVG).
- **WebP** — `image/webp` (animated WebP for WMS animations and for
  observation time animation of flash symbols).
- **PDF** — `application/pdf`.
- **PostScript** — `application/postscript`.
- **GeoJSON** — vector output (geometry-only, all styling dropped).
- **KML** — Google Earth XML.
- **TopoJSON** / **Raw GeoJSON / KML** — `tmpl/rawgeojson.tmpl`,
  `tmpl/rawkml.tmpl`.
- **GeoTIFF** — bypasses the SVG pipeline; uses GDAL directly via
  `GeoTiffLayer` and `GridDataGeoTiff`.
- **Mapbox Vector Tiles (MVT)** — `application/vnd.mapbox-vector-tile`,
  encoded from `wms/vector_tile.proto`.
- **DataTile** — RGBA-encoded PNG with float scale/offset embedded in
  PNG `tEXt` chunks; lets browser map clients (Leaflet, OpenLayers,
  MapLibre) decode field values for particle animations, hover readouts
  and client-side colouring (see `test/canvas/` demos).

## 8. Templates

CTPP2 templates in `tmpl/`:

- **`svg.tmpl`** — main SVG output.
- **`geojson.tmpl`** / **`rawgeojson.tmpl`** — GeoJSON output.
- **`kml.tmpl`** / **`rawkml.tmpl`** — KML output.
- **`topojson.tmpl`** — TopoJSON output.

## 9. Color rendering

Multiple specialised colour painters, each tuned to a domain:

- **ColorPainter_ARGB** — generic ARGB color mapping.
- **ColorPainter_range** — value-range color ramps.
- **ColorPainter_rain** — precipitation-specific palettes.
- **ColorPainter_shading** — terrain shading.
- **ColorPainter_shadow** — drop-shadow effects.
- **ColorPainter_stream** — streamline colouring.
- **ColorPainter_border** — outline painting.
- **ColorMap**, **Gradients**, **CSSs** — colour-map files and inline
  CSS resolution.

## 10. Geometry processing

- **Geometry.cpp** — vector geometry utilities.
- **BezierFit** / **BezierCache** — Bezier curve smoothing of contour
  lines, cached for repeated renders.
- **Filters.cpp** — SVG filter elements.
- **Connector** — line-segment joining.
- **Circle.cpp** — primitive circle drawing helpers.
- **Attributes** / **AttributeSelection** — XML/SVG attribute
  bookkeeping.

## 11. Aggregation & data utilities

- **AggregationUtility.cpp** — time-window aggregation of grid/obs
  data prior to rendering.
- **ComputedFields** — derived parameters computed on the fly.
- **DataTile.cpp** — float-data PNG encoding.
- **GridDataGeoTiff** — float-data GeoTIFF encoding.

## 12. Engine integration

The plugin wires to these SmartMet engines:

- **querydata engine** — forecast data (required).
- **observation engine** — station observations (optional).
- **grid engine** — GRIB1/GRIB2/NetCDF grids (optional but required
  for GeoTIFF / MVT / DataTile outputs).
- **contour engine** — isoband and isoline generation.
- **gis engine** — PostGIS geometry storage and shapefile support.
- **geonames engine** — place name / location resolution.
- **avi engine** — METAR messages.
- **authentication engine** — API keys (optional).
- **satellite engine** — satellite imagery (optional).
- **OSM engine** — OpenStreetMap data, only when built without `-DWITHOUT_OSM`.

## 13. Caching

Multiple caches inside the plugin:

- **Image cache** — final rendered images keyed by content hash;
  drives ETags.
- **File cache** — referenced external files.
- **JSON cache** — parsed Dali product definitions.
- **Stylesheet cache** — resolved CSS.
- **Layer hash** — every `Layer::hash_value()` participates in cache
  keys so identical inputs reuse outputs.

## 14. Animation

- **Animation.cpp** — the product level `animation` settings of WMS
  animations (time steps × loop steps, frame durations), rendered into an
  animated WebP by the WMS handler.
- **AnimationEffects** — hiding layers during given loop steps.
- **WebP-based output** — animated WebP loops.
- **DataTile animations** — client-side particle systems
  (`WeatherParticles.js`, `WeatherTimeline.js` in companion repos)
  consume DataTile PNGs for rain/snow/wind animations.

## 15. OGC layer metadata

`wms/ogc/`:

- **LayerConfig** — per-layer metadata exposed to GetCapabilities.
- **GridDataLayer** — grid-engine-backed layer with time/elevation
  dimensions.
- **ElevationDimension**, **IntervalDimension** — dimension model.
- **LayerFactory** — builds OGC layer wrappers around Dali layers.

## 16. Configuration

- **libconfig** format with SmartMet extensions (`@include`,
  `@ifdef`, `$(VAR)`, `%(DIR)`).
- **Per-customer product directories** — every customer has its
  own product JSON tree (see `test/wms/customers/`).
- **Database registry** (`cnf/db_registry`) — PostGIS / observation
  database wiring.
- **DaliCapabilities** — automatic capability extraction.
- **Hot-reload** for product JSON, color maps, CSS, templates.

## 17. Testing

Large integration-test suite under `test/`:

- **~600 tests** total (`test/input/*.get`).
- **Group runners**: `make test-dali`, `make test-wms`,
  `make test-wmts`, `make test-tiles`.
- **Per-test selection**: `make test-dali DALI_TESTS="input/foo.get"` (and
  likewise `test-wms WMS_TESTS=...`, `test-wmts WMTS_TESTS=...`,
  `test-tiles TILES_TESTS=...`).
- **Format-aware comparison** — ImageMagick for raster, xmllint for
  XML, jq for JSON, protoc-decode for MVT, rasterisation for SVG.
- **Failure capture** — `test/failures/` keeps the actual output of
  failed tests for inspection.
- **Bulk update** — `make update-outputs` accepts every failure
  output as the new expected.
- **Skip list** — `test/input/.testignore`.
- **Unit tests** — `test/unit/` (Boost.Test: label placement, isoline
  filter validation, smoother options, Mapbox styles, MVT geometry etc).

## 18. Documentation

- **`docs/reference.md`** — Dali product reference.
- **`docs/labeling_algorithms.md`** — label-placement algorithm notes.
- **`docs/isolabel_algorithms.md`** — isolabel algorithm notes.
- **`docs/tutorial.md`** — programmer's tutorial of the rendering pipeline.
- **`docs/grid-support.md`** — grid engine data access.
- **`docs/OGC-API-STYLES.md`** — OGC API Styles notes.
- **`docs/examples/`** — illustrated example requests and outputs.
- **`docs/images/`** — diagrams used in the documentation.

## 19. Build & integration

- **Output**: `wms.so`.
- **Loaded at**: `$(prefix)/share/smartmet/plugins/wms.so`.
- **Build**: `make` (release) / `make debug`.
- **Format target**: `make format` runs clang-format.
- **Install**: `make install`.
- **RPM**: `make rpm`.
- **Protobuf**: `wms/vector_tile.proto` compiled at build time to
  `vector_tile.pb.{h,cc}` for MVT encoding.
- **CI**: CircleCI on RHEL 8 / RHEL 10 with the
  `fmidev/smartmet-cibase-{8,10}` Docker images. Tests require Redis
  and configured test databases.
- **External libraries**: GDAL (raster + GeoTIFF), Cairo / Giza (PNG /
  PDF), libwebp (WebP), libpng (DataTile), CTPP2 (templates), Protobuf
  (MVT), Boost.

---

*Last updated: 2026-10-17.*
