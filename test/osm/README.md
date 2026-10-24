# OSM test data

The `osm` layer renders vector tile datasets of the OSM engine with MapLibre
styles. Two kinds of tests use it.

## OpenStreetMap extract (always run)

`data/extract/` is a small extract of the Protomaps OpenStreetMap basemap:

| File               | Zoom levels | Area                               | Size  |
|--------------------|-------------|------------------------------------|-------|
| `world.pmtiles`    | 0-4         | whole world                        | 6 MB  |
| `helsinki.pmtiles` | 0-12        | lon 24.5...25.5, lat 60.0...60.4   | 10 MB |

The `osm_*`, `tiles_osm_*` and `wms_getmap_osm*` tests use it. The detailed
views show Helsinki, the projection tests the whole world or Europe. Together
the two files test how the engine combines files of different resolution: the
Helsinki area comes from the detailed file and the rest of the world from the
coarse one (see `osm_mixed_resolution`), and zoom levels beyond 12 are cut
from ancestor tiles (`osm_overzoom`, `tiles_osm_mvt_overzoom`).

The extract was cut from the Protomaps build 20261017 with the `pmtiles` tool:

    pmtiles extract planet.pmtiles world.pmtiles --maxzoom=4
    pmtiles extract planet.pmtiles helsinki.pmtiles --maxzoom=12 --bbox=24.5,60.0,25.5,60.4

The data is © OpenStreetMap contributors, available under the Open Database
License (ODbL) 1.0, https://www.openstreetmap.org/copyright.

`styles/` holds copies of the engine styles so that the expected results do
not change when the installed styles do. Their fonts are changed from Noto Sans
to Roboto, the font of all the tests: Noto Sans is installed on RHEL 10 but
not on RHEL 8, so the labels would differ between the platforms.

## Real OpenStreetMap data

The tests use only the extract above: the full OpenStreetMap data (the planet is about 140 GB)
is never packaged or committed and is not available in CI. Some images in
`docs/images/dali/osmdata_*.png` were rendered from the full Protomaps build 20261017 for the
documentation; they are not test results, see the OpenStreetMap section of
`docs/examples/dali.md`.
