# Map Projections

Any PROJ projection string can be supplied as the `p.crs` URL parameter on the `/dali` endpoint.  The test suite exercises the full set of projections supported by the installed PROJ library using the `map` test product.

## Test Product

The `map` product ([`test/dali/customers/test/products/map.json`](../../test/dali/customers/test/products/map.json)) renders:

- A blue ocean polygon covering the full world extent
- Natural Earth `admin_0_countries` coastlines and land fills
- A 10° graticule grid

Additional per-test parameters override the projection's center (`p.cx`, `p.cy`), output size (`p.xsize`, `p.ysize`), and resolution (`p.resolution`, in km/pixel).  Test inputs are in [`test/input/map_*.get`](../../test/input/) and PNG renderings in [`docs/images/maps/`](../images/maps/).

## Unsupported Projections

The following projections are listed in [`test/input/.testignore`](../../test/input/.testignore) and excluded from the test suite.

**PROJ 7 rendering defect** (interrupts placed at wrong locations):

| Test | PROJ String |
|------|-------------|
| `map_isea` | `+proj=isea` |

**Not working — given up on:**

| Test | PROJ String |
|------|-------------|
| `map_aeqd` | `+proj=aeqd` |
| `map_bertin1953` | `+proj=bertin1953` |
| `map_bipc` | `+proj=bipc +ns` |
| `map_ccon` | `+proj=ccon +lat_1=60 +lat_2=70` |
| `map_chamb` | `+proj=chamb +lat_1=45 +lon_1=-30 +lon_2=30` |
| `map_gnom` | `+proj=gnom` |
| `map_imw_p` | `+proj=imw_p +lat_1=30 +lat_2=60` |
| `map_lsat` | `+proj=lsat +lsat=1 +path=1` |
| `map_murd2` | `+proj=murd2 +lat_1=30 +lat_2=50` |
| `map_nicol` | `+proj=nicol` |
| `map_nsper` | `+proj=nsper +h=1000000 +lat_0=60 +lon_0=25` |
| `map_ob_tran` | `+proj=ob_tran +o_proj=latlong +o_lat_p=45 +o_lon_p=0 +lon_0=0` |
| `map_omerc` | `+proj=omerc +lat_0=45 +lonc=20 +alpha=15 +k=1 +x_0=0 +y_0=0` |
| `map_pconic` | `+proj=pconic +lat_1=30 +lat_2=60` |
| `map_peirce_q` | `+proj=peirce_q` |
| `map_roteqc` | `+proj=ob_tran +o_proj=eqc +o_lon_p=0 +o_lat_p=30 +lon_0=0` |
| `map_rotlatlon` | `+proj=ob_tran +o_proj=longlat +o_lon_p=0 +o_lat_p=30 +lon_0=0` |
| `map_tcc` | `+proj=tcc` |
| `map_tcea` | `+proj=tcea` |

Note: `map_bipc`, `map_roteqc`, and `map_rotlatlon` do produce PNG output (shown in the alphabetical index below) but their rendering is not considered reliable.

---

## Projections by Type

### Cylindrical

Cylindrical projections map the sphere onto a cylinder.

