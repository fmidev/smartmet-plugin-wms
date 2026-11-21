# Grid support in the WMS/Dali plugin (developer notes)

These notes describe how WMS and Dali layers get their data from the grid engine instead
of the querydata engine. They are for developers; the product format is documented in
[reference.md](reference.md) and the rendering pipeline in [tutorial.md](tutorial.md).
For the grid services, see the developer guides of the
[grid engine](https://github.com/fmidev/smartmet-engine-grid/blob/master/docs/developer-guide.md)
and [grid-content](https://github.com/fmidev/smartmet-library-grid-content/blob/master/docs/developer-guide.md).

## Choosing the source

Every layer derives from `Properties`, which holds `paraminfo` (`source`, `producer`,
`geometryId`, `level`, `levelId`, `pressure`, `elevation`, …). `Properties::init()` sets
`paraminfo.source`:

1. the layer's `"source"` in the product JSON, if given;
2. otherwise `"grid"` when the plugin configuration has `primaryForecastSource = "grid"`;
3. otherwise, when `primaryForecastSource` is set to an empty string, `"grid"` if the
   producer is a grid producer according to `Engine::isGridProducer()`;
4. otherwise the configured `primaryForecastSource` (the default is `"querydata"`).

Each data layer then branches on it, typically in `generate()`:

```cpp
if (paraminfo.source == std::string("grid"))
  generate_gridEngine(theGlobals, theLayersCdt, theState);
else
  generate_qEngine(theGlobals, theLayersCdt, theState);
```

The layers with a grid path are `IsobandLayer`, `IsolineLayer` (and `IsolabelLayer`,
which derives from it), `RasterLayer`, `ArrowLayer`, `NumberLayer`, `SymbolLayer`,
`StreamLayer` and `TimeLayer`. The GeoTIFF and data tile outputs of the contour
layers have grid variants too (`GridDataGeoTiff`, `DataTile`); of the MVT outputs only
the isolines. Layers such as
`MapLayer` and `BackgroundLayer` use the source only to decide what they need.

## The grid path of a layer

The pattern, taking `IsobandLayer::generate_gridEngine()` as the example. The repeated
query blocks are shared helpers in `wms/GridQueryTools.{h,cpp}`:

1. **Build a `QueryServer::Query`** with `QueryServer::QueryConfigurator`, from an
   attribute list of the parameter, producer, times and levels.
   * The producer name is expanded with `Engine::getProducerNameList(name, list)`
     (producer mapping names can mean several producers), and parameters may be
     rewritten with `Engine::getParameterString(producer, parameter)` into the Query
     Server's `name:producer:geometry:levelId:level:…` form.
   * Each `QueryParameter` gets a location type (`Geometry` for a whole output grid,
     `Point` for point lists) and a result type: `Isoband` here, `Isoline` for isolines,
     `Vector` for gridded values (rasters, arrows, streamlines), `PointValues` for
     numbers and symbols. Level flags (`PressureLevels`, `MetricLevels`) follow `pressure` /
     `elevation_unit`.
2. **Describe the output grid** in the query's attribute list, with the grid-files keys:
   `grid.crs` (WKT of the product projection), `grid.bbox` / `grid.llbox`,
   `grid.width` / `grid.height` or `grid.size` / `grid.resolution` / `grid.cx` /
   `grid.cy` / `grid.bboxcrs`, and `grid.areaInterpolationMethod`.
3. **Contouring options** travel the same way: `contour.interpolation.type`
   (linear, midpoint, logarithmic), `contour.closedRange`, `contour.extrapolation`,
   `contour.strict`, `contour.validate`, `contour.desliver`, `contour.subdivide`,
   `contour.smooth.*` (the Trax smoother), `contour.coordinateType` and
   `contour.threads`; grid-files reads them all with `getContourOptions()`.
   `multiplier`, `offset` and `unit_conversion` are applied by wrapping the parameter in
   the grid engine's `MUL{}` and `SUM{}` functions; the `contour.multiplier`,
   `contour.offset`, `contour.smooth.size` and `contour.smooth.degree` attributes are
   sent but not read. See [Settings with grid engine data](#settings-with-grid-engine-data).
4. **`Engine::executeQuery(query)`**. The Query Server finds the content, and the Data
   Server contours or samples the field in the requested grid.
5. **Read the results** from each parameter's `mValueList`. For contours, `mValueData`
   holds one WKB geometry per band or line; the layer turns it into `OGRGeometry`,
   optionally despeckles it (`Fmi::OGR::despeckle`), and hands it to the normal
   rendering and clipping code, the same as on the querydata path. For points and
   rasters, the values come as `GridValueList`s or value vectors.

The grid-specific keys of the attribute list are described in the grid-files developer
guide (§9). They are plain strings: a typo silently has no effect.

## Settings with grid engine data

The contour layers (isoband, isoline, isolabel and the `intersect` of an isoband) do not
apply every setting to grid engine data. Checked against the code of the plugin,
grid-content and grid-files:

| Setting | Grid engine data |
|---|---|
| `multiplier`, `offset`, `unit_conversion` (also of `intersect`) | Applied (`MUL{}`, `SUM{}`) |
| `interpolation`, `minarea`, `filter`, `inside`, `outside`, `precision`, isolabel settings | Applied |
| `closed_range`, `extrapolation`, `strict`, `validate`, `desliver` | Applied with grid-files and grid-content 26.11.20 or later (`contour.closedRange`, `contour.extrapolation`, `contour.strict`, `contour.validate`, `contour.desliver`); ignored by older versions |
| `smoother` `method` box / median / morphology, `subdivide` | Applied with grid-files 26.11.20 or later; older versions apply them to function and metric level parameters only |
| `smoother` `size` / `degree`, `method` savitzky-golay | Not applied |
| `sampling`, `subdivide_min_cell_pixels` | Not applied |
| `tfp` (the TFP metaparameter) | Not supported: the layer fails |
| isoband or `intersect` without `lolimit` and `hilimit` | The area of missing values, as with querydata (grid-files 26.11.21 or later) |
| `intersect` on isoline and isolabel layers, and in `positions` | Applied with the grid engine; `intersect.producer` is honoured. `intersect.level` is not sent |
| `heatmap` | Not applied |

The point and raster layers:

| Layer | Setting | Difference |
|---|---|---|
| number, symbol, arrow | `multiplier`, `offset`, `unit_conversion`, `level`, `positions`, `minvalues` | None: both paths apply them |
| number, symbol, arrow | `maxdistance` | Used for stations (observations, point querydata) only |
| raster | `multiplier`, `offset`, `unit_conversion` | Applied on both paths to a single `parameter`; `direction` and `speed` are not converted |
| raster | `interpolation` | Grid engine data only (`nearest`, `linear`, `transfer`); querydata is always interpolated bilinearly |

Other differences that remain between the two sources (from the audit of 2026-11-21):

| Topic | Querydata | Grid engine |
|---|---|---|
| `pressure`, `levelId`, `elevation_unit`, `forecastType`, `forecastNumber`, `geometryId` | Ignored, only `level` is used | Used |
| `level` | Any number; a missing level is an error | Truncated to an integer; a missing level gives an empty layer |
| `positions.directionoffset`, `positions.layout: "data"` | Applied | Not applied: no offset, no points |
| Arrow and stream `u` / `v` of data with grid-relative winds | Rotated to true north | Not rotated |
| `.raw` parameter suffix | Not supported | Nearest neighbour for gridded results; point layers still interpolate |
| `interpolation` (nearest, discrete, midpoint), `extrapolation`, smoother `radius`, `subdivide` | In native grid cells | In the cells of the output grid, after a bilinear resampling |
| MVT output | All vector layers | Isolines only; the other layers are empty |
| Data tiles, two-band arrow GeoTIFF | Not supported | Supported |
| GeoTIFF sampling | Pixel centres | Corner to corner (up to half a pixel shift) |
| Hovmoeller layer, isoband `heatmap` | Supported | Not supported |
| Unknown `origintime` in Dali, WMTS and tiles requests | Error | Another model run is used silently |
| Default time when all the data is in the past | Latest time for producers marked as observations | Oldest time |
| `Expires` header | Follows the data | One hour |

The source of a layer is resolved once for the product: a layer with a `producer` of its
own inherits the source of the product, and a WMS capabilities layer is a grid layer only
when the product says `"source": "grid"`. Set `"source"` explicitly in products that mix
the sources.

Slayer warns about the settings that are not applied (`smoothing-grid`, `grid-unsupported`,
`querydata-unsupported`).

## ETags and caching

`Properties::hash_value()` includes `getProducerHash()`: for grid layers it is
`Engine::getProducerHash()` of the producer. `countParameterHash()` does the same for
every producer named inside a parameter string. That hash is the Content Server's hash of
the producer's content, cached in the engine for 120 s. Consequently:

* a grid product's ETag changes when the producer gets new content, **up to about two
  minutes later**;
* if you add a grid layer type, include `countParameterHash()` /
  `getProducerHash()` in its `hash_value()`, or its tiles will keep their ETag after
  new data arrives.

Observation producers still return `Fmi::bad_hash` (no caching) as before.

## Pitfalls

* **Source decided per product.** A product can mix grid and querydata layers by giving
  `"source"` in the layers. Without it the source comes from `primaryForecastSource` and
  `isGridProducer()` of the product level producer, so the same product can switch paths
  when the grid engine's producer list changes. The projection of the grid engine layers
  of a view is that of its first visible grid engine layer.
* **Producer mapping names.** `getProducerNameList()` may return several producers; the
  Query Server tries them in order. A layer can therefore show data from a different
  producer than the one named in the product.
* **Attribute-key typos.** `grid.*` and `contour.*` keys are not validated.
* **Engine disabled.** The grid hash functions throw `The grid-engine is disabled!` when
  a grid layer is rendered with the engine turned off, and the request fails.
