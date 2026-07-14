# OGC API - Styles: emitting MapLibre/Mapbox styles from wms-conf

Goal: let MapLibre clients style the plugin's **vector (MVT)** tiles with the
**real wms-conf colours**, with zero client-side style maintenance and a single
source of truth (the same config the renderer uses). The standard vehicle is
**OGC API - Styles** serving the **Mapbox style encoding**
(`application/vnd.mapbox.style+json`), which MapLibre consumes natively.

## Why this is small
The colour data already lives in C++: `StyleSheet` (`wms/StyleSheet.{h,cpp}`)
parses the wms-conf CSS and `declarations(".Class")` returns the concrete
`fill`/`stroke`. The MVT output already tags every isoband polygon with the
`class` attribute (`IsobandLayer.cpp`). So a style is just an assembly of
`class → fill` into a Mapbox `["match", ["get","class"], …]` expression.

## Delivered + verified (this change)
- **`wms/MapboxStyle.{h,cpp}`** — `mapboxStyleForIsobands(layerName, tileUrlTemplate,
  isobandLevels, css, sourceLayer="l")` → a full Mapbox GL style JSON: a vector
  source pointing at the MVT tile template and a `fill` layer whose `fill-color`
  is a `match` on `["get","class"]`, colours resolved from the CSS via
  `StyleSheet`. Outside-any-band → transparent.
- **Verified** by compiling against the real `StyleSheet.cpp` and running on a
  real wms-conf product (`weatherapp/Temperature`): **50 levels → 50 class→colour
  match pairs**, valid JSON, `source-layer:"l"`, `version:8`. (Test:
  `test_mapboxstyle.cpp` in the session scratchpad.)
- Compiles cleanly: `g++ -I wms test_mapboxstyle.cpp wms/MapboxStyle.cpp
  wms/StyleSheet.cpp $(pkg-config --cflags --libs jsoncpp) -lboost_regex`.

## Integration (wired into `wms/tiles/Handler.cpp`)
The OGC API - Tiles handler dispatches these routes on `splitTilesPath()`:

1. **Routes** (OGC API - Styles), returning `application/vnd.mapbox.style+json`:
   - `GET /styles` — styleset list (one style id per renderable collection),
     via `Handler::handleStyles()`.
   - `GET /collections/{id}/styles` — styles for a collection, via
     `Handler::handleCollectionStyles()`.
   - `GET /collections/{id}/styles/{styleId}?f=mapbox` — the style document, via
     `Handler::handleStyle()` → `mapboxStyle(...)`.
2. **Layer level table + CSS** are obtained at request time by
   `Handler::resolveStyleLayers()`, which resolves the product JSON exactly as the
   tile renderer does (`Spine::JSON::preprocess`/`dereference`/`expand` +
   `useStyle()`, inlining the `json:` includes and applying the named style
   variant), then walks the active `views` tree collecting every styleable
   isoband/isoline layer (`collectStyleLayers()`). CSS is resolved through
   `State::getStyle()`.
3. **Tile template**: the style's vector source points at the same MVT template
   the tiles handler serves
   (`…/collections/{id}/tiles/EPSG:3857/{tileMatrix}/{tileRow}/{tileCol}?f=application/vnd.mapbox-vector-tile`).
4. **Advertised**: a landing-page link (`rel="styles"`,
   `http://www.opengis.net/def/rel/ogc/1.0/styles`) plus the OGC API - Styles
   core + mapbox-styles conformance class URIs in `/conformance`.
5. **Isolines**: a `line` layer variant (`line-color`/`line-width` from `stroke`)
   is emitted alongside the isoband `fill` variant.

Covered by `test/unit/test_mapboxstyle.cpp` (generator unit tests) and the
`tiles_getstyles` / `tiles_getcollectionstyles` / `tiles_getstyle_isoband` /
`tiles_getstyle_multilayer` integration tests.

## Client side (maplibre-fmi), once the endpoint exists
`ogctiles.js` would, for the MVT path, fetch the style document and apply its
`paint` to the vector source instead of the generic `mvtstyles.js` palettes —
retiring the only drift-prone, hand-maintained styling. (The current default is
the server-styled **raster** path, which already needs no styling at all.)

## Build / deploy / test (for the maintainer)
```bash
# add MapboxStyle.cpp to the wms plugin sources (Makefile picks up wms/*.cpp),
# wire the routes in tiles/Handler.cpp, then:
cd ~/hub/brainstorm/plugins/wms && make
# deploy wms.so to a test server (e.g. back2), then:
curl '.../tiles/collections/fmi:pal:rawtemperature/styles/default?f=mapbox' \
  -H 'Accept: application/vnd.mapbox.style+json'
# → a Mapbox style; load it in MapLibre over the MVT source and confirm the
#   bands render in the real FMI colours.
```

## Why standard, not bespoke
OGC API - Styles is a finalized OGC standard; the Mapbox style encoding is a
recognised style encoding (used by ldproxy, GeoServer, pygeoapi) and is exactly
what MapLibre loads. So this "MapLibre output" is interoperable and reusable by
any compliant client, not a one-off.