| Test | PROJ String / EPSG | Description | Output |
|------|-------------------|-------------|--------|
| `map_merc` | `+proj=merc` | [Mercator](https://proj.org/en/stable/operations/projections/merc.html) — conformal, shapes preserved, area greatly distorted near poles | ![map_merc](../images/maps/map_merc.png) |
| `map_webmercator` | `EPSG:3857` | [Web Mercator](https://proj.org/en/stable/operations/projections/merc.html) — de-facto standard for web tile services | ![map_webmercator](../images/maps/map_webmercator.png) |
| `map_tmerc` | `+proj=tmerc` | [Transverse Mercator](https://proj.org/en/stable/operations/projections/tmerc.html) — conformal, accurate along a central meridian | ![map_tmerc](../images/maps/map_tmerc.png) |
| `map_gstmerc` | `+proj=gstmerc` | [Gauss-Schreiber Transverse Mercator](https://proj.org/en/stable/operations/projections/gstmerc.html) | ![map_gstmerc](../images/maps/map_gstmerc.png) |
| `map_tobmerc` | `+proj=tobmerc` | [Tobler-Mercator](https://proj.org/en/stable/operations/projections/tobmerc.html) — variation with reduced polar distortion | ![map_tobmerc](../images/maps/map_tobmerc.png) |
| `map_eqc` | `EPSG:4087` | [Equidistant Cylindrical](https://proj.org/en/stable/operations/projections/eqc.html) — plate carrée variant, meridians and parallels equally spaced | ![map_eqc](../images/maps/map_eqc.png) |
| `map_cc` | `+proj=cc` | [Central Cylindrical](https://proj.org/en/stable/operations/projections/cc.html) — perspective projection from the Earth's centre onto a cylinder | ![map_cc](../images/maps/map_cc.png) |
| `map_cea` | `+proj=cea` | [Equal-Area Cylindrical](https://proj.org/en/stable/operations/projections/cea.html) — Lambert cylindrical, preserves area | ![map_cea](../images/maps/map_cea.png) |
| `map_mill` | `+proj=mill` | [Miller Cylindrical](https://proj.org/en/stable/operations/projections/mill.html) — compromise between Mercator and equal-area | ![map_mill](../images/maps/map_mill.png) |
| `map_comill` | `+proj=comill` | [Compact Miller](https://proj.org/en/stable/operations/projections/comill.html) — reduced polar extent | ![map_comill](../images/maps/map_comill.png) |
| `map_gall` | `+proj=gall` | [Gall Stereographic](https://proj.org/en/stable/operations/projections/gall.html) — cylindrical with standard parallels at ±45° | ![map_gall](../images/maps/map_gall.png) |
| `map_patterson` | `+proj=patterson` | [Patterson Cylindrical](https://proj.org/en/stable/operations/projections/patterson.html) — modern compromise projection | ![map_patterson](../images/maps/map_patterson.png) |
| `map_ocea` | `+proj=ocea` | [Oblique Cylindrical Equal-Area](https://proj.org/en/stable/operations/projections/ocea.html) | ![map_ocea](../images/maps/map_ocea.png) |

---

### Conic

Conic projections are well-suited for mid-latitude regions.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_lcc` | `+proj=lcc +lon_0=-90 +lat_1=33 +lat_2=45` | [Lambert Conformal Conic](https://proj.org/en/stable/operations/projections/lcc.html) — conformal, standard for aviation and NWP | ![map_lcc](../images/maps/map_lcc.png) |
| `map_lcca` | `+proj=lcca +lat_0=35` | [Lambert Conformal Conic Alternative](https://proj.org/en/stable/operations/projections/lcca.html) | ![map_lcca](../images/maps/map_lcca.png) |
| `map_aea` | `+proj=aea +lat_1=29.5 +lat_2=42.5` | [Albers Equal-Area Conic](https://proj.org/en/stable/operations/projections/aea.html) — equal-area, used for US thematic maps | ![map_aea](../images/maps/map_aea.png) |
| `map_leac` | `+proj=leac` | [Lambert Equal-Area Conic](https://proj.org/en/stable/operations/projections/leac.html) | ![map_leac](../images/maps/map_leac.png) |
| `map_eqdc` | `+proj=eqdc +lat_1=55 +lat_2=60` | [Equidistant Conic](https://proj.org/en/stable/operations/projections/eqdc.html) — preserves distances along meridians | ![map_eqdc](../images/maps/map_eqdc.png) |
| `map_bonne` | `+proj=bonne +lat_1=10` | [Bonne](https://proj.org/en/stable/operations/projections/bonne.html) — pseudo-conic equal-area | ![map_bonne](../images/maps/map_bonne.png) |
| `map_cass` | `+proj=cass` | [Cassini-Soldner](https://proj.org/en/stable/operations/projections/cass.html) — transverse equidistant cylindrical | ![map_cass](../images/maps/map_cass.png) |
| `map_poly` | `+proj=poly` | [American Polyconic](https://proj.org/en/stable/operations/projections/poly.html) | ![map_poly](../images/maps/map_poly.png) |
| `map_rpoly` | `+proj=rpoly` | [Rectangular Polyconic](https://proj.org/en/stable/operations/projections/rpoly.html) | ![map_rpoly](../images/maps/map_rpoly.png) |
| `map_euler` | `+proj=euler +lat_1=67 +lat_2=75` | [Euler](https://proj.org/en/stable/operations/projections/euler.html) | ![map_euler](../images/maps/map_euler.png) |
| `map_tissot` | `+proj=tissot +lat_1=60 +lat_2=65` | [Tissot Conic](https://proj.org/en/stable/operations/projections/tissot.html) | ![map_tissot](../images/maps/map_tissot.png) |
| `map_murd1` | `+proj=murd1 +lat_1=30 +lat_2=50` | [Murdoch I](https://proj.org/en/stable/operations/projections/murd1.html) | ![map_murd1](../images/maps/map_murd1.png) |
| `map_murd3` | `+proj=murd3 +lat_1=30 +lat_2=50` | [Murdoch III](https://proj.org/en/stable/operations/projections/murd3.html) | ![map_murd3](../images/maps/map_murd3.png) |

---

### Azimuthal

Azimuthal projections preserve directions from the projection centre.

| Test | PROJ String / EPSG | Description | Output |
|------|-------------------|-------------|--------|
| `map_laea` | `+proj=laea` | [Lambert Azimuthal Equal-Area](https://proj.org/en/stable/operations/projections/laea.html) — equal-area, used for continental maps | ![map_laea](../images/maps/map_laea.png) |
| `map_stere` | `+proj=stere +lat_0=90 +lat_ts=75` | [Polar Stereographic](https://proj.org/en/stable/operations/projections/stere.html) — conformal, standard for polar regions | ![map_stere](../images/maps/map_stere.png) |
| `map_sterea` | `+proj=sterea +lat_0=90` | [Oblique Stereographic Alternative](https://proj.org/en/stable/operations/projections/sterea.html) — Dutch variant used for the Netherlands | ![map_sterea](../images/maps/map_sterea.png) |
| `map_arctic` | `EPSG:3995` | [Arctic Polar Stereographic](https://proj.org/en/stable/operations/projections/stere.html) — standard for Arctic products | ![map_arctic](../images/maps/map_arctic.png) |
| `map_antarctic` | `EPSG:3031` | [Antarctic Polar Stereographic](https://proj.org/en/stable/operations/projections/stere.html) — standard for Antarctic products | ![map_antarctic](../images/maps/map_antarctic.png) |
| `map_ups` | `+proj=ups` | [Universal Polar Stereographic](https://proj.org/en/stable/operations/projections/ups.html) — polar component of the UPS military system | ![map_ups](../images/maps/map_ups.png) |
| `map_ortho` | `+proj=ortho` | [Orthographic](https://proj.org/en/stable/operations/projections/ortho.html) — perspective view from infinity; globe appearance | ![map_ortho](../images/maps/map_ortho.png) |
| `map_airy` | `+proj=airy` | [Airy](https://proj.org/en/stable/operations/projections/airy.html) — minimum-error azimuthal | ![map_airy](../images/maps/map_airy.png) |
| `map_rouss` | `+proj=rouss` | [Roussilhe Stereographic](https://proj.org/en/stable/operations/projections/rouss.html) | ![map_rouss](../images/maps/map_rouss.png) |
| `map_tpers` | `+proj=tpers +h=5500000 +lat_0=40` | [Tilted Perspective](https://proj.org/en/stable/operations/projections/tpers.html) — satellite view from finite altitude with tilt | ![map_tpers](../images/maps/map_tpers.png) |
| `map_tpeqd` | `+proj=tpeqd +lat_1=60 +lat_2=65` | [Two-Point Equidistant](https://proj.org/en/stable/operations/projections/tpeqd.html) | ![map_tpeqd](../images/maps/map_tpeqd.png) |
| `map_aitoff` | `+proj=aitoff` | [Aitoff](https://proj.org/en/stable/operations/projections/aitoff.html) — modified azimuthal, world map | ![map_aitoff](../images/maps/map_aitoff.png) |
| `map_hammer` | `+proj=hammer` | [Hammer & Eckert-Greifendorff](https://proj.org/en/stable/operations/projections/hammer.html) — equal-area modified azimuthal | ![map_hammer](../images/maps/map_hammer.png) |

---

### Pseudocylindrical

Pseudocylindrical projections have straight parallels but curved meridians.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_sinu` | `+proj=sinu` | [Sinusoidal (Sanson-Flamsteed)](https://proj.org/en/stable/operations/projections/sinu.html) — equal-area, simple formula | ![map_sinu](../images/maps/map_sinu.png) |
| `map_moll` | `+proj=moll` | [Mollweide](https://proj.org/en/stable/operations/projections/moll.html) — equal-area ellipse | ![map_moll](../images/maps/map_moll.png) |
| `map_robin` | `+proj=robin` | [Robinson](https://proj.org/en/stable/operations/projections/robin.html) — compromise, formerly used by National Geographic | ![map_robin](../images/maps/map_robin.png) |
| `map_natearth` | `+proj=natearth` | [Natural Earth](https://proj.org/en/stable/operations/projections/natearth.html) — smooth compromise, widely used for thematic maps | ![map_natearth](../images/maps/map_natearth.png) |
| `map_natearth2` | `+proj=natearth2` | [Natural Earth II](https://proj.org/en/stable/operations/projections/natearth2.html) — variation with flatter poles | ![map_natearth2](../images/maps/map_natearth2.png) |
| `map_natearth_shifted` | `+proj=natearth +lon_0=90` | Natural Earth (Pacific-centred) | ![map_natearth_shifted](../images/maps/map_natearth_shifted.png) |
| `map_eqearth` | `+proj=eqearth` | [Equal Earth](https://proj.org/en/stable/operations/projections/eqearth.html) — equal-area, published 2018 | ![map_eqearth](../images/maps/map_eqearth.png) |
| `map_wintri` | `+proj=wintri` | [Winkel Tripel](https://proj.org/en/stable/operations/projections/wintri.html) — compromise minimising distance/area/angle distortions; used by National Geographic since 1998 | ![map_wintri](../images/maps/map_wintri.png) |
| `map_wink1` | `+proj=wink1` | [Winkel I](https://proj.org/en/stable/operations/projections/wink1.html) | ![map_wink1](../images/maps/map_wink1.png) |
| `map_wink2` | `+proj=wink2` | [Winkel II](https://proj.org/en/stable/operations/projections/wink2.html) | ![map_wink2](../images/maps/map_wink2.png) |
| `map_eck1` | `+proj=eck1` | [Eckert I](https://proj.org/en/stable/operations/projections/eck1.html) | ![map_eck1](../images/maps/map_eck1.png) |
| `map_eck2` | `+proj=eck2` | [Eckert II](https://proj.org/en/stable/operations/projections/eck2.html) | ![map_eck2](../images/maps/map_eck2.png) |
| `map_eck3` | `+proj=eck3` | [Eckert III](https://proj.org/en/stable/operations/projections/eck3.html) | ![map_eck3](../images/maps/map_eck3.png) |
| `map_eck4` | `+proj=eck4` | [Eckert IV](https://proj.org/en/stable/operations/projections/eck4.html) — equal-area | ![map_eck4](../images/maps/map_eck4.png) |
| `map_eck5` | `+proj=eck5` | [Eckert V](https://proj.org/en/stable/operations/projections/eck5.html) | ![map_eck5](../images/maps/map_eck5.png) |
| `map_eck6` | `+proj=eck6` | [Eckert VI](https://proj.org/en/stable/operations/projections/eck6.html) — equal-area | ![map_eck6](../images/maps/map_eck6.png) |
| `map_wag1` | `+proj=wag1` | [Wagner I](https://proj.org/en/stable/operations/projections/wag1.html) — equal-area | ![map_wag1](../images/maps/map_wag1.png) |
| `map_wag2` | `+proj=wag2` | [Wagner II](https://proj.org/en/stable/operations/projections/wag2.html) | ![map_wag2](../images/maps/map_wag2.png) |
| `map_wag3` | `+proj=wag3` | [Wagner III](https://proj.org/en/stable/operations/projections/wag3.html) | ![map_wag3](../images/maps/map_wag3.png) |
| `map_wag4` | `+proj=wag4` | [Wagner IV](https://proj.org/en/stable/operations/projections/wag4.html) — equal-area | ![map_wag4](../images/maps/map_wag4.png) |
| `map_wag5` | `+proj=wag5` | [Wagner V](https://proj.org/en/stable/operations/projections/wag5.html) | ![map_wag5](../images/maps/map_wag5.png) |
| `map_wag6` | `+proj=wag6` | [Wagner VI](https://proj.org/en/stable/operations/projections/wag6.html) | ![map_wag6](../images/maps/map_wag6.png) |
| `map_wag7` | `+proj=wag7` | [Wagner VII](https://proj.org/en/stable/operations/projections/wag7.html) — equal-area azimuthal hybrid | ![map_wag7](../images/maps/map_wag7.png) |
| `map_kav5` | `+proj=kav5` | [Kavraiskiy V](https://proj.org/en/stable/operations/projections/kav5.html) | ![map_kav5](../images/maps/map_kav5.png) |
| `map_kav7` | `+proj=kav7` | [Kavraiskiy VII](https://proj.org/en/stable/operations/projections/kav7.html) | ![map_kav7](../images/maps/map_kav7.png) |
| `map_crast` | `+proj=crast` | [Craster Parabolic](https://proj.org/en/stable/operations/projections/crast.html) — equal-area | ![map_crast](../images/maps/map_crast.png) |
| `map_boggs` | `+proj=boggs` | [Boggs Eumorphic](https://proj.org/en/stable/operations/projections/boggs.html) — equal-area | ![map_boggs](../images/maps/map_boggs.png) |
| `map_hatano` | `+proj=hatano` | [Hatano Asymmetrical Equal-Area](https://proj.org/en/stable/operations/projections/hatano.html) — different standard parallels in N and S hemispheres | ![map_hatano](../images/maps/map_hatano.png) |
| `map_fouc` | `+proj=fouc` | [Foucaut](https://proj.org/en/stable/operations/projections/fouc.html) — equal-area | ![map_fouc](../images/maps/map_fouc.png) |
| `map_fouc_s` | `+proj=fouc_s` | [Foucaut Sinusoidal](https://proj.org/en/stable/operations/projections/fouc_s.html) — blend of sinusoidal and cylindrical | ![map_fouc_s](../images/maps/map_fouc_s.png) |
| `map_qua_aut` | `+proj=qua_aut` | [Quartic Authalic](https://proj.org/en/stable/operations/projections/qua_aut.html) — equal-area | ![map_qua_aut](../images/maps/map_qua_aut.png) |
| `map_nell` | `+proj=nell` | [Nell](https://proj.org/en/stable/operations/projections/nell.html) | ![map_nell](../images/maps/map_nell.png) |
| `map_nell_h` | `+proj=nell_h` | [Nell-Hammer](https://proj.org/en/stable/operations/projections/nell_h.html) | ![map_nell_h](../images/maps/map_nell_h.png) |
| `map_loxim` | `+proj=loxim` | [Loximuthal](https://proj.org/en/stable/operations/projections/loxim.html) | ![map_loxim](../images/maps/map_loxim.png) |
| `map_putp1` | `+proj=putp1` | [Putnins P1](https://proj.org/en/stable/operations/projections/putp1.html) | ![map_putp1](../images/maps/map_putp1.png) |
| `map_putp2` | `+proj=putp2` | [Putnins P2](https://proj.org/en/stable/operations/projections/putp2.html) | ![map_putp2](../images/maps/map_putp2.png) |
| `map_putp3` | `+proj=putp3` | [Putnins P3](https://proj.org/en/stable/operations/projections/putp3.html) | ![map_putp3](../images/maps/map_putp3.png) |
| `map_putp3p` | `+proj=putp3p` | [Putnins P3'](https://proj.org/en/stable/operations/projections/putp3p.html) | ![map_putp3p](../images/maps/map_putp3p.png) |
| `map_putp4p` | `+proj=putp4p` | [Putnins P4'](https://proj.org/en/stable/operations/projections/putp4p.html) | ![map_putp4p](../images/maps/map_putp4p.png) |
| `map_putp5` | `+proj=putp5` | [Putnins P5](https://proj.org/en/stable/operations/projections/putp5.html) | ![map_putp5](../images/maps/map_putp5.png) |
| `map_putp5p` | `+proj=putp5p` | [Putnins P5'](https://proj.org/en/stable/operations/projections/putp5p.html) | ![map_putp5p](../images/maps/map_putp5p.png) |
| `map_putp6` | `+proj=putp6` | [Putnins P6](https://proj.org/en/stable/operations/projections/putp6.html) | ![map_putp6](../images/maps/map_putp6.png) |
| `map_putp6p` | `+proj=putp6p` | [Putnins P6'](https://proj.org/en/stable/operations/projections/putp6p.html) | ![map_putp6p](../images/maps/map_putp6p.png) |
| `map_collg` | `+proj=collg` | [Collignon](https://proj.org/en/stable/operations/projections/collg.html) — equal-area triangle shape | ![map_collg](../images/maps/map_collg.png) |
| `map_denoy` | `+proj=denoy` | [Denoyer Semi-Elliptical](https://proj.org/en/stable/operations/projections/denoy.html) | ![map_denoy](../images/maps/map_denoy.png) |
| `map_fahey` | `+proj=fahey` | [Fahey](https://proj.org/en/stable/operations/projections/fahey.html) | ![map_fahey](../images/maps/map_fahey.png) |
| `map_gn_sinu` | `+proj=gn_sinu +m=2 +n=3` | [General Sinusoidal Series](https://proj.org/en/stable/operations/projections/gn_sinu.html) | ![map_gn_sinu](../images/maps/map_gn_sinu.png) |
| `map_urmfps` | `+proj=urmfps +n=0.5` | [Urmaev Flat-Polar Sinusoidal](https://proj.org/en/stable/operations/projections/urmfps.html) | ![map_urmfps](../images/maps/map_urmfps.png) |
| `map_urm5` | `+proj=urm5 +n=0.9 +alpha=2 +q=4` | [Urmaev V](https://proj.org/en/stable/operations/projections/urm5.html) | ![map_urm5](../images/maps/map_urm5.png) |
| `map_weren` | `+proj=weren` | [Werenskiold I](https://proj.org/en/stable/operations/projections/weren.html) | ![map_weren](../images/maps/map_weren.png) |
| `map_larr` | `+proj=larr` | [Larrivée](https://proj.org/en/stable/operations/projections/larr.html) | ![map_larr](../images/maps/map_larr.png) |
| `map_lask` | `+proj=lask` | [Laskowski](https://proj.org/en/stable/operations/projections/lask.html) | ![map_lask](../images/maps/map_lask.png) |
| `map_times` | `+proj=times` | [Times](https://proj.org/en/stable/operations/projections/times.html) | ![map_times](../images/maps/map_times.png) |
| `map_mbt_fps` | `+proj=mbt_fps` | [McBryde-Thomas Flat-Polar Sinusoidal](https://proj.org/en/stable/operations/projections/mbt_fps.html) — equal-area | ![map_mbt_fps](../images/maps/map_mbt_fps.png) |
| `map_mbtfps` | `+proj=mbtfps` | [McBryde-Thomas Flat-Polar Sinusoidal (alt.)](https://proj.org/en/stable/operations/projections/mbtfps.html) — equal-area | ![map_mbtfps](../images/maps/map_mbtfps.png) |
| `map_mbt_s` | `+proj=mbt_s` | [McBryde-Thomas Flat-Polar Sinusoidal (var.)](https://proj.org/en/stable/operations/projections/mbt_s.html) — equal-area | ![map_mbt_s](../images/maps/map_mbt_s.png) |
| `map_mbtfpp` | `+proj=mbtfpp` | [McBryde-Thomas Flat-Polar Parabolic](https://proj.org/en/stable/operations/projections/mbtfpp.html) — equal-area | ![map_mbtfpp](../images/maps/map_mbtfpp.png) |
| `map_mbtfpq` | `+proj=mbtfpq` | [McBryde-Thomas Flat-Polar Quartic](https://proj.org/en/stable/operations/projections/mbtfpq.html) — equal-area | ![map_mbtfpq](../images/maps/map_mbtfpq.png) |

---

### Globular and Polyhedral

These projections use unconventional boundaries for artistic or special purposes.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_adams_hemi` | `+proj=adams_hemi` | [Adams Hemisphere-in-a-Square](https://proj.org/en/stable/operations/projections/adams_hemi.html) — conformal, entire hemisphere in a square | ![map_adams_hemi](../images/maps/map_adams_hemi.png) |
| `map_adams_ws1` | `+proj=adams_ws1` | [Adams World in a Square I](https://proj.org/en/stable/operations/projections/adams_ws1.html) — conformal world map in a square | ![map_adams_ws1](../images/maps/map_adams_ws1.png) |
| `map_adams_ws2` | `+proj=adams_ws2` | [Adams World in a Square II](https://proj.org/en/stable/operations/projections/adams_ws2.html) | ![map_adams_ws2](../images/maps/map_adams_ws2.png) |
| `map_apian` | `+proj=apian` | [Apian Globular I](https://proj.org/en/stable/operations/projections/apian.html) | ![map_apian](../images/maps/map_apian.png) |
| `map_bacon` | `+proj=bacon` | [Bacon Globular](https://proj.org/en/stable/operations/projections/bacon.html) | ![map_bacon](../images/maps/map_bacon.png) |
| `map_ortel` | `+proj=ortel` | [Ortelius Oval](https://proj.org/en/stable/operations/projections/ortel.html) | ![map_ortel](../images/maps/map_ortel.png) |
| `map_august` | `+proj=august` | [August Epicycloidal](https://proj.org/en/stable/operations/projections/august.html) — conformal world-in-a-circle | ![map_august](../images/maps/map_august.png) |
| `map_lagrng` | `+proj=lagrng` | [Lagrange](https://proj.org/en/stable/operations/projections/lagrng.html) — conformal in a circle | ![map_lagrng](../images/maps/map_lagrng.png) |
| `map_gins8` | `+proj=gins8` | [Ginsburg VIII](https://proj.org/en/stable/operations/projections/gins8.html) | ![map_gins8](../images/maps/map_gins8.png) |
| `map_healpix` | `+proj=healpix` | [HEALPix](https://proj.org/en/stable/operations/projections/healpix.html) — hierarchical equal-area pixelization; used in astrophysics and CMB analysis | ![map_healpix](../images/maps/map_healpix.png) |

---

### Van der Grinten

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_vandg` | `+proj=vandg` | [Van der Grinten I](https://proj.org/en/stable/operations/projections/vandg.html) — world in a circle | ![map_vandg](../images/maps/map_vandg.png) |
| `map_vandg2` | `+proj=vandg2` | [Van der Grinten II](https://proj.org/en/stable/operations/projections/vandg2.html) | ![map_vandg2](../images/maps/map_vandg2.png) |
| `map_vandg3` | `+proj=vandg3` | [Van der Grinten III](https://proj.org/en/stable/operations/projections/vandg3.html) | ![map_vandg3](../images/maps/map_vandg3.png) |
| `map_vandg4` | `+proj=vandg4` | [Van der Grinten IV](https://proj.org/en/stable/operations/projections/vandg4.html) | ![map_vandg4](../images/maps/map_vandg4.png) |

---

### Interrupted

Interrupted projections break the map along selected meridians to reduce distortion in landmasses or oceans.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_igh` | `+proj=igh` | [Goode Interrupted Homolosine](https://proj.org/en/stable/operations/projections/igh.html) — equal-area, interrupts over oceans | ![map_igh](../images/maps/map_igh.png) |
| `map_igh_o` | `+proj=igh_o +lon_0=-160` | [Goode Interrupted Homolosine (Oceans)](https://proj.org/en/stable/operations/projections/igh_o.html) — interrupts over land, shows ocean continuity | ![map_igh_o](../images/maps/map_igh_o.png) |
| `map_igh_shifted` | `+proj=igh +lon_0=90` | Goode Interrupted Homolosine (shifted 90° E) | ![map_igh_shifted](../images/maps/map_igh_shifted.png) |
| `map_goode` | `+proj=goode` | [Goode Homolosine (uninterrupted)](https://proj.org/en/stable/operations/projections/goode.html) | ![map_goode](../images/maps/map_goode.png) |

---

### Satellite and Perspective

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_geos` | `+proj=geos +h=35785831 +lon_0=-60 +sweep=y` | [Geostationary Satellite View](https://proj.org/en/stable/operations/projections/geos.html) — view from 35786 km altitude over the Atlantic | ![map_geos](../images/maps/map_geos.png) |
| `map_tpers` | `+proj=tpers +h=5500000 +lat_0=40` | [Tilted Perspective](https://proj.org/en/stable/operations/projections/tpers.html) — view from 5500 km altitude with tilt | ![map_tpers](../images/maps/map_tpers.png) |
| `map_mil_os` | `+proj=mil_os` | [Miller Oblated Stereographic](https://proj.org/en/stable/operations/projections/mil_os.html) — used for European maps | ![map_mil_os](../images/maps/map_mil_os.png) |

---

## Alphabetical Index

| Test | PROJ String / EPSG | PROJ Docs |
|------|-------------------|-----------|
| `map_adams_hemi` | `+proj=adams_hemi` | [Adams Hemisphere-in-a-Square](https://proj.org/en/stable/operations/projections/adams_hemi.html) |
| `map_adams_ws1` | `+proj=adams_ws1` | [Adams World in a Square I](https://proj.org/en/stable/operations/projections/adams_ws1.html) |
| `map_adams_ws2` | `+proj=adams_ws2` | [Adams World in a Square II](https://proj.org/en/stable/operations/projections/adams_ws2.html) |
| `map_aea` | `+proj=aea +lat_1=29.5 +lat_2=42.5` | [Albers Equal-Area Conic](https://proj.org/en/stable/operations/projections/aea.html) |
| `map_airy` | `+proj=airy` | [Airy](https://proj.org/en/stable/operations/projections/airy.html) |
| `map_aitoff` | `+proj=aitoff` | [Aitoff](https://proj.org/en/stable/operations/projections/aitoff.html) |
| `map_antarctic` | `EPSG:3031` | [Polar Stereographic (South)](https://proj.org/en/stable/operations/projections/stere.html) |
| `map_apian` | `+proj=apian` | [Apian Globular I](https://proj.org/en/stable/operations/projections/apian.html) |
| `map_arctic` | `EPSG:3995` | [Arctic Polar Stereographic](https://proj.org/en/stable/operations/projections/stere.html) |
| `map_august` | `+proj=august` | [August Epicycloidal](https://proj.org/en/stable/operations/projections/august.html) |
| `map_bacon` | `+proj=bacon` | [Bacon Globular](https://proj.org/en/stable/operations/projections/bacon.html) |
| `map_bipc` ⚠ | `+proj=bipc +ns` | [Bipolar Conic of Western Hemisphere](https://proj.org/en/stable/operations/projections/bipc.html) |
| `map_boggs` | `+proj=boggs` | [Boggs Eumorphic](https://proj.org/en/stable/operations/projections/boggs.html) |
| `map_bonne` | `+proj=bonne +lat_1=10` | [Bonne](https://proj.org/en/stable/operations/projections/bonne.html) |
| `map_cass` | `+proj=cass` | [Cassini-Soldner](https://proj.org/en/stable/operations/projections/cass.html) |
| `map_cc` | `+proj=cc` | [Central Cylindrical](https://proj.org/en/stable/operations/projections/cc.html) |
| `map_cea` | `+proj=cea` | [Equal-Area Cylindrical](https://proj.org/en/stable/operations/projections/cea.html) |
| `map_collg` | `+proj=collg` | [Collignon](https://proj.org/en/stable/operations/projections/collg.html) |
| `map_comill` | `+proj=comill` | [Compact Miller](https://proj.org/en/stable/operations/projections/comill.html) |
| `map_crast` | `+proj=crast` | [Craster Parabolic](https://proj.org/en/stable/operations/projections/crast.html) |
| `map_denoy` | `+proj=denoy` | [Denoyer Semi-Elliptical](https://proj.org/en/stable/operations/projections/denoy.html) |
| `map_eck1` | `+proj=eck1` | [Eckert I](https://proj.org/en/stable/operations/projections/eck1.html) |
| `map_eck2` | `+proj=eck2` | [Eckert II](https://proj.org/en/stable/operations/projections/eck2.html) |
| `map_eck3` | `+proj=eck3` | [Eckert III](https://proj.org/en/stable/operations/projections/eck3.html) |
| `map_eck4` | `+proj=eck4` | [Eckert IV](https://proj.org/en/stable/operations/projections/eck4.html) |
| `map_eck5` | `+proj=eck5` | [Eckert V](https://proj.org/en/stable/operations/projections/eck5.html) |
| `map_eck6` | `+proj=eck6` | [Eckert VI](https://proj.org/en/stable/operations/projections/eck6.html) |
| `map_eqc` | `EPSG:4087` | [Equidistant Cylindrical](https://proj.org/en/stable/operations/projections/eqc.html) |
| `map_eqdc` | `+proj=eqdc +lat_1=55 +lat_2=60` | [Equidistant Conic](https://proj.org/en/stable/operations/projections/eqdc.html) |
| `map_eqearth` | `+proj=eqearth` | [Equal Earth](https://proj.org/en/stable/operations/projections/eqearth.html) |
| `map_euler` | `+proj=euler +lat_1=67 +lat_2=75` | [Euler](https://proj.org/en/stable/operations/projections/euler.html) |
| `map_fahey` | `+proj=fahey` | [Fahey](https://proj.org/en/stable/operations/projections/fahey.html) |
| `map_fouc` | `+proj=fouc` | [Foucaut](https://proj.org/en/stable/operations/projections/fouc.html) |
| `map_fouc_s` | `+proj=fouc_s` | [Foucaut Sinusoidal](https://proj.org/en/stable/operations/projections/fouc_s.html) |
| `map_gall` | `+proj=gall` | [Gall Stereographic](https://proj.org/en/stable/operations/projections/gall.html) |
| `map_geos` | `+proj=geos +h=35785831 +lon_0=-60 +sweep=y` | [Geostationary Satellite View](https://proj.org/en/stable/operations/projections/geos.html) |
| `map_gins8` | `+proj=gins8` | [Ginsburg VIII](https://proj.org/en/stable/operations/projections/gins8.html) |
| `map_gn_sinu` | `+proj=gn_sinu +m=2 +n=3` | [General Sinusoidal Series](https://proj.org/en/stable/operations/projections/gn_sinu.html) |
| `map_goode` | `+proj=goode` | [Goode Homolosine](https://proj.org/en/stable/operations/projections/goode.html) |
| `map_gstmerc` | `+proj=gstmerc` | [Gauss-Schreiber Transverse Mercator](https://proj.org/en/stable/operations/projections/gstmerc.html) |
| `map_hammer` | `+proj=hammer` | [Hammer & Eckert-Greifendorff](https://proj.org/en/stable/operations/projections/hammer.html) |
| `map_hatano` | `+proj=hatano` | [Hatano Asymmetrical Equal-Area](https://proj.org/en/stable/operations/projections/hatano.html) |
| `map_healpix` | `+proj=healpix` | [HEALPix](https://proj.org/en/stable/operations/projections/healpix.html) |
| `map_igh` | `+proj=igh` | [Goode Interrupted Homolosine](https://proj.org/en/stable/operations/projections/igh.html) |
| `map_igh_o` | `+proj=igh_o +lon_0=-160` | [Goode Interrupted Homolosine (Oceans)](https://proj.org/en/stable/operations/projections/igh_o.html) |
| `map_igh_shifted` | `+proj=igh +lon_0=90` | [Goode Interrupted Homolosine (shifted)](https://proj.org/en/stable/operations/projections/igh.html) |
| `map_isea` ⚠ | `+proj=isea` | [Icosahedral Snyder Equal-Area](https://proj.org/en/stable/operations/projections/isea.html) |
| `map_kav5` | `+proj=kav5` | [Kavraiskiy V](https://proj.org/en/stable/operations/projections/kav5.html) |
| `map_kav7` | `+proj=kav7` | [Kavraiskiy VII](https://proj.org/en/stable/operations/projections/kav7.html) |
| `map_laea` | `+proj=laea` | [Lambert Azimuthal Equal-Area](https://proj.org/en/stable/operations/projections/laea.html) |
| `map_lagrng` | `+proj=lagrng` | [Lagrange](https://proj.org/en/stable/operations/projections/lagrng.html) |
| `map_larr` | `+proj=larr` | [Larrivée](https://proj.org/en/stable/operations/projections/larr.html) |
| `map_lask` | `+proj=lask` | [Laskowski](https://proj.org/en/stable/operations/projections/lask.html) |
| `map_lcc` | `+proj=lcc +lon_0=-90 +lat_1=33 +lat_2=45` | [Lambert Conformal Conic](https://proj.org/en/stable/operations/projections/lcc.html) |
| `map_lcca` | `+proj=lcca +lat_0=35` | [Lambert Conformal Conic Alternative](https://proj.org/en/stable/operations/projections/lcca.html) |
| `map_leac` | `+proj=leac` | [Lambert Equal-Area Conic](https://proj.org/en/stable/operations/projections/leac.html) |
| `map_loxim` | `+proj=loxim` | [Loximuthal](https://proj.org/en/stable/operations/projections/loxim.html) |
| `map_mbt_fps` | `+proj=mbt_fps` | [McBryde-Thomas Flat-Polar Sinusoidal](https://proj.org/en/stable/operations/projections/mbt_fps.html) |
| `map_mbt_s` | `+proj=mbt_s` | [McBryde-Thomas Flat-Polar Sinusoidal (var.)](https://proj.org/en/stable/operations/projections/mbt_s.html) |
| `map_mbtfpp` | `+proj=mbtfpp` | [McBryde-Thomas Flat-Polar Parabolic](https://proj.org/en/stable/operations/projections/mbtfpp.html) |
| `map_mbtfpq` | `+proj=mbtfpq` | [McBryde-Thomas Flat-Polar Quartic](https://proj.org/en/stable/operations/projections/mbtfpq.html) |
| `map_mbtfps` | `+proj=mbtfps` | [McBryde-Thomas Flat-Polar Sinusoidal (alt.)](https://proj.org/en/stable/operations/projections/mbtfps.html) |
| `map_merc` | `+proj=merc` | [Mercator](https://proj.org/en/stable/operations/projections/merc.html) |
| `map_mil_os` | `+proj=mil_os` | [Miller Oblated Stereographic](https://proj.org/en/stable/operations/projections/mil_os.html) |
| `map_mill` | `+proj=mill` | [Miller Cylindrical](https://proj.org/en/stable/operations/projections/mill.html) |
| `map_moll` | `+proj=moll` | [Mollweide](https://proj.org/en/stable/operations/projections/moll.html) |
| `map_murd1` | `+proj=murd1 +lat_1=30 +lat_2=50` | [Murdoch I](https://proj.org/en/stable/operations/projections/murd1.html) |
| `map_murd3` | `+proj=murd3 +lat_1=30 +lat_2=50` | [Murdoch III](https://proj.org/en/stable/operations/projections/murd3.html) |
| `map_natearth` | `+proj=natearth` | [Natural Earth](https://proj.org/en/stable/operations/projections/natearth.html) |
| `map_natearth2` | `+proj=natearth2` | [Natural Earth II](https://proj.org/en/stable/operations/projections/natearth2.html) |
| `map_natearth_shifted` | `+proj=natearth +lon_0=90` | Natural Earth (Pacific-centred) |
| `map_nell` | `+proj=nell` | [Nell](https://proj.org/en/stable/operations/projections/nell.html) |
| `map_nell_h` | `+proj=nell_h` | [Nell-Hammer](https://proj.org/en/stable/operations/projections/nell_h.html) |
| `map_ocea` | `+proj=ocea` | [Oblique Cylindrical Equal-Area](https://proj.org/en/stable/operations/projections/ocea.html) |
| `map_ortel` | `+proj=ortel` | [Ortelius Oval](https://proj.org/en/stable/operations/projections/ortel.html) |
| `map_ortho` | `+proj=ortho` | [Orthographic](https://proj.org/en/stable/operations/projections/ortho.html) |
| `map_patterson` | `+proj=patterson` | [Patterson Cylindrical](https://proj.org/en/stable/operations/projections/patterson.html) |
| `map_poly` | `+proj=poly` | [American Polyconic](https://proj.org/en/stable/operations/projections/poly.html) |
| `map_putp1` | `+proj=putp1` | [Putnins P1](https://proj.org/en/stable/operations/projections/putp1.html) |
| `map_putp2` | `+proj=putp2` | [Putnins P2](https://proj.org/en/stable/operations/projections/putp2.html) |
| `map_putp3` | `+proj=putp3` | [Putnins P3](https://proj.org/en/stable/operations/projections/putp3.html) |
| `map_putp3p` | `+proj=putp3p` | [Putnins P3'](https://proj.org/en/stable/operations/projections/putp3p.html) |
| `map_putp4p` | `+proj=putp4p` | [Putnins P4'](https://proj.org/en/stable/operations/projections/putp4p.html) |
| `map_putp5` | `+proj=putp5` | [Putnins P5](https://proj.org/en/stable/operations/projections/putp5.html) |
| `map_putp5p` | `+proj=putp5p` | [Putnins P5'](https://proj.org/en/stable/operations/projections/putp5p.html) |
| `map_putp6` | `+proj=putp6` | [Putnins P6](https://proj.org/en/stable/operations/projections/putp6.html) |
| `map_putp6p` | `+proj=putp6p` | [Putnins P6'](https://proj.org/en/stable/operations/projections/putp6p.html) |
| `map_qua_aut` | `+proj=qua_aut` | [Quartic Authalic](https://proj.org/en/stable/operations/projections/qua_aut.html) |
| `map_robin` | `+proj=robin` | [Robinson](https://proj.org/en/stable/operations/projections/robin.html) |
| `map_roteqc` ⚠ | `+proj=ob_tran +o_proj=eqc +o_lon_p=0 +o_lat_p=30 +lon_0=0` | [Rotated Equidistant Cylindrical](https://proj.org/en/stable/operations/projections/ob_tran.html) |
| `map_rotlatlon` ⚠ | `+proj=ob_tran +o_proj=longlat +o_lon_p=0 +o_lat_p=30 +lon_0=0` | [Rotated Geographic](https://proj.org/en/stable/operations/projections/ob_tran.html) |
| `map_rouss` | `+proj=rouss` | [Roussilhe Stereographic](https://proj.org/en/stable/operations/projections/rouss.html) |
| `map_rpoly` | `+proj=rpoly` | [Rectangular Polyconic](https://proj.org/en/stable/operations/projections/rpoly.html) |
| `map_sinu` | `+proj=sinu` | [Sinusoidal (Sanson-Flamsteed)](https://proj.org/en/stable/operations/projections/sinu.html) |
| `map_stere` | `+proj=stere +lat_0=90 +lat_ts=75` | [Stereographic (polar, true scale at 75°N)](https://proj.org/en/stable/operations/projections/stere.html) |
| `map_sterea` | `+proj=sterea +lat_0=90` | [Oblique Stereographic Alternative](https://proj.org/en/stable/operations/projections/sterea.html) |
| `map_times` | `+proj=times` | [Times](https://proj.org/en/stable/operations/projections/times.html) |
| `map_tissot` | `+proj=tissot +lat_1=60 +lat_2=65` | [Tissot Conic](https://proj.org/en/stable/operations/projections/tissot.html) |
| `map_tmerc` | `+proj=tmerc` | [Transverse Mercator](https://proj.org/en/stable/operations/projections/tmerc.html) |
| `map_tobmerc` | `+proj=tobmerc` | [Tobler-Mercator](https://proj.org/en/stable/operations/projections/tobmerc.html) |
| `map_tpeqd` | `+proj=tpeqd +lat_1=60 +lat_2=65` | [Two-Point Equidistant](https://proj.org/en/stable/operations/projections/tpeqd.html) |
| `map_tpers` | `+proj=tpers +h=5500000 +lat_0=40` | [Tilted Perspective](https://proj.org/en/stable/operations/projections/tpers.html) |
| `map_ups` | `+proj=ups` | [Universal Polar Stereographic](https://proj.org/en/stable/operations/projections/ups.html) |
| `map_urm5` | `+proj=urm5 +n=0.9 +alpha=2 +q=4` | [Urmaev V](https://proj.org/en/stable/operations/projections/urm5.html) |
| `map_urmfps` | `+proj=urmfps +n=0.5` | [Urmaev Flat-Polar Sinusoidal](https://proj.org/en/stable/operations/projections/urmfps.html) |
| `map_vandg` | `+proj=vandg` | [Van der Grinten I](https://proj.org/en/stable/operations/projections/vandg.html) |
| `map_vandg2` | `+proj=vandg2` | [Van der Grinten II](https://proj.org/en/stable/operations/projections/vandg2.html) |
| `map_vandg3` | `+proj=vandg3` | [Van der Grinten III](https://proj.org/en/stable/operations/projections/vandg3.html) |
| `map_vandg4` | `+proj=vandg4` | [Van der Grinten IV](https://proj.org/en/stable/operations/projections/vandg4.html) |
| `map_wag1` | `+proj=wag1` | [Wagner I](https://proj.org/en/stable/operations/projections/wag1.html) |
| `map_wag2` | `+proj=wag2` | [Wagner II](https://proj.org/en/stable/operations/projections/wag2.html) |
| `map_wag3` | `+proj=wag3` | [Wagner III](https://proj.org/en/stable/operations/projections/wag3.html) |
| `map_wag4` | `+proj=wag4` | [Wagner IV](https://proj.org/en/stable/operations/projections/wag4.html) |
| `map_wag5` | `+proj=wag5` | [Wagner V](https://proj.org/en/stable/operations/projections/wag5.html) |
| `map_wag6` | `+proj=wag6` | [Wagner VI](https://proj.org/en/stable/operations/projections/wag6.html) |
| `map_wag7` | `+proj=wag7` | [Wagner VII](https://proj.org/en/stable/operations/projections/wag7.html) |
| `map_webmercator` | `EPSG:3857` | [Web Mercator](https://proj.org/en/stable/operations/projections/merc.html) |
| `map_weren` | `+proj=weren` | [Werenskiold I](https://proj.org/en/stable/operations/projections/weren.html) |
| `map_wink1` | `+proj=wink1` | [Winkel I](https://proj.org/en/stable/operations/projections/wink1.html) |
| `map_wink2` | `+proj=wink2` | [Winkel II](https://proj.org/en/stable/operations/projections/wink2.html) |
| `map_wintri` | `+proj=wintri` | [Winkel Tripel](https://proj.org/en/stable/operations/projections/wintri.html) |

⚠ = listed in `.testignore`; output may be rendered but is not considered reliable.

---

