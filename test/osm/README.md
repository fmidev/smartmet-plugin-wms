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

## Real OpenStreetMap data (optional, local only)

The `full_global_osm_*` tests are the only tests using the full data, and they never run in CI.
They render real OpenStreetMap data: global, European,
Scandinavian, Finnish and Helsinki views with several styles.

The `full_global_osm_radar_*` tests replicate the radar animation of the FMI
precipitation map at https://www.ilmatieteenlaitos.fi/sadekartta with the
rain rate composite of 2026-07-18 18:15 UTC from smartmet-test-data: the
national (EPSG:3067 and LAEA EPSG:3035), regional and Tampere city scales.
The isobands are those of the production layer
`wwwfmifi:tutkahavainto_isoband`, including the translucent band of missing
values which shades the area outside the radar coverage. The radar is drawn
between two OSM layers like on the web page: `sadekartta-base` draws the land
and water under it, `sadekartta-overlay` the coastline, roads, boundaries and
place names over it so that the rain can be located. The two styles are copies
of the styles of the web page (`wwwfmifi/base.json`, `wwwfmifi/overlay_fi.json`)
with the fonts changed to Roboto; the web page itself uses Web Mercator.

The full data
is never packaged or committed: the planet file is about 140 GB. The tests run
only if the dataset directory exists:

    $(OSM_DATA_DIR)/protomaps/*.pmtiles      default OSM_DATA_DIR=/smartmet/data/osm

Install the data with the engine's update tool, for example the whole planet

    smartmet-osm-update -d /smartmet/data/osm

or a smaller set of files, which the engine combines by resolution:

    smartmet-osm-update -d /smartmet/data/osm -m extract -n world -z 8
    smartmet-osm-update -d /smartmet/data/osm -m extract -n nordic -z 15 -b 4,54,33,72

Then run `make test` or only the OSM tests with `make test-osm`, optionally
with `OSM_DATA_DIR=/other/dir`.

The expected results were made with the Protomaps build 20261017 (the whole
planet). Newer builds change the results slightly as OpenStreetMap is edited,
so after updating the data inspect the differences in `failures/` and accept
them if they only reflect changes in the data.
