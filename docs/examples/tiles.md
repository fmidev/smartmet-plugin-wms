# OGC Tiles Test Examples

The OGC Tiles tests exercise the `/tiles` endpoint, which implements the OGC API — Tiles standard.  This is the RESTful successor to WMTS using the collections model.  The URL path structure is:

```
/tiles/collections/{collection}/tiles/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}?f={format}&TIME={time}
```

Test inputs are in [`test/input/tiles_*.get`](../../test/input/) and expected outputs in [`test/output/tiles_*.get`](../../test/output/).

## tiles_getcollections

**Input:** [`test/input/tiles_getcollections.get`](../../test/input/tiles_getcollections.get)

```
GET /tiles/collections HTTP/1.0
```

Returns a catalogue of all available tile collections (layers), with their identifiers, titles, extent, and supported tile matrix sets and formats.

**Output:** [`test/output/tiles_getcollections.get`](../../test/output/tiles_getcollections.get) — XML/JSON

## tiles_gettile_isoband

**Input:** [`test/input/tiles_gettile_isoband.get`](../../test/input/tiles_gettile_isoband.get)

```
GET /tiles/collections/test:t2m/tiles/EPSG:4326/5/4/36?f=png&TIME=20080805T030000 HTTP/1.0
```

| Query segment | Value | Description |
|---------------|-------|-------------|
| `test:t2m` | Collection | Temperature isoband layer |
| `EPSG:4326` | TileMatrixSet | WGS 84 tile grid |
| `5/4/36` | TileMatrix/Row/Col | Tile coordinates |
| `f` | `png` | Output format as query parameter (vs. file extension in WMTS) |
| `TIME` | `20080805T030000` | Valid time |

Returns a 1024×1024 PNG isoband tile.  Functionally equivalent to the WMTS `wmts_gettile_isoband` test.

**Output:** [`test/output/tiles_gettile_isoband.get`](../../test/output/tiles_gettile_isoband.get) — PNG (1024×1024)

![tiles_gettile_isoband](../images/tiles/tiles_gettile_isoband.png)

## tiles_gettile_temperature_numbers

**Input:** [`test/input/tiles_gettile_temperature_numbers.get`](../../test/input/tiles_gettile_temperature_numbers.get)

```
GET /tiles/collections/test:opendata_temperature_numbers/tiles/EPSG:4326/5/4/36?f=svg&TIME=20130805T1500 HTTP/1.0
```

Returns a 1024×1024 SVG tile of temperature number annotations.

**Output:** [`test/output/tiles_gettile_temperature_numbers.get`](../../test/output/tiles_gettile_temperature_numbers.get) — SVG

![tiles_gettile_temperature_numbers](../images/tiles/tiles_gettile_temperature_numbers.png)

## tiles_gettile_geotiff

**Input:** [`test/input/tiles_gettile_geotiff.get`](../../test/input/tiles_gettile_geotiff.get)

```
GET /tiles/collections/grid:raster_1/tiles/EPSG:4326/5/4/36?f=tiff&TIME=20080805T080000 HTTP/1.0
```

Returns a GeoTIFF tile of raw numerical grid data.

**Output:** [`test/output/tiles_gettile_geotiff.get`](../../test/output/tiles_gettile_geotiff.get) — GeoTIFF

## tiles_gettile_geotiff_wind_speed_and_direction_1 and _2

These tests retrieve GeoTIFF tiles for a multi-band wind product (`grid:wind_speed_and_direction`).  Two variants test different band configurations:

```
GET /tiles/collections/grid:wind_speed_and_direction_1/tiles/EPSG:4326/5/4/36?f=tiff&TIME=20080805T080000 HTTP/1.0
GET /tiles/collections/grid:wind_speed_and_direction_2/tiles/EPSG:4326/5/4/36?f=tiff&TIME=20080805T080000 HTTP/1.0
```

Wind speed and direction are encoded in separate GeoTIFF bands, allowing clients to reconstruct vector wind fields.

**Output:** [`test/output/tiles_gettile_geotiff_wind_speed_and_direction_1.get`](../../test/output/tiles_gettile_geotiff_wind_speed_and_direction_1.get) / [`_2`](../../test/output/tiles_gettile_geotiff_wind_speed_and_direction_2.get) — GeoTIFF

## Mapbox Vector Tile (MVT) outputs

MVT is a compact binary format for encoding vector geometry data in tiles, widely used by Mapbox, OpenLayers, and other mapping clients.

### tiles_gettile_mvt_isoband

**Input:** [`test/input/tiles_gettile_mvt_isoband.get`](../../test/input/tiles_gettile_mvt_isoband.get)

```
GET /tiles/collections/test:t2m/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

| Query segment | Value | Description |
|---------------|-------|-------------|
| `f` | `mvt` | Mapbox Vector Tile binary format |

Returns temperature isoband polygons encoded as an MVT tile.  The isoband geometry is quantised and delta-encoded per the MVT specification.

**Output:** [`test/output/tiles_gettile_mvt_isoband.get`](../../test/output/tiles_gettile_mvt_isoband.get) — MVT (binary)

### tiles_gettile_mvt_isoline

**Input:** [`test/input/tiles_gettile_mvt_isoline.get`](../../test/input/tiles_gettile_mvt_isoline.get)

```
GET /tiles/collections/test:t2m_p/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

Returns temperature isolines as MVT `LineString` features.

**Output:** [`test/output/tiles_gettile_mvt_isoline.get`](../../test/output/tiles_gettile_mvt_isoline.get) — MVT (binary)

### tiles_gettile_mvt_numbers

**Input:** [`test/input/tiles_gettile_mvt_numbers.get`](../../test/input/tiles_gettile_mvt_numbers.get)

```
GET /tiles/collections/test:t2m_numbers/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

Returns temperature number positions and values as MVT `Point` features with the numeric value as an attribute.

**Output:** [`test/output/tiles_gettile_mvt_numbers.get`](../../test/output/tiles_gettile_mvt_numbers.get) — MVT (binary)

### tiles_gettile_mvt_circles

**Input:** [`test/input/tiles_gettile_mvt_circles.get`](../../test/input/tiles_gettile_mvt_circles.get)

```
GET /tiles/collections/test:t2m_circles/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

Returns circle-layer data (station point positions with radius attributes) as MVT `Point` features.

**Output:** [`test/output/tiles_gettile_mvt_circles.get`](../../test/output/tiles_gettile_mvt_circles.get) — MVT (binary)
