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
`StreamLayer` and `TimeLayer`. The GeoTIFF, data tile and MVT outputs of the contour
layers have grid variants too (`GridDataGeoTiff`, `DataTile`). Layers such as
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
     `elevation`.
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

The point and raster layers:

| Layer | Setting | Difference |
|---|---|---|
| number, symbol, arrow | `multiplier`, `offset`, `unit_conversion`, `level`, `positions`, `minvalues` | None: both paths apply them |
| number, symbol, arrow | `maxdistance` | Used for stations (observations, point querydata) only |
| raster | `multiplier`, `offset`, `unit_conversion` | Applied on both paths to a single `parameter`; `direction` and `speed` are not converted |
| raster | `interpolation` | Grid engine data only (`nearest`, `linear`, `transfer`); querydata is always interpolated bilinearly |

Slayer warns about the settings that are not applied (`smoothing-grid`, `grid-unsupported`).

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

* **Source decided per layer.** A product can mix grid and querydata layers. A layer
  without `"source"` gets its source from `primaryForecastSource` and
  `isGridProducer()`, so the same product can switch paths when the grid engine's
  producer list changes.
* **Producer mapping names.** `getProducerNameList()` may return several producers; the
  Query Server tries them in order. A layer can therefore show data from a different
  producer than the one named in the product.
* **Attribute-key typos.** `grid.*` and `contour.*` keys are not validated.
* **Engine disabled.** The grid hash functions throw `The grid-engine is disabled!` when
  a grid layer is rendered with the engine turned off, and the request fails.
