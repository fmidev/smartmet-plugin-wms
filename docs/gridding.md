# Gridded observations

The plugin can interpolate point observations onto a fixed grid per area (Europe, Africa...)
with the help of the terrain, and draw the result like gridded querydata: the isoline, isoband
and isolabel layers of WMS, WMTS and OGC Tiles work with it, and OGC Tiles can deliver it as
GeoTIFF. There is no timeseries or download access to it.

The interpolation is `Fmi::PointGridding` of the gis library, see its
`docs/gis-pointgridding.md` for the method and its validation: a height and topographic
position regression fitted locally from Gaussian-weighted station moments, two height-aware
damped Barnes passes for the residuals, and a leave-one-out gross error check. In
cross-validation of European 2 m temperatures the error is 1.62 K RMS against 2.44 K for plain
Barnes interpolation.

The code is in `wms/gridding/`.

## Contents

1. [Design](#1-design)
2. [Configuration](#2-configuration)
3. [Terrain rasters](#3-terrain-rasters)
4. [Sources of observations](#4-sources-of-observations)
5. [Code](#5-code)
6. [Testing](#6-testing)

---

## 1. Design

- **One grid per area.** The whole area is gridded at once for a time, and every request for
  that time shares the result. Map tiles are therefore seamless: neighbouring tiles cut their
  isobands from the same field. A full 2.5 km Europe grid takes about half a second.
- **Gridded querydata.** The result is an in-memory `NFmiQueryData` wrapped in an ordinary
  querydata `Q`, with one time and all the parameters of the producer, in the coordinate system
  of the terrain raster. Its grid points are the cell centres of the raster. `State::getModel`
  returns it for gridded producers, so the layers need no changes.
- **Observations are read again after a few seconds.** The hash of the observations identifies
  the gridded data, so new and corrected observations are picked up soon after they are in the
  source. A read is shared by the requests for the same time during `observation_cache_seconds`
  (default 10), so the tiles of a map read the observations once. ETags use the same hash
  without gridding.
- **Cache.** Gridded datasets are cached by that hash (`cache_size`), and simultaneous requests
  for a dataset which is being gridded wait for it instead of gridding it again. The cache holds
  the data, and every request gets a new `Q` over it: a `Q` holds an iterator to the data and
  must not be shared between threads. At most `max_concurrent` datasets (default 10) are gridded
  at the same time, since every distinct time is a gridding of its own.
- **Capabilities.** WMS layers of gridded producers are `ogc/ObsGridLayer`s: the bounding box
  and the time dimension come from the gridding configuration, and the default time is the
  latest one.

## 2. Configuration

The gridding is enabled by a `gridding` group in the plugin configuration file:

```
gridding:
{
	# Number of gridded datasets kept in memory
	cache_size = 50;

	# Threads used for gridding one dataset, 0 for all cores
	threads = 0;

	# Datasets gridded at the same time, further griddings wait for their turn
	max_concurrent = 10;

	# Seconds an observation read is shared by requests for the same time, 0 disables
	observation_cache_seconds = 10;

	producers:
	{
		obs_europe:                          # the producer name used in products
		{
			source   = "observations";       # "observations" or "querydata"
			producer = "opendata";           # station type, or the point querydata producer
			dem      = "/smartmet/share/gridding/europe_3035_2500.tif";
			tpi_sigma = 10.0;                # km, terrain smoothing for the TPI predictor

			# Observation times offered in capabilities: every timestep minutes, up to now
			# minus delay, history minutes back. Any observation time can be requested.
			# Point querydata offers its own times.
			timestep = 60;
			delay    = 10;
			history  = 1440;
			time_tolerance = 0;              # minutes an observation may be off the time

			# Point querydata has no station elevations: lines of "id,elevation", and/or the
			# station metadata of an observation engine station type by WMO number
			station_elevations = "/smartmet/share/gridding/stations.csv";
			elevation_stationtype = "foreign";

			parameters =
			(
				# name is the newbase name of the gridded parameter, source the name at the
				# source (default: the same)
				{ name = "Temperature"; source = "TA_PT1H_AVG"; },
				{ name = "DewPoint"; source = "TD_PT1H_AVG"; },
				{ name = "Pressure"; source = "PSEA"; method = "barnes";
				  residual_sigmas = [80.0]; qc_threshold = 8.0; },
				{ name = "Humidity"; source = "RH_PT1H_AVG"; prior_lapse_rate = 0;
				  qc_threshold = 15; lolimit = 0; hilimit = 100; },
				{ name = "WindSpeedMS"; source = "WS_PT10M_AVG"; prior_lapse_rate = 0;
				  qc_threshold = 5; lolimit = 0; oi_sigma_h = 300; oi_error_ratio = 0.7; }
			);
		};
	};
};
```

Parameter options:

| Setting | Default | Meaning |
|---------|---------|---------|
| `method` | `terrain` | `terrain`: height and TPI regression with height-aware optimal interpolation of the residuals, for temperatures. `barnes`: damped Barnes without terrain, for reduced pressure and other smooth fields. |
| `residuals` | per method | `oi` (the `terrain` method) or `barnes` (the `barnes` method): overrides the residual analysis, see below |
| `oi_scales` | [120, 50] | km, `oi` only: the covariance of the residuals is the mean of Gaussians of these scales |
| `oi_sigma_h` | 500 | m, `oi` only: height scale of the covariance, 0 for horizontal only (the `barnes` method sets 0) |
| `oi_error_ratio` | 0.5 | `oi` only: observation error variance relative to the residual variance; larger values smooth more |
| `qc` | true | Gross error check |
| `qc_threshold` | 6 | Minimum rejection threshold in the units of the parameter |
| `lolimit`, `hilimit` | none | Physical range of the values, see below |
| `prior_lapse_rate` | −6.5 | Prior of the height coefficient of the `terrain` method in units per km, meant for temperatures; 0 for other parameters |
| `residual_sigmas` | [60, 25] | km, `barnes` only: the residual passes |
| `damping` | 0.25 | `barnes` only: station equivalents in the denominator |
| `local_sigma`, `regional_sigma` | 150, 600 | km, the regression scales |
| `bilateral_sigma_h` | 300 | m, `barnes` only: height scale of the residual passes, 0 for horizontal only |
| `background` | none | A forecast or other gridded field which the observations correct, see below |

Numbers may be written as integers or decimals.

### Residual analysis

The station residuals from the terrain trend are interpolated with optimal interpolation
(simple kriging with a fixed covariance) by default. Unlike Barnes passes it weights clustered
stations by their redundancy, and far from the stations the correction relaxes to the trend.
In cross-validation of 85 European cases it was better than the earlier two Barnes passes in
81–84 cases per quantity, at about the same cost:

| Quantity | Barnes passes | Optimal interpolation |
|----------|---------------|-----------------------|
| Temperature | 1.602 K | 1.572 K |
| Relative humidity | 8.735 % | 8.599 % |
| Wind speed | 1.743 m/s | 1.699 m/s (`oi_sigma_h = 300; oi_error_ratio = 0.7;`) |

The gain is small compared with what the terrain trend itself gives over simpler methods
(2.44 K for plain Barnes, 1.98 K for a fixed lapse rate). `residuals = "barnes"` restores the
earlier Barnes passes, which use `residual_sigmas`, `damping` and `bilateral_sigma_h`. The
`barnes` method keeps the Barnes passes.

### Limited values

Parameters with a physical range, such as relative humidity (0...100 %), wind speed (0
upwards) or friction (0...1), set `lolimit` and/or `hilimit`. The regression, the residual
passes and the correction of a background can all go out of range even when every
observation is valid: a height trend fitted through saturated stations extrapolates beyond
100 % up the slopes, and a negative trend in calm valleys turns the calm stations into
positive residuals which spread as wind to their neighbours. The limits are therefore applied
inside the analysis, after the trend and after every residual pass, and not only to the
result; with a background they apply to the values, not to the corrections. The gross error
check compares the stations with the limited predictions, so that a station at a limit is not
rejected for disagreeing with a trend beyond it.

In cross-validation of 85 European cases the terrain method with `prior_lapse_rate = 0` was
better than the `barnes` method for both relative humidity (8.74 against 9.41 % RMS) and wind
speed (1.74 against 1.81 m/s), and the limits removed the 0.4–0.5 % of the predictions out of
range. The default prior −6.5 per km was no better at the stations, and away from them it
would impose a temperature lapse rate on quantities it does not apply to. The gain at the stations is small, since the field goes out of range mostly away from
them; see `gis/docs/gis-pointgridding.md` for the numbers.

Observations slightly out of range, such as 101 % from a humidity sensor, are clamped to the
limits. Values farther out than `qc_threshold` are rejected as gross errors when `qc` is on.
The limits are also the range of the parameter in the gridded querydata.

The gross error check of a limited parameter needs a threshold in its own units, the default 6
is meant for temperatures. A linear height trend through a saturating profile undershoots
near the saturation level, so a too small threshold rejects correct saturated stations.

### Correcting a background

With a `background` the observations correct a gridded querydata producer, typically the
latest forecast, instead of being gridded on their own. Where there are no observations, for
example over the seas, the result is the background:

```
{
	name = "Temperature";
	background =
	{
		producer   = "ecmwf_europe";   # gridded querydata producer
		parameter  = "Temperature";    # default: the gridded parameter
		orography  = "GeopHeight";     # model orography (m) in the same producer, optional
		lapse_rate = -6.5;             # K/km, default -6.5, or 0 for the barnes method
		weight     = 0.7;              # weight of the corrected background
		taper      = 50.0;             # km, fading out at the edges of the background data
	};
}
```

The newest model run containing the requested time is used, interpolated to the grid. The
background is moved from the model orography to the terrain heights with the lapse rate; without
`orography` the model is assumed to see the same terrain. Among the stations the corrected
background is blended with the analysis of the observations alone by `weight` (0.7 gave the
best scores with IFS forecasts, see `gis/docs/gis-pointgridding.md`); far from the stations
the corrected background is used alone, and where the background is missing the
observations alone are used. The background fades out over about `taper` km from the edges of
its data, so that the two do not meet in a seam. A new model run changes the cache key of the gridded data.

Producers reading observations need the observation engine; a plugin built without it accepts
only point querydata producers. Wind speed can be gridded as a scalar with `lolimit = 0`, but
wind direction cannot be gridded yet: it should be gridded as vector components.

## 3. Terrain rasters

The terrain raster defines the grid and its coordinate system. It must be north-up with square
cells, and the coordinate system should be equal-area. Missing values and negative heights are
taken to be sea level. For example a 2.5 km Europe grid from ETOPO 2022:

```
gdalwarp -t_srs EPSG:3035 -te 2000000 1200000 7000000 5600000 -tr 2500 2500 \
    -r average -ot Float32 -co COMPRESS=DEFLATE -co PREDICTOR=3 \
    ETOPO_2022_v1_60s_N90W180_surface.tif europe_3035_2500.tif
```

Averaging gives the cell mean heights, which is what a grid cell represents. The raster is read
when the plugin starts; the terrain smoothing takes about 0.1 s for the grid above.

## 4. Sources of observations

**Observation engine.** All stations of the station type inside the WGS84 bounding box of the
grid are read, with their `elevation`. With `time_tolerance` the observation closest to the
requested time is used.

**Point querydata.** Used in international operations where observations come as querydata.
Querydata has no station elevations, so they are read from the `station_elevations` file by
station number, and from the station metadata of the observation engine station type
`elevation_stationtype` by WMO number (the file takes precedence). The latter assumes the
station numbers of the querydata are WMO numbers, as in FMI's international SYNOP data. The
number of elevations found is logged at startup. Stations without an elevation get the height
of the terrain cell, which underestimates summit stations by up to a kilometre and spoils the
lapse rate estimate in mountains.

Stations farther than 5 degrees outside the bounding box are ignored.

## 5. Code

| File | Contents |
|------|----------|
| `gridding/Gridder.{h,cpp}` | Producers, times, reading, gridding into querydata, cache |
| `gridding/Config.{h,cpp}` | The `gridding` settings |
| `gridding/Area.{h,cpp}` | Terrain raster → grid, `Fmi::PointGridding::Terrain`, WGS84 bounding box |
| `gridding/Sources.{h,cpp}` | Reading the observation engine and point querydata |
| `ogc/ObsGridLayer.{h,cpp}` | Capabilities of gridded producers |
| `State.cpp` | `getModel` and `getModelHashValue` for gridded producers |

The `Plugin` owns the `Gridder`, created after the engines when the configuration has a
`gridding` group.

## 6. Testing

The test configuration grids the European SYNOP point querydata of 2008-12-05 from
smartmet-test-data and the Finnish observations of the test database with a 20 km Europe
terrain (`test/cnf/gridding/europe_3035_20km.tif`). The tests `obsgrid_*`,
`wms_getmap_obsgrid_t2m`, `tiles_gettile_obsgrid` and `tiles_gettile_geotiff_obsgrid` draw and
deliver them. `obsgrid_humidity`, `tiles_gettile_geotiff_obsgrid_humidity` and
`obsgrid_windspeed` grid limited parameters, and draw values out of range in red.

The `obsgrid_bg_*` tests correct a background and draw the background alone for comparison:

- `fmi_hirlam`: 189 FMI stations of 2013-03-19 12 UTC (`test/data/gridding/fmi_observations`,
  from the cldb archive with `obs-gridding/fetch_archive_obs.py`) correct the HIRLAM test
  forecast, which has its own orography (`GeopHeight`), on a 5 km Finland terrain
  (`test/cnf/gridding/finland_3035_5km.tif`). The cropped test HIRLAM is a strip only about 400 km wide,
  so the test also shows the background fading out at the edges of its data.
- `ifs_europe_corrected`: 2966 European stations of 2026-01-21 06 UTC correct the +24 h ECMWF
  IFS open data forecast (CC-BY-4.0, `test/data/gridding/ifs_europe`, made with
  `obs-gridding/make_ifs_fixture.py`), whose orography is derived from its surface and sea
  level pressures. The pressure tests show the correction fading out over the seas. Their requests and outputs are shown in the [Dali](examples/dali.md#gridded-observations),
[WMS](examples/wms.md#gridded-observations) and
[OGC API Tiles](examples/tiles.md#tiles_gettile_obsgrid) examples.
