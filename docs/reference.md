Table of Contents
=================

- [Table of Contents](#table-of-contents)
- [Product definition](#product-definition)
  - [Product configuration](#product-configuration)
    - [Shared attributes](#shared-attributes)
      - [Time intervals](#time-intervals)
      - [Clipping and margins](#clipping-and-margins)
      - [Output formatting](#output-formatting)
    - [Product level attributes](#product-level-attributes)
    - [Views](#views)
    - [Layers](#layers)
      - [BackgroundLayer](#backgroundlayer)
      - [MapLayer](#maplayer)
      - [LocationLayer](#locationlayer)
      - [IsobandLayer](#isobandlayer)
      - [IsolineLayer](#isolinelayer)
        - [isolines-attribute](#isolines-attribute)
      - [IsolabelLayer](#isolabellayer)
      - [SymbolLayer](#symbollayer)
        - [mindistance and priority](#mindistance-and-priority)
      - [ArrowLayer](#arrowlayer)
      - [NumberLayer](#numberlayer)
      - [NullLayer](#nulllayer)
      - [CloudCeilingLayer](#cloudceilinglayer)
      - [StreamLayer](#streamlayer)
      - [RasterLayer](#rasterlayer)
      - [LegendLayer](#legendlayer)
      - [TimeLayer](#timelayer)
      - [TagLayer](#taglayer)
      - [WKTLayer](#wktlayer)
      - [HovmoellerLayer](#hovmoellerlayer)
      - [GraticuleLayer](#graticulelayer)
        - [GraticuleLayer settings](#graticulelayer-settings)
        - [Graticule settings](#graticule-settings)
        - [GraticuleLabels](#graticulelabels)
      - [CircleLayer](#circlelayer)
        - [CircleLayer settings](#circlelayer-settings)
        - [Circle settings](#circle-settings)
        - [CircleLabels settings](#circlelabels-settings)
      - [TranslationLayer](#translationlayer)
      - [WindRoseLayer](#windroselayer)
      - [OSMLayer](#osmlayer)
      - [PostGISLayer](#postgislayer)
      - [IceMapLayer](#icemaplayer)
      - [FinnishRoadObservationLayer](#finnishroadobservationlayer)
        - [Algorithm to deduce weather condition symbol](#algorithm-to-deduce-weather-condition-symbol)
      - [PresentWeatherObservationLayer](#presentweatherobservationlayer)
        - [Algorithm to deduce present weather symbol](#algorithm-to-deduce-present-weather-symbol)
      - [MetarLayer](#metarlayer)
    - [Structure definitions](#structure-definitions)
      - [Attribute structure](#attribute-structure)
      - [AttributeSelection structure](#attributeselection-structure)
      - [Projection structure](#projection-structure)
      - [Defs structure](#defs-structure)
      - [Smoother structure](#smoother-structure)
    - [Sampling structure](#sampling-structure)
      - [Heatmap structure](#heatmap-structure)
      - [Isoband structure](#isoband-structure)
      - [Intersection structure](#intersection-structure)
      - [Isoline structure](#isoline-structure)
      - [Isofilter structure](#isofilter-structure)
      - [LegendLabels structure](#legendlabels-structure)
      - [LegendSymbols structure](#legendsymbols-structure)
      - [Text structure](#text-structure)
      - [Map structure](#map-structure)
      - [MapStyles structure](#mapstyles-structure)
      - [MapFeature structure](#mapfeature-structure)
      - [Positions structure](#positions-structure)
      - [Label structure](#label-structure)
      - [Location structure](#location-structure)
      - [Observation structure](#observation-structure)
      - [Station structure](#station-structure)
      - [WindRose structure](#windrose-structure)
      - [Connector structure](#connector-structure)
      - [Filters structure](#filters-structure)
- [Using dynamically created grids](#using-dynamically-created-grids)
    - [Introduction](#introduction)
    - [Grids with the same timestamp](#grids-with-the-same-timestamp)
    - [Grids calculated over multiple timesteps](#grids-calculated-over-multiple-timesteps)
    - [Functions](#functions)
- [JSON references and includes](#json-references-and-includes)
- [Symbol substitutions via URI](#symbol-substitutions-via-uri)
- [Dali querystring parameters](#dali-querystring-parameters)
  - [Product modification via querystring](#product-modification-via-querystring)
  - [Template selection](#template-selection)
  - [Debugging parameters](#debugging-parameters)
- [WMS querystring parameters](#wms-querystring-parameters)
- [WMS GetMap and GetCapabilities configuration](#wms-getmap-and-getcapabilities-configuration)
  - [WMS layer variants](#wms-layer-variants)
- [Configuration](#configuration)
  - [Main configuration file](#main-configuration-file)
  - [Plugin configuration file](#plugin-configuration-file)
    - [Sample configuration file](#sample-configuration-file)


# Product definition

## Product configuration

There can be thousands of different products defined for the Dali plugin. Each product has its own configuration file, which is used in order to define values for the attributes needed when constructing the final image. 
The table below shows an example of a product configuration file and the image that is returned when this product is requested. 

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image </th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
  "title": "MyRedMap",
  "projection":
   {
     "xsize": 300,
      "ysize": 550,
      "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326",
      "cy": 64.8,
      "cx": 25.4,
      "resolution": 2.5
   },
   "views": [
       {  // *** View 1
       "layers": [
        { // **** Layer 1 : Light blue background
           "layer_type": "background",
           "attributes":
           {
               "stroke": "none",
               "fill": "rgb(190,230,255)"
           }
        },
        { // ***  Layer 2 : Red map with black borders
           "layer_type": "map",
           "map":
          {
            "schema": "natural_earth",
            "table": "europe_country_wgs84"
           },
           "attributes":
           {
            "class": "Europe",
            "stroke": "black",
            "stroke-width": "0.5",
            "fill": "rgb(255,0,0)"
           }
        }
       ]
     }
    ]
}</sub></code></pre></td><td>f</td></tr>
</table>


The product configuration files are written in Java Script Object Notation (JSON). In order to create your own product configuration files you just need to know what attributes are used on different configuration levels, i.e.,  what attributes can be given on the product level, on the view level and on the layer level. These attributes are described flater in this chapter. 

	
The attribute description for each attribute  consists of the attribute name, the attribute type, the default value and a short description of the attribute usage. In the attribute description the attribute type can be defined in the following ways:


| Type                   | Description                                                                                                                                                                                                                                                                                                            |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| (string)               | A string with no default value. Not giving a value to the string may be considered as an error if the  title for the SVG is not defined, or if no default symbol name is given for a legend.                                                                                                                           |
| string                 | A string. Usually this variable needs to be defined, for example, qid to give an ID to isobands. The string may also have a default value, for example, the default separator for the  LegendLabels is an "en dash" (Unicode 2013).                                                                                    |
| _Structure_            | When the type is a structure name then the attribute value is defined as a structure, or the value is a reference to a such structure. For example "projection" -attribute is defined to be the type of  Projection structure, which means that it contains a list of attributes defined for the Projection structure. |
| [_Structure_]          | An array of structures. The type of the structure is defined inside the brackets. For example an attribute which type is [Layer] contains an array of Layer structures.                                                                                                                                                |
| {string:string}        | A map from names to values.                                                                                                                                                                                                                                                                                            |
| {string:string:string} | A map from principal names to a map of name-value pairs.                                                                                                                                                                                                                                                               |
   


### Shared attributes 


There are some upper level attributes that can be used also on the lower level in the hierarchy. For example, some attributes defined on the product level are visible also on the view and layer levels. These levels can directly use these upper level attribute values or override the values with their own values. These attributes can be overridden also by the request parameters. For example, the "language" attribute can be overridden by using the "language" parameter in the request.

The table below contains a list of attributes that are shared from the top level to the lower levels. 

<pre><b>Shared attributes </b></pre>
| Name           | Type         | Default value   | Description                                                                                                                                                                                                    |
| -------------- | ------------ | --------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| language       | (string)     | config:language | Language code. The code does not need to follow ISO 639-1 or other similar standards, it is merely a code name.                                                                                                |
| customer       | (string)     | config:customer | The customer name. The customer setting defines the subdirectory in the Dali configuration directory from which the JSON, CSS and SVG includes are taken, unless an absolute path is given for a JSON include. |
| producer       | (string)     | config:model    | The producer name to be used when querying data from QEngine.                                                                                                                                                  |
| tz             | (string)     | UTC             | The timezone used for parsing time (but not origintime)                                                                                                                                                        |
| time           | (string)     |                 | The time in any string format recognized by the MacGyver time parser (ISO-format, SQL, timestamps, epoch times, offsets from current time).                                                                    |
| time_offset    | (int)        |                 | The time offset in minutes to be added to the time. The principal time is usually set at the top most level (Product), and a time_offset is then used to modify it for different views of data.                |
| interval_start | (int)        |                 | Time in minutes backward from time+time_offset. Used for selecting an interval of observations.                                                                                                                |
| interval_end   | (int)        |                 | Time in minutes forward from time+time_offset. Used for selecting an interval of observations.                                                                                                                 |
| origintime     | (string)     |                 | The querydata origintime in any string format recognized by the MacGyver time parser (ISO-format, SQL, timestamps, epoch times, offsets from current time).                                                    |
| projection     | _Projection_ |                 | The Projection structure.                                                                                                                                                                                      |
| xmargin        | int          | 0               | X-margin in pixels for clipping out features.                                                                                                                                                                  |
| ymargin        | int          | 0               | Y-margin in pixels for clipping out features.                                                                                                                                                                  |
| clip           | bool         | false           | Whether each layer should be placed into a rectangular clipPath.                                                                                                                                               |

#### Time intervals

Normally the "valid time" for the data is time + time_offset. If a time interval is defined, it is producer dependent how the settings are interpreted. Most layers ignore the settings, and the settings are ignored for all forecasts. For lightning data the interval is true, all flashes in the interval are picked. For other observations only the latest one in the time interval will be selected. For this reason interval_end is usually not needed for observations either.

#### Clipping and margins

Normally any point strictly outside the layer rectangle will be omitted. However, if the image will be tiled, this may produce images where symbols or text appear only in one tile, and the rest of the symbol is cut off from the adjacent tile. The layer effective bounding box may be expanded using the setting "margin", which sets both the x-margin and y-margin, or directly by setting "xmargin" and/or "ymargin".

For observations any point inside the expanded area will be included. Typically one would expand the layer by half the size of any symbol needed in the image, be it a symbol, a number or whatever. The marging has an effect also on geometries. For example if one wishes to draw isolines with a line a few pixels wide, the margings should be expanded accordingly.

For tiled images expanding the area has no discernible effect, since the clipping will be automatic since the rendered image is still the expected size and does not include the margins. However, if one has multiple views in a single image, one should set "clip" to true to make sure none of the layers leak outside their respective areas. Using a clipPath slows rendering down a little, hence it is not on by default.

#### Output formatting

PNG output formatting can be tuned using the following settings inside a top level "png" tag:

| Name        | Type     | Default value | Description                                                                                                          |
| ----------- | -------- | ------------- | -------------------------------------------------------------------------------------------------------------------- |
| quality     | (double) | 10            | The PNG compression level. 10=good, 20=poor                                                                          |
| errorfactor | (double) | 2.0           | Tuning parameter for color reduction. Must be greater than 1.0                                                       |
| maxcolors   | (int)    | 0             | Desired maximum number of colors in the palette. Zero implies no maximum, and palette fitting will be fully adaptive |
| truecolor   | (bool)   | false         | Set to avoid color reduction completely                                                                              |

### Product level attributes

The product level is the highest structural level used in the product configuration file. The product attributes are used in order to define product level properties for the current product. On the other hand, the product attributes define the substructures related to the current product. 

The table below contains a list of attributes that can be defined for the product. 

<pre><b>Product </b></pre>
| Name       | Type         | Default value    | Description                                                                                                                                                                              |
| ---------- | ------------ | ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| svg_tmpl   | (string)     | config:templates | The <a href="http://ctpp.havoc.ru/en/">CTPP2</a> template to be used for generating the SVG image. The default template is selected automatically based on the selected image format.    |
| qid        | (string)     | -                | An identifier for the product. Usually this can be left empty.                                                                                                                           |
| width      | (int)        | projection.xsize | The width of the SVG image. If the product contains only one view with no transformations, the default value will be taken from the Product projection xsize variable.                   |
| height     | (int)        | projection.ysize | The height of the SVG image. If the product contains only one view with no transformations, the default value will be taken from the Product projection ysize variable.                  |
| title      | (string)     | -                | The title of the generated SVG image.                                                                                                                                                    |
| attributes | _Attributes_ | -                | The SVG image attributes for the product level &lt;g&gt;...&lt;/g&gt; group.                                                                                                             |
| defs       | _Defs_       | -                | The SVG image header definitions such as styles, symbols, paths not to be drawn directly etc. This information is defined as the Defs structure, which is described in the next section. |
| views      | _[View]_     | -                | An array of the View structures used in the SVG image. Usually a product has only one View structure which shares the projection defined on the product level.                           |


### Views

A product can contain several views that are merged together into single image / product. The product level attribute "views" are used in order to define an array of the _View_ structures for the product. Each view must have its own _View_ structure in this array. The _View_ structure is used in order to define the view level attributes and the lower level structures (i.e. layers).

The table below contains a list of attributes that can be defined in the _View_ structure. 

<pre><b>View</b></pre>
| Name       | Type         | Default value | Description                                                                    |
| ---------- | ------------ | ------------- | ------------------------------------------------------------------------------ |
| qid        | (string)     | -             | An identifier for the view. Normally something simple such as "view1" or "v1". |
| attributes | _Attributes_ | -             | The SVG attributes for the View level <g>...</g> group.                        |
| layers     | _[Layer]_    | -             | An array of the _Layer_ structures needed in order to construct the view.      |


### Layers

A view can contain several layers that are merged together into single view. The view level attribute "layers" are used in order to define an array of the Layer structures for the view. Each layer must have its own Layer structure in this array. 
The Layer structure is used in order to define layer level properties. The most important attribute on the layer level is the "layer_type" attribute, which defines the type of the current layer. At the same time it defines indirectly which layer module the Dali plugin should use  to create an image for the current layer. 
On the other hand, the layer type tells us which other attributes we should define for the current layer. In other words, if the layer type is for example "isoband" then we should define on this level also all isoband layer related attributes.
All the layers share some common attributes. In addition, each layer type has its own attributes. The table below contains a list of attributes that are common in all layers.  

<pre><b>Layer</b></pre>
<table>
<tr>
<th>Name </th>
<th>Type </th>
<th>Default Value </th>
<th colspan="2"> Description </th></tr>
<tr><td>qid</td><td>(string)</td><td>-</td><td colspan="2">An identified for the layer. Normally something simple such as "layer1" or "t2mlayer" or just "l1".</td></tr>
<tr><td>css</td><td>(string)</td><td>-</td><td colspan="2">An external CSS file to be included into the style section. E.g.,  "isobands/temperature.css".</td></tr>
<tr><td>attributes</td><td><i>Attributes</i></td><td>-</td><td colspan="2">The SVG attributes for the Layer level top SVG tag. Often the tag is &lt;g&gt;, but may be any other tag too for example when layer_type=tag is used.</td></tr>
<tr><td rowspan="17">layer_type</td><td rowspan="17">(string)</td><td rowspan="17">"tag"</td><td colspan="2">The type of the layer.</td></tr>
<tr><td><b>Value</b></td><td><b>Layer</b></td></tr>
<tr><td>background</td><td>BackgroundLayer</td></tr>
<tr><td>map</td><td>MapLayer</td></tr>
<tr><td>location</td><td>LocationLayer</td></tr>
<tr><td>isoband</td><td>IsobandLayer</td></tr>
<tr><td>isoline</td><td>IsolineLayer</td></tr>
<tr><td>isolabel</td><td>IsolabelLayer</td></tr>
<tr><td>symbol</td><td>SymbolLayer</td></tr>
<tr><td>arrow</td><td>ArrowLayer</td></tr>
<tr><td>number</td><td>NumberLayer</td></tr>
<tr><td>null</td><td>NumllLayer</td></tr>
<tr><td>cloudceiling</td><td>CloudCeilingLayer</td></tr>
<tr><td>stream</td><td>StreamLayer</td></tr>
<tr><td>legend</td><td>LegendLayer</td></tr>
<tr><td>time</td><td>TileLayer</td></tr>
<tr><td>tag</td><td>TagLayer</td></tr>
<tr><td>translation</td><td>TranslationLayer</td></tr>
<tr><td>windrose</td><td>WindRoseLayer</td></tr>
<tr><td>metar</td><td>MetarLayer</td></tr>
<tr><td>osm</td><td>OSMLayer</td></tr>
<tr><td>postgis</td><td>PostGISLayer</td></tr>
<tr><td>icemap</td><td>IceMApLayer</td></tr>
<tr><td>minresolution</td><td>(double)</td><td>-</td><td colspan="2">Minimum resolution of the projection for the layer to be generated at all.</td></tr>
<tr><td>maxresolution</td><td>(double)</td><td>-</td><td colspan="2">Maximum resolution of the projection for the layer to be generated at all.</td></tr>
</table>

Notice that the term "resolution" used above might be a little bit misleading in this case, since it refers to the number of kilometers represented by one pixel. Hence the higher the resolution the worse the accuracy of the image actually is. The actual condition to be satisfied is minresolution <= resolution < maxresolution, where one or both of the limits may be missing.

#### BackgroundLayer

The background layer is used to create a background for the current view. So it  is usually the lowest layer in the view. 

The table below shows  a simple example on the usage of  the background layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer </th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
  "title": "MyLightBlueBackground",
  "projection":
   {
     "xsize": 300,
      "ysize": 550,
      "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326",
      "cy": 64.8,
      "cx": 25.4,
      "resolution": 2.5
   },
   "views": [
       {  // *** View 1
       "layers": [
        { // **** Layer 1 : Light blue background
           "layer_type": "background",
           "attributes":
           {
               "stroke": "none",
               "fill": "rgb(190,230,255)"
           }
        }
       ]
     }
    ]
}</sub></code></pre></td><td><img src="images/light_blue_background.png"></td></tr>
</table>

The background layer has no attributes than the generic Layer or Properties attributes. The layer exists merely to obtain the "xsize" and "ysize" attribute values for a rectangle element so that the background can be filled with a value.

#### MapLayer

The table below shows  a simple example on the usage of  the  map layer.


<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
   "title": "MyMap",
   "projection":
   {
      "xsize": 300,
      "ysize": 550,
      "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326",
      "cy": 64.8,
      "cx": 25.4,
      "resolution": 2.5
   },
   "views": [
    {
     "layers": [
    {
       "layer_type": "map",
       "map":
       {
        "schema": "natural_earth",
        "table": "europe_country_wgs84"
       },
       "attributes":
       {
        "class": "Europe"
       }
    } ]
  }]
 }</sub></code></pre></td><td><img src="images/map_layer_ex.png"></td></tr>
</table>


The table below contains a list of attributes that can be defined for the map layer in addition to the common layer attributes.

<pre><b>MapLayer</b></pre>
| Name      | Type        | Default value | Description                                        |
| --------- | ----------- | ------------- | -------------------------------------------------- |
| map       | Map         | -             | The Map structure                                  |
| precision | double      | 1.0           | Precision of SVG coordinates                       |
| styles    | [MapStyles] | -             | Optional regional styling based on forecast values |

#### LocationLayer

The location layer can be used  to point specific locations in the image. For example, we can create an image layer that shows all important cities by using predefined symbols. It is also possible to use different symbols based on some information. For example, we can use a red square symbol for  cities with  population  over a million and a red circle symbol for  cities with  population  less than a million, but more than fifty thousand. 


The table below shows  a simple example on the usage of  the location layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
      "title": "MyCities",
   "projection":
  {
      "xsize": 300, "ysize": 550, "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326", "cy": 64.8, "cx": 25.4,
      "resolution": 2.5
   },
   "views": [
     { // *** View 1
     "attributes": {   "id": "view1" },
     "layers": [
    { // *** Layer 1 : Map
      "layer_type": "map",
      "map":
       {
        "schema": "natural_earth",
        "table": "europe_country_wgs84"
       },
       "attributes":
       {
        "class": "Europe"
       }
    },
    { // *** Layer 2: Cities
      "qid": "cities",
       "layer_type": "location",
        "keyword" : "ely_cities",
           "css": "maps/map.css",
        "symbols":
       {
          "default": [
         {
            "hilimit": 1000000,
            "symbol": "city"
         },
         {
            "lolimit": 1000000,
            "symbol": "bigcity"
         }],
         "PPLC": [
         {
            "symbol": "capital"
         } ]
        }
    } ]
  } ]
}</sub></code></pre></td><td><img src="images/location_layer_ex.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for the location layer in addition to the common layer attributes.

<pre><b>LocationLayer</b></pre>
| Name        | Type                                                  | Default value | Description                                                                            |
| ----------- | ----------------------------------------------------- | ------------- | -------------------------------------------------------------------------------------- |
| keyword     | string                                                | -             | The geonames keyword.                                                                  |
| mindistance | double                                                | 30            | Minimum distance between location symbols.                                             |
| countries   | (string) or string                                    | -             | Allowed country isocodes.                                                              |
| symbol      | (string)                                              | -             | Default symbol.                                                                        |
| symbols     | [AttributeSelection] or {string:[AttributeSelection]} | -             | Symbol selection based on population, or a map from feature codes to symbol selection. |

#### IsobandLayer

The isoband layer can be used  to add filled contours (= isobands) to the view. Isobands can be used for presenting several kinds of data such as  temperatures, rain amounts, cloudiness, ice cover, flash heatmap etc.


The tables below show simple examples  on the usage of   the isoband layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
  "title": "MyCloudiness",
  "projection":
   {
     "xsize": 300,
      "ysize": 550,
      "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326",
      "cy": 64.8,
      "cx": 25.4,
      "resolution": 2.5
   },
   "views": [
       {  // *** View 1
       "layers": [
        { // ***  Layer 1 : Cloudiness
           "layer_type": "isoband",
           "isobands": "json:isobands/cloudiness.json",
           "css": "isobands/cloudiness.css",
           "parameter": "TotalCloudCover",
           "multiplier": 0.0898,
           "offset" : -0.49,
           "attributes":
           {
            "shape-rendering": "crispEdges"
           }
        }
       ]
     }
    ]
}</sub></code></pre></td><td><img src="images/isoband_ex.png">/td></tr>
</table>

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
    "title" : "Lightning heatmap for 15 minute period",
    "abstract" :  "Lightning heatmap for 15 minute period",
    "interval_start": 15,
    "interval_end": 15,
    "projection":
    {
        "crs": "EPSG:4326",
        "cx" : 25,
        "cy" : 65,
        "resolution" : 5,
        "xsize": 500,
        "ysize": 500
    },
    "producer": "flash",
    "views": [{
        "layers": [
            {
                "qid": "l",
                "layer_type": "isoband",
                "parameter": "peak_current",
                "isobands": "json:isobands/heatmap.json",
                "css": "isobands/heatmap.css",
                "attributes": {
                    "shape-rendering": "crispEdges"
                },
                "heatmap" : {
                    "resolution" : 10,
                    "radius" : 10,
                    "kernel" : "exp",
                    "deviation" : 10.0
                }
            },
            {
                "qid": "l1",
                "layer_type": "map",
                "map":
                {
                    "schema": "natural_earth",
                    "table": "admin_0_countries",
                    "minarea": 100
                },
                "attributes":
                {
                    "id": "europe_country_lines",
                    "fill": "none",
                    "stroke": "#222",
                    "stroke-width": "0.3px"
                }
            }
        ]
    }]
}</sub></code></pre></td><td><img src="images/isoband_ex_hm.png"></td></tr>
</table>


The table below contains a list of attributes that can be defined for the isoband layer in addition to the common layer attributes.
<pre><b>IsobandLayer</b></pre>
<table>
<tr>
<th>Name </th>
<th>Type </th>
<th>Default Value </th>
<th colspan="2"> Description </th></tr>
<tr><td>parameter</td><td>(string)</td><td>-</td><td colspan="2">The parameter name for the isobands.</td></tr>
<tr><td>level</td><td>double</td><td>linear</td><td colspan="2">The querydata level value. By default the first level is used.</td></tr>
<tr><td rowspan="6">interpolation</td><td rowspan="6">(string)</td><td rowspan="6">"linear"</td><td colspan="2">The interpolation method.</td></tr>
<tr><td><b>Value</b></td><td><b>Description</b></td></tr>
<tr><td>linear</td><td>Values are interpolated linearly.</td></tr>
<tr><td>midpoint|nearest|discrete</td><td>Nearest point value is used.</td></tr>
<tr><td>logarithmic</td><td>Logarithmic interpolation. Useful mostly for logarithmic parameters such as precipitation rate, which may have sharp peaks near strong showers.</td></tr>
<tr><td>isobands</td><td><i>[Isoband]</i></td><td>-</td><td colspan="2">An array if <i>Isoband</i> structures./td></tr>
<tr><td>autoqid</td><td>(string)</td><td>-</td><td>Pattern for generating a qid for each isoband, for example "temperature_{}_{}"|
<tr><td>autoclass</td><td>(string)</td><td>-</td><td>Pattern for generating a class for each isoband, for example "Temperature_{}_{}"|
<tr><td>precision</td><td>(double)</td><td>1.0</td><td colspan="2">Precision of SVG coordinates./td></tr>
<tr><td>unit_conversion</td><td>(string)</td><td>-</td><td colspan="2">Name of desired unit conversion defined in the configuration file.</td></tr>
<tr><td>multiplier</td><td>(double)</td><td>-</td><td colspan="2">A multiplier for valid data values for unit conversion purposes.</td></tr>
<tr><td>offset</td><td>(double)</td><td>0.0</td><td colspan="2">An offset for valid data values for unit conversion purposes.</td></tr>
<tr><td>smoother</td><td><i>Smoother</i></td><td>-</td><td colspan="2">Smoother settings for the 2D grid data.</tr>
<tr><td>sampling</td><td><i>Sampling<i></td><td>-</td><td colspan="2">Sampling settings for the 2D grid data.</tr>
<tr><td>extrapolation</td><td><i>int</i></td><td>0</td><td colspan="2">How many grid cells to extrapolate data into regions with unknown values</tr>
<tr><td>minarea</td><td><i>double</i></td><td>-</td><td colspan="2">Mimimum area for polygons, including holes</tr>
<tr><td>areaunits</td><td><i>(string)</i></td><td>km^2</td><td colspan="2">Units for minarea setting. km^2 or px^2</tr>
<tr><td>isofilter</td><td><i>Isofilter</i></td><td>-</td><td>Lowpass filter for isolines/isobands. Preferable to smoother settings when data resolution is high.</td></tr>
<tr><td>heatmap</td><td><i>Heatmap</i></td><td>-</td><td colspan="2">Heatmap settings.</tr>
<tr><td>inside</td><td><i>Map</i></td><td>-</td><td colspan="2">Intersect isoband with the map</tr>
<tr><td>outsider</td><td><i>Map</i></td><td>-</td><td colspan="2">Substract map from isoband.</tr>
<tr><td>intersect</td><td><i>[Intersection]</i> or <i>Intersection</i></td><td>-</td><td colspan="2">Alternate isoband(s) with which to intersect.</td></tr>
<tr><td>closed_range</td><td>bool<td>true</td><td>True if the last isoband is a closed interval. For example for percentages one would want range 90 <= x <= 100 instead of 90 <= x < 100.</td></tr>
<tr><td>validate</td><td>bool</td><td>false</td><td>True if the geometries are to be validated (slow, for debugging purposes)</td></tr>
<tr><td>strict</td><td>bool</td><td>false</td><td>In strict mode invalid geometries cause an error. Extra information will be dumped to the journal and attached to the thrown exception</td></tr>
<tr><td>desliver</td><td>bool</td><td>false</td><td>True if sliver polygons are to be removed</td></tr>
</table>

In automatic qid and class generation "." will be replaced by ",", since "." is reserved for JSON paths. If both the lower limit and upper limit are undefined, "nan" will be used to fill the pattern. If only the lower limit is missing, "-inf" will be used. If only the upper limit is missing, "inf" will be used.

#### IsolineLayer

The isoline layer can be used  to add isolines to the view. Isolines can be used  to present several  kinds of data like pressure variations, altitude variations, etc.

The table below shows a simple example  on the usage of   the isoline layer.


<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
  "title": "MyPressure",
  "projection":
   {
     "xsize": 300,
      "ysize": 550,
      "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326",
      "cy": 64.8,
      "cx": 25.4,
      "resolution": 2.5
   },
   "views": [
       {  // *** View 1
       "layers": [
      { // *** Layer 1 : Map
         "layer_type": "map",
         "map":
         {
          "schema": "natural_earth",
          "table": "europe_country_wgs84"
         },
         "attributes":
         {
          "class": "Europe"
         }
      },
      { // *** Layer 2 : Isoline
           "layer_type": "isoline",
           "isolines": "json:isolines/pressure.json",
           "css": "isolines/pressure.css",
           "parameter": "Pressure",
           "attributes" :
           {
            "class" : "Pressure"
           }
        }
       ]
     }
    ]
}
}</sub></code></pre></td><td><img src="images/isoline_ex.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for the isoline layer in addition to the common layer attributes.


<pre><b>IsolineLayer</b></pre>
| Name            | Type                         | Default value | Description                                                                                                                                    |
| --------------- | ---------------------------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| parameter       | (string)                     | -             | The parameter name for the isolines.                                                                                                           |
| level           | (double)                     | -             | The querydata level value. By default the first level is used.                                                                                 |
| interpolation   | linear                       | (string)      | linear or logarithmic                                                                                                                          |
| isolines        | [Isoline]                    | -             | An array of Isoline structures or set of parameters which define how array of Isoline structures are generated (see. isolines-attribute below) |
| autoqid         | (string)                     | -             | Pattern for generating a qid for each isoline, for example "temperature_{}"                                                                    |
| autoclass       | (string)                     | -             | Pattern for generating a class for each isoline, for example "Temperature_{}"                                                                  |
| precision       | (double)                     | 1.0           | Precision of SVG coordinates.                                                                                                                  |
| unit_conversion | (string)                     | -             | Name of desired unit conversion. Unit conversions are listed in the configuration file.                                                        |
| multiplier      | (double)                     | 1.0           | A multiplier for valid data values for unit conversion purposes.                                                                               |
| offset          | (double)                     | 0.0           | An offset for valid data values for unit conversion purposes.                                                                                  |
| smoother        | _Smoother_                   | -             | Smoother settings for the 2D grid data.                                                                                                        |
| sampling        | _Sampling_                   | -             | Sampling settings for the 2D grid data.                                                                                                        |
| extrapolation   | int                          | 0             | How many grid cells to extrapolate data into regions with unknown values.                                                                      |
| minarea         | double                       | -             | Minimum area for closed linestrings in km^2.                                                                                                   |
|areaunits        | (string)                     | km^2          | Units for minarea setting. km^2 or px^2|
| inside          | _Map_                        | -             | Intersect isoband with a map                                                                                                                   |
| outside         | _Map_                        | -             | Substract map from isoband                                                                                                                     |
| intersect       | _[Intersect]_ or _Intersect_ | -             | Alternate isoband(s) with which to intersect.                                                                                                  |
| validate        | bool                         | false         | True if the geometries are to be validated (slow, for debugging purposes)                                                                      |
| strict          | bool                         | false         | In strict mode invalid geometries cause an error. Extra information will be dumped to the journal and attached to the thrown exception         |
| desliver        | bool                         | false         | True if sliver polygons are to be removed                                                                                                      |

In automatic qid/class generation "." will be replaced by ",", since "." is reserved for JSON path definitions.

##### isolines-attribute

Isolines are defined by using isolines-attribute. There are two alternative ways to define the attribute

1) Define explicitly array of Isoline structures

Example:

<pre><code>
"isolines":
{
[
    {
        "qid": "pressure_950",
        "value": 950,
	"attributes": {}
    },
    {
        "qid": "pressure_960",
        "value": 960,
	"attributes": {}
    },
    {
        "qid": "pressure_970",
        "value": 970,
	"attributes": {}
    },
    {
        "qid": "pressure_980",
        "value": 980,
	"attributes": {}
    },
    {
        "qid": "pressure_990",
        "value": 990,
	"attributes": {}
    },
    {
        "qid": "pressure_1000",
        "value": 1000,
	"attributes": {}
    },
    {
        "qid": "pressure_1010",
        "value": 1010,
	"attributes": {}
    },
    {
        "qid": "pressure_1020",
        "value": 1020,
	"attributes": {}
    },
    {
        "qid": "pressure_1030",
        "value": 1030,
	"attributes": {}
    },
    {
        "qid": "pressure_1040",
        "value": 1040,
	"attributes": {}
    },
    {
        "qid": "pressure_1050",
        "value": 1050,
	"attributes": {}
    },
]
}
</code></pre>

2) Define set of parameters which are used to generate Isoline structures. The following parameters must be defined:

| Name       | Type                                | Description                                                                       | Note                                  | Example                       |
| ---------- | ----------------------------------- | --------------------------------------------------------------------------------- | ------------------------------------- | ----------------------------- |
| qidprefix  | string                              | Prefix of isoline layer id.                                                       | optional, default value is 'isoline_' | qidprefix="pressure_isoline_" |
| startvalue | int/double                          | Value of first Isoline.                                                           |                                       | startvalue=900                |
| endvalue   | int/double                          | Value of last Isoline                                                             |                                       | endvalue=1100                 |
| interval   | int/double                          | The interval between two successive Isolines.                                     |                                       | interval=10                   |
| except     | int/double or array of ints/doubles | The interval is not shown if it is divisible by except value(s) (modulo-operator) | optional                              | except=5 or except=[3,5]      |

Example:

<pre><code>
                {
		    "qid": "l4",
                    "layer_type": "isoline",
                     "isolines": 
		    {  
			"qidprefix": "isoline_main_", 
			"startvalue": 900, 
			"endvalue": 1100,
			"interval": 5
		    },
                    "css": "isolines/pressure.css",
                    "parameter": "Pressure",
		    "attributes":
		    {
			"fill": "none",
			"stroke": "#333",
			"stroke-width": "1.5px"
		    }
                },
                {
		    "qid": "l5",
                    "layer_type": "isoline",
                    "isolines": 
		    {  
			"qidprefix": "isoline_sub_", 
			"startvalue": 900, 
			"endvalue": 1100,
			"interval": 2,
			"except": 5
		    },
                    "css": "isolines/pressure.css",
                    "parameter": "Pressure",
		    "attributes":
		    {
			"fill": "none",
			"stroke": "red",
			"stroke-dasharray": "10,5",
			"stroke-width": "1.0px"
		    }
		}
</code></pre>

#### IsolabelLayer

The isolabel layer is derived from the isoline layer, and thus inherits all its settings. The settings below are then available for positioning isovalues on the isolines.
It is also possible to refer to the isolines that would be generated by an isoband layer, or to list to generate isovalues explicitly without referring to another layer.

<pre><b>IsolabelLayer</b></pre>
| Name               | Type      | Default value  | Description                                                                                              |
| ------------------ | --------- | -------------- | -------------------------------------------------------------------------------------------------------- |
| label              | Label     | -              | Label settings including the orientation                                                                 |
| angles             | [double]  | [0,-45,45,180] | Orientations in which local maxima are searched for suitable label positions.                            |
| upright            | bool      | false          | Optionally turn labels upright if their angle is outside -90...90                                        |
| max_angle          | double    | 60             | Allow label angles only in range -60...60                                                                |
| min_isoline_length | double    | 150            | Minimum length of the isoline in pixels for placing any labels                                           |
| min_distance_other | double    | 20             | Minimum distance to labels on isolines with another value                                                |
| min_distance_same  | double    | 50             | Minimum distance to labels on isolines with the same value                                               |
| min_distance_same  | double    | 200            | Minimum distance to labels on the same isoline segment                                                   |
| max_curvature      | double    | 90             | Maximum local curvature change at the label position                                                     |
| stencil_size       | int       | 5              | Search N adjacent points when looking for extrema and calculating the local curvature.                   |
| isovalues          | [double]  | -              | Array of isovalue to be used.                                                                            |
| isobands           | [Isoband] | -              | Array of isoband structures. All potential values will be extracted and inserted to the isovalues array. |
| isolines           | [Isoline] | -              | Array of isoline structures. All values will be extracted and inserted to the isovalues array.           |
| textattributes     | [Attributes] | -           | Attributes to be assigned separately for each label                                                      |

The isovalues parameter may also be a JSON object with start, stop and step (default=1) doubles.

#### SymbolLayer

The symbol layer can be used to add different kinds of symbols to the view. The symbols are added at  the given locations and the type of the symbol depends on the value of the field defined by the "parameter" attribute. The actual symbol figures are stored in the file systems s SVG images.

The table below shows a simple example  on the usage of   the symbol layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
    "title": "MyWeatherSymbols",
    "refs":
  {
      "myprojection": "json:maps/finlandprojection.json",
      "finland":  {
       "schema": "natural_earth",
       "table": "finland_country_wgs84",
       "minarea": 100,
       "mindistance": 1
      }
    },
    "projection": "ref:refs.myprojection",
    "views": [
   {
    "attributes":  { "id": "view1" },
       "layers": [
    {
         "layer_type": "map",
         "map": {
          "schema": "natural_earth",
           "table": "finland_country_wgs84",
          "minarea": 80,
           "mindistance": 5
         },
         "attributes": {
           "id": "europe_country_lines",
           "fill": "none",
           "stroke": "#666",
           "stroke-width": "0.5pt"
         }
       },
      {
        "layer_type": "symbol",
        "css": "symbols/weather.css",
        "symbols": "json:symbols/weather.json",
        "parameter": "weathersymbol",
        "scale": 1.25,
        "positions": {
         "layout": "latlon",
         "locations": [
             { "longitude": 24.93417, "latitude": 60.17556 },
             { "longitude": 28.27838, "latitude": 61.02292 },
             { "longitude": 21.85943, "latitude": 61.57477 },
             { "longitude": 25.60742, "latitude": 62.20806 },
             { "longitude": 27.89568, "latitude": 62.94718 },
             { "longitude": 23.13066, "latitude": 63.83847 },
             { "longitude": 27.40756, "latitude": 64.13355 },
             { "longitude": 24.56371, "latitude": 65.73641 },
             { "longitude": 28.15806, "latitude": 67.29250 },
             { "longitude": 24.15138, "latitude": 67.60517 },
             { "longitude": 27.02881, "latitude": 68.90596 }
         ],

         "dx": -20,
         "dy": -20
        }
      }]
  }]

}</sub></code></pre></td><td><img src="images/symbols_ex.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for the symbol layer in addition to the common layer attributes.

<pre><b>SymbolLayer </b></pre>
| Name            | Type                    | Default value | Description                                                                                   |
| --------------- | ----------------------- | ------------- | --------------------------------------------------------------------------------------------- |
| parameter       | (string)                | -             | The parameter for the symbols.                                                                |
| unit_conversion | (string)                | -             | Name of desired unit conversion. Unit conversions are listed in the configuration file.       |
| multiplier      | (double)                | 1.0           | A multiplier for valid data values for unit conversion purposes.                              |
| offset          | (double)                | 0.0           | An offset for valid data for unit conversion purposes.                                        |
| level           | (double)                | -             | The querydata level value. By default the first level is used.                                |
| positions       | _Positions_             | -             | The positions for the symbols.                                                                |
| minvalues       | int                     | 0             | Minimum required number of valid values (or return error response)                            |
| maxdistance     | double                  | 5             | Maximum distance for a station to be accepted close enough to the position.                   |
| dx              | (int)                   | -             | Common positional adjustment for all symbols.                                                 |
| dy              | (int)                   | -             | Common positional adjustment for all symbols.                                                 |
| scale           | (double)                | -             | Scale factor for the symbols.                                                                 |
| symbol          | (string)                | -             | The default symbol. This is mostly useful for marking all the positions with a single symbol. |
| symbols         | _[AttributeSelection]_  | -             | The symbols for different data values.                                                        |
| mindistance     | int                     | -             | Minimum distance in pixels between symbols.                                                   |
| priority        | string or integer array | -             | Priority order of symbols.                                                                    |
| rendering_order | string                  | "normal"      | Rendering order of the symbol is normal or reverse with respect to priority                   |

Note that assigning a proper scale for symbols with CSS or SVG attributes alone  is difficult. Using the scale-attribute eases the  scaling of the symbols.

##### mindistance and priority

<pre><b>Allowed priority values</b></pre>
| Value      | Description                                                                                       |
| ---------- | ------------------------------------------------------------------------------------------------- |
| min        | Symbols with smallest value are drawn first.                                                      |
| max        | Symbols with biggest value are drawn first.                                                       |
| extrema    | Symbols with biggest mean deviation drawn first.                                                  |
| none       | Symbols are treated equally, there is no priority order.                                          |
| [83,82,81] | Symbols with values 83,82,81 are drawn first in the order given, the rest have no priority order. |

Symbols are drawn on the map starting from the highest priority. If there already is a symbol nearby on the map (mindistance parameter), the symbol is not shown. Regarding mindistance and priority parameters the same logic is applied in Arrow-, Number- and CloudCeilingLayers.

#### ArrowLayer

The arrow layer is usually used for showing wind directions on the map. Technically it can be used for showing all kinds of directions. 

The table below shows a simple example  on the usage of   the arrow layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
  "title": "MyWindDirections",
  "projection":
   {
     "xsize": 300,
      "ysize": 550,
      "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326",
      "cy": 64.8,
      "cx": 25.4,
      "resolution": 2.5
   },
   "views": [
  {  // *** View 1
      "layers": [
    { // *** Layer 1 : Map
      "layer_type": "map",
      "map":
      {
        "schema": "natural_earth",
        "table": "europe_country_wgs84"
       },
       "attributes":
       {
        "class": "Europe"
       }
    },

    { // *** Layer 2 :Wind arrows
      "layer_type": "arrow",
      "css": "arrows/windarrow.css",
      "direction": "WindDirection",
      "speed": "WindSpeedMS",
      "attributes":
      {
        "id": "wind_arrows",
        "class": "WindArrow",
        "mask": "url(#windlegendmask)"
       },
       "arrows": "json:arrows/windarrow.json",
       "positions":
       {
        "x": 5,
        "y": 5,
        "dx": 30,
        "dy": 30,
        "ddx": 15
       }
    } ]
   } ]
}</sub></code></pre></td><td><img src="images/arrow_ex.png"></td></tr>
</table>


The table below contains a list of attributes that can be defined for the arrow layer in addition to the common layer attributes. 

<pre><b>ArrowLayer </b></pre>
| Name             | Type                    | Default value | Description                                                                                                                                |
| ---------------- | ----------------------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| direction        | (string)                | -             | The parameter name for the arrow direction.                                                                                                |
| speed            | (string)                | -             | The parameter name for speed, if one is needed.                                                                                            |
| u                | (string)                | -             | The parameter name for the U-component of the speed vector.                                                                                |
| v                | (string)                | -             | The parameter name for the V-component of the speed vector.                                                                                |
| fixeddirection   | (double)                | -             | Fixed direction value for testing purposes.                                                                                                |
| fixedspeed       | (double)                | -             | Fixed speed value for testing purposes.                                                                                                    |
| level            | (double)                | -             | The querydata level value. By default the first level is used.                                                                             |
| symbol           | (string)                | -             | The default symbol for the arrows. May be overridden in the arrows-definitions. The special value "windbarb" has not yet been implemented. |
| scale            | (double)                | -             | Scale factor for the symbols                                                                                                               |
| unit_conversion  | (string)                | -             | Name of desired unit conversion. Unit conversions are listed in the configuration file.                                                    |
| multiplier       | (double)                | -             | Multiplier for the speed parameter                                                                                                         |
| offset           | (double)                | -             | Offset for the speed parameter                                                                                                             |
| minrotationspeed | (double)                | -             | Minimum required speed for the arrow to be rotated at all                                                                                  |
| dx               | (int)                   | -             | Common positional adjustment for all arrows.                                                                                               |
| dy               | (int)                   | -             | Common positional adjustment for all arrows.                                                                                               |
| southflop        | boolean                 | false         | Set to true if the arrow symbol should be flipped horizontally in the southern hemisphere (used for wind barbs).                           |
| northflop        | boolean                 | false         | Set to true if the arrow symbol should be flipped horizontally in the northern hemisphere.                                                 |
| flip             | boolean                 | false         | Set to true if the arrow symbol should always be flipped vertically.                                                                       |
| positions        | _Positions_             | -             | Arrow position generation definitions.                                                                                                     |
| minvalues        | int                     | 0             | Minimum required number of valid values (or return error response)                                                                         |
| maxdistance      | double                  | 5             | Maximum distance for a station to be accepted close enough to the position.                                                                |
| arrows           | _[AttributeSelection]_  | -             | SVG attribute selection for speed dependent arrows.                                                                                        |
| mindistance      | int                     | -             | Minimum distance in pixels between arrows.                                                                                                 |
| rendering_order  | string                  | "normal"      | Rendering order of the symbol is normal or reverse with respect to priority                                                                |
| priority         | string or integer array | -             | Priority order of arrows.                                                                                                                  |

Note: A direction parameter is sufficient to draw arrows. An additional speed component may be defined to style the arrows depending on the speed. Both U- and V-components must be specified if used, but these cannot be used simultaneously with the direction and speed parameters.


#### NumberLayer

The number layers is used  to add numerical values for  temperatures, pressures, rain amounts, etc. in  the view. 


The table below shows a simple example  on the usage of   the number layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
   "title": "Temperature Overlay",
   "refs":
  {
    "myprojection": "json:maps/finlandprojection.json"
   },
   "projection": "ref:refs.myprojection",
   "views": [
     {  // *** View 1
     "layers": [
    { // *** Layer 1 : Map
      "layer_type": "map",
      "map":
      {
        "schema": "natural_earth",
        "table": "europe_country_wgs84"
       },
       "attributes":
       {
        "class": "Europe"
       }
    },
      { // *** Layer 2 : Numbers
       "layer_type": "number",
       "css": "numbers/numbers.css",
       "parameter": "Temperature",
       "attributes":
      {
         "class": "Number",
        "mask" : "url(#legendmask)"
        },
        "label": "json:numbers/integers.json",
        "positions":
      {
        "x": 20,
        "y": 18,
        "dx": 30,
        "dy": 30,
        "ddx": 15
       }
        } ]
  } ]
}</sub></code></pre></td><td><img src="images/numbers_ex.png"></td></tr>
</table>


The table below contains a list of attributes that can be defined for the number layer in addition to the common layer attributes.

<pre><b>NumberLayer </b></pre>
| Name            | Type                    | Default value | Description                                                                             |
| --------------- | ----------------------- | ------------- | --------------------------------------------------------------------------------------- |
| parameter       | (string)                | -             | The parameter name for the numbers.                                                     |
| level           | (double)                | -             | The querydata level value. By default the first level is used.                          |
| positions       | _Positions_             | -             | The positions for the numbers.                                                          |
| minvalues       | int                     | 0             | Minimum required number of valid values (or return error response)                      |
| maxdistance     | double                  | 5             | Maximum distance for a station to be accepted close enough to the position.             |
| label           | _Label_                 | -             | Label definitions.                                                                      |
| numbers         | _[AttributeSelection]_  | -             | SVG attribute selection based on the value of the label.                                |
| unit_conversion | (string)                | -             | Name of desired unit conversion. Unit conversions are listed in the configuration file. |
| multiplier      | (double)                | 1.0           | A multiplier for valid data values for unit conversion purposes.                        |
| offset          | (double)                | 0.0           | An offset for valid data for unit conversion purposes.                                  |
| mindistance     | int                     | -             | Minimum distance in pixels between numbers.                                             |
| rendering_order | string                  | "normal"      | Rendering order of the symbol is normal or reverse with respect to priority             |
| priority        | string or integer array | -             | Priority order of numbers.                                                              |

#### NullLayer

Null layers are merely placeholders for definitions which will be inserted into the block if a suitable STYLES option is used and the qid settings match. Sample use cases for null layers include

 * enabling numbers over isobands
 * enabling symbols over the underlying image
 * enabling arrows over isobands
 * enabling isolabels over isobands

#### CloudCeilingLayer

The cloud_ceiling layer is used for cloud ceiling observations (ie. the height of the base of the lowest clouds when the cloud amount is more than falf of the sky: broken, overcast or obscured)

The table below shows a simple example on the usage of the cloud_ceiling layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{
    "title": "Cloud Ceiling",
    "abstract": "Cloud ceiling from FMI's AWS-stations",
    "producer": "observations_fmi",
    "interval_start": 15,
    "interval_end": 0,
    "language": "fi",
    "projection": {},
    "views": [{
	"qid": "v1",
	"attributes": {
	    "id": "view1"
	},
	"layers": [
	    {
		"qid": "finland",
		"layer_type": "map",
		"map":
		{
		    "schema": "natural_earth",
		    "table": "admin_0_countries",
		    "where": "iso_a2 IN ('FI','AX')"
		},
		"attributes": {
		    "id": "finland_country",
		    "fill": "rgb(255, 255, 204)"
		}
	    },
            {
                "qid": "finland-roads",
                "layer_type": "map",
                "map": {
                    "schema": "esri",
                    "table": "europe_roads_eureffin",
                    "where": "cntryname='Finland'",
                    "lines": true
                },
                "attributes": {
                    "class": "Road"
                }
            },
            {
		"qid": "borders",
		"layer_type": "map",
		"css": "maps/map.css",
		"map": {
                    "lines": true,
                    "schema": "esri",
                    "table": "europe_country_wgs84",
                    "mindistance": 2.5,
                    "minarea": 10
		},
		"attributes": {
                    "class": "Border",
                    "id": "BorderMap"
		}
            },
	    {
                "qid": "cities",
                "layer_type": "location",
                "keyword": "ely_cities",
                "css": "maps/map.css",
                "symbols": {
                    "default": [
                        {
                            "symbol": "city"
                        }
                    ]
                }
            },
	    {
		"qid": "cloud_ceiling_observations",
		"layer_type": "cloud_ceiling",
		"keyword": "synop_fi",
		"priority": "min",
		"mindistance": 35,
		"attributes": {
		    "fill": "rgba(0,0,255,1.00)", 
		    "font-family": "Verdana", 
		    "font-size": "16px", 
		    "text-anchor": "middle", 
		    "stroke": "#000000", 
		    "stroke-width": 0.5
		},		
		"label": {		    
		    "padding_char": "0",
		    "padding_length": 3,
		    "missing": "-",
		    "multiple": 100,
		    "dx": 6,
		    "dy": -10,
		    "rounding": "towardzero"
		}
	    }
        ]
    }]
}
</sub></code></pre></td><td><img src="images/cloud_ceiling.png"></td></tr>
</table>

The cloudceiling layer is inherited from number layer so the same attributes are available. Label object has two new attributes 'padding_char' and 'padding_length' to format numbers.



#### StreamLayer

The streamline layer is used for visualizing directional parameters such as wind direction, wave direction, ice drift direction etc.

<img src="images/streamline.png">

The table below contains a list of attributes that can be defined for the streamline layer in addition to the common layer attributes.

<pre><b>StreamLayer </b></pre>
| Name        | Type     | Default value | Description                                          |
| ----------- | -------- | ------------- | ---------------------------------------------------- |
| parameter   | (string) | -             | The parameter name for the direction.                |
| u           | (string) | -             | Alternative U-component parameter for the direction. |
| v           | (string) | -             | Alternative V-component parameter for the direction. |
| min_length  | (int)    | 5             | Minimum generated stream line length in pixels.      |
| max_length  | (int)    | 2048          | Maximum generated stream line length in pixels.      |
| line_length | (int)    | 32            | Length of a stream line segment in pixels.           |
| xstep       | (int)    | 20            | Streamline start point step size in x-direction.     |
| ystep       | (int)    | 20            | Streamline start point step size in y-direction.     |
| precision   | (double) | 1.0           | Precision of the generated coordinates.              |


#### RasterLayer

The core concept of a raster layer is to render an image from a set of grid-based values, where each grid cell corresponds to a pixel in the resulting raster image. The color of each pixel is determined by mapping its associated value to a position within a predefined color map. This color map is essentially a list of value-color pairs, allowing for the identification of lower and upper bounds for each input value, along with their associated colors. By default, the final color is computed via linear interpolation between these bounds on a per-channel basis (A, R, G, B), resulting in a smooth color gradient. This technique eliminates hard boundaries between value regions, yielding a more continuous and visually intuitive representation.

The value-to-color mapping is handled by so-called painter elements, which can vary in type. The current implementation supports the following painter types. 

1. Painter: "default"    
The "default" painter, utilizes a full value-to-color list to perform interpolation across multiple defined intervals. The relationships between values and the colors are defined in the colormap file. It is also possible to use colors without interpolation (smooth_colors=false), which makes them look like the vector based isobands.

2. Painter: "range"
The "range" painter functions almost similarly as the "default" painter. The difference is that the value ranges are part of the layer definition. Each range contains the minimum and maximum values of the dataset, along with their corresponding colors. In addition a range definition can contain colors that should be used above or below the given range. Usually this is the fastest way to define simple color ranges. For example, clouds can be painted so that only the opacity of the white color is changed according to the cloud coverage percent.

3. Painter: "stream"
The "stream" painter has the same idea as the Stream Layer, but the stream lines are drawn by using raster points. The direction of the stream lines are painted by using the variation of the alfa channel. Notice that the "stream" painter is capable for painting animation steps, which means that it can paint stream lines differently in each loop steps. This creates a moving effect when multiple images are put into the same animation file (WebP -file). It is also possible to make stream lines move faster in certain areas and this way simulate different stream speeds in different areas.

4. Painter: "border"
The "border" painter can be used for painting different kinds of border lines (like isolines). The current painter paints each border by using two colors: inside color and the outside color. This improves their visibility. For example, if the inside color is white and the outside color is black, then the border should be visible almost everywhere, because the background color cannot be dark and light at the same time. Notice that the "border" painter is also used as an additional painter for each layer. This means that in spite of that we have selected the "master painter" to be "default","range" or "stream", we can use the "border" painter with the same parameter. For example, if we use the "range" painter to paint clouds, we might want to make cloud borders more visible by using "data_borders" definition with the current painter. This definition contains the same attributes that would be given normally to the "border" painter.

5. Painter: "shadow"
The "shadow" painter can be used for painting shadows for the actual data. For example, we can paint shadows for clouds or raining areas. The basic idea is that we pick data ranges from the actual and paint this data with party transparent colors. In addition we shift these areas so that they are painter a little bit different location than the original data. This creates quite nice shadow effects. Notice that the "shadow" painter is also used as an additional painter for each layer. This means that in spite of that we have selected the "master painter" to be "default","range" or "stream", we can use the "shadow" painter with the same parameter. For example, if we use the "range" painter to paint clouds, we might want to create some shadows for these cloud by using "data_shadows" definition with the current painter. This definition contains the same attributes that would be given normally to the "shadow" painter.

   

<img src="images/raster.png">

<pre><b>Product configuration file</b></pre>
<pre><code><sub> {
  "title": "Temperature",
  "abstract": "Temperature",
  "source": "grid",
  "producer": "ec",
  "projection": {},
  "animation" :
  {
    "enabled"           : false,  // Enables/disables the animation
    "timesteps"         : 24,     // Number of animation timesteps
    "timestep_interval" : 90,     // Frame interval time (= milliseconds) between timesteps
    "data_timestep"     : 20,     // Timestep length (= minutes)in in the actual data
    "loopsteps"         : 1,      // Number of animation steps inside the same timestep
    "loopstep_interval" : 70      // Frame interval time (=milliseconds) between loopsteps
  },

  "views": 
  [
    {
      "layers":
      [
        {
          // ************************************************************
          //  Temperature
          // ************************************************************
          //  This demonstrates the usage of the "default" data painter,
          //  which paints data values according to color definitions
          //  specified in the "colormap" file.
          // ************************************************************

          "layer_type"            : "raster",
          "qid"                   : "temperature-1",
          "compression"           : 1,
          "visible"               : true,
          "parameter"             : "temperature",
          "interpolation"         : "linear",
          "data_painter"          : "default",
          "data_opacity_land"     : 1.0,
          "data_opacity_sea"      : 1.0,
          "colormap"              : "temperature",
          "smooth_colors"         : true,

          "land_position"         : "bottom",
          "land_color"            : "FFD0D0D0",

          "sea_position"          : "top",
          "sea_color"             : "FFCCE0F8",

          "land_border_position"  : "none",
          "land_border_color"     : "80000000",
          "sea_border_color"      : "80FFFFFF",

          "land_shading_position" : "top",
          "land_shading_light"    : 128,
          "land_shading_shadow"   : 384,

          "sea_shading_position"  : "top",
          "sea_shading_light"     : 128,
          "sea_shading_shadow"    : 384
        }
        ,
        {
          // ************************************************************
          //  High clouds 
          // ************************************************************
          //  This demonstrates the usage of the "range" data painter,
          //  which is almost the same as the "default" data painter. The
          //  main diffence is that it does not have colormaps, but it
          //  defines similar color ranges in the "ranges" array. In 
          //  addition it can define colors that are outside of the given
          //  range (color_low,color_high). This is a fast way to define
          //  colors for data that does not require many color ranges.
          //
          //  Notice that in this example we use interpolation method
          //  "transfer" that tries to calculate cloud transitions
          //  with a different algorithm (i.e. it does not use linear
          //  interpolation. The same algorithm can be used with rain.
          // ************************************************************

          "layer_type"            : "raster",
          "qid"                   : "high-clouds-1",
          "visible"               : true,
          "compression"           : 1,
          "parameter"             : "NH-PRCNT:ECGMTA:1008:6:0:1",
          "interpolation"         : "transfer",
          "data_painter"          : "range",
          "data_opacity_land"     : 1.0,
          "data_opacity_sea"      : 1.0,

          "ranges" : [
            {
              "value_min"         : 10,
              "value_max"         : 100,
              "color_min"         : "20FFFFFF",
              "color_max"         : "FFFFFFFF",
              "color_low"         : "00000000",
              "color_high"        : "00000000"
            }
          ]
          ,
          "data_shadows" : [
            {
              "value_min"         : 20,
              "value_max"         : 100,
              "color_min"         : "30000000",
              "color_max"         : "30000000",
              "dx"                : 5,
              "dy"                : 5
            }
          ]
          ,
          "land_position"         : "none",
          "land_color"            : "FF404040",

          "sea_position"          : "none",
          "sea_color"             : "FFCCE0F8",

          "land_border_position"  : "top",
          "land_border_color"     : "20000000",
          "sea_border_color"      : "20FFFFFF",

          "land_shading_position" : "none",
          "land_shading_light"    : 256,
          "land_shading_shadow"   : 384,

          "sea_shading_position"  : "none",
          "sea_shading_light"     : 128,
          "sea_shading_shadow"    : 384
        }
      ]
    }
  ]
} </sub></code></pre>


The table below contains a list of attributes that can be defined for the raster layer in addition to the common layer attributes.

<pre><b>RasterLayer </b></pre>
| Name               | Type     | Default value | Description                                                                    |
| ------------------ | -------- | ------------- | ------------------------------------------------------------------------------ |
| parameter          | (string) | -             | The parameter name for the direction.                                          |
| compression        | (int)    | 1             | Compression rate (1 = fast,low compression, 9 = slow, high compression).       |
| interpolation      | (string) | linear        | Interpolation method when fetching grid pixels (linear / nearest / transfer).  |
| data_painter       | (string) | default       | Painter element (default / range / stream / border / shadow / null).           |
| data_opacity_land  | (double) | 1.0           | Opacity of the painted parameter over the land (0 .. 1).                       |
| data_opacity_sea   | (double) | 1.0           | Opacity of the painted parameter over the sea (0 .. 1).                        |

The raster layer can paint land and sea colors above or below of the painted parameter. This allows you to create some nice effects. For example, if you want to make sea areas darker than the land areas the you can paint sea areas above the parameter layer by using colors with some transparency (for example 20000000).

| Name                 | Type     | Default value | Description                                                                    |
| -------------------- | -------- | ------------- | ------------------------------------------------------------------------------ |
| land_position        | (string) | none          | Land layer position compared to the painted parameter (none,bottom,top).       |
| land_color           | (ARGB)   | 00000000      | Land layer color.                                                              |
| sea_position         | (string) | none          | Sea layer position compared to the painted parameter (none,bottom,top).        |
| sea_color            | (ARGB)   | 00000000      | Sea layer color.                                                               |
| land_border_position | (string) | none          | Land border position compared to the painted parameter (none,bottom,top).      |
| land_border_color    | (ARGB)   | 00000000      | Land border color.                                                             |
| sea_border_color     | (ARGB)   | 00000000      | Sea border color.                                                              |

The raster layer can paint topographical shadings above or below the painted parameter. 

| Name                  | Type     | Default value | Description                                                                      |
| --------------------- | -------- | ------------- | -------------------------------------------------------------------------------- |
| land_shading_position | (string) | none          | Land shading layer position compared to the painted parameter (none,bottom,top). |
| land_shading_light    | (int)    | 0             | Multiplier for light areas (0..1000).                                            |
| land_shading_shadow   | (int)    | 0             | Multiplier for shadow areas (0..1000).                                           |
| sea_shading_position  | (string) | none          | Sea shading layer position compared to the painted parameter (none,bottom,top).  |
| sea_shading_light     | (int)    | 0             | Multiplier for light areas (0..1000). Bigger value inceases the lightness.       |
| sea_shading_shadow    | (int)    | 0             | Multiplier for shadow areas (0..1000). Bigger value inceases the darkness.       | 

The raster layer can create borders for the actual parameter. Technically this uses the "border" painter functionality, but in this case is more like an addition to the current painter.

| Name                  | Type     | Default value | Description                                                                      |
| --------------------- | -------- | ------------- | -------------------------------------------------------------------------------- |
| data_borders          | (struct) |               | This is a structure that contain same attributes as the "border" painter uses.   |

The raster layer can create shadows for the actual parameter. Technically this uses the "shadow" painter functionality, but in this case is more like an addition to the current painter.

| Name                  | Type     | Default value | Description                                                                      |
| --------------------- | -------- | ------------- | -------------------------------------------------------------------------------- |
| data_shadows          | (struct) |               | This is a structure that contain same attributes as the "shadow" painter uses.   |


Attributes for the painter "default".

| Name          | Type     | Default value | Description                                                                     |
| ------------- | -------- | ------------- | ------------------------------------------------------------------------------- |
| colormap      | (string) |               | Name of the colormap for "default" painter. This refers to a colormap file.     |
| smooth_colors | (bool)   | true          | Should the painter use "smooth colors" i.e. linarly interpolate colors.         |

Attributes for the painter "range".

| Name          | Type     | Default value | Description                                                                     |
| ------------- | -------- | ------------- | ------------------------------------------------------------------------------- |
| ranges        | (array)  |               | Array of range structures.                                                      |

Attributes for the "range" structure.

| Name          | Type     | Default value | Description                                                                     |
| ------------- | -------- | ------------- | ------------------------------------------------------------------------------- |
| value_min     | (float)  |               | The minimum value of the value range.                                           |
| value_max     | (float)  |               | The maximum value of the value range.                                           |
| color_min     | (ARGB)   | 00000000      | The color used with the minimum value (min_value).                              |
| color_max     | (ARGB)   | 00000000      | The color used with the maximum value (max_value).                              |
| color_low     | (ARGB)   | 00000000      | The color used with values that are smaller than the minimum value (min_value). |
| color_high    | (ARGB)   | 00000000      | The color used with values that are bigger than the maximum value (max_value).  |

Attributes for the painter "stream".

| Name              | Type     | Default value | Description                                                                 |
| ----------------- | -------- | ------------- | --------------------------------------------------------------------------- |
| stream_color      | (RGB)    | 000000        | The color used for stream lines.                                            |
| trace_length_min  | (int)    | 10            | Minimum trace length of the stream line.                                    |
| trace_length_max  | (int)    | 128           | Maximum trace length of the steram line.                                    |
| trace_step_x      | (int)    | 10            | Stream line trace start point step size in x-direction.                     |
| trace_step_y      | (int)    | 10            | Stream line trace start point step size in y-direction.                     |
| line_length       | (int)    | 16            | Length of the line segment.                     |

Stream lines can use colors for indicating the speed of the stream. In this case the 'parameter' attribute is replaced with 'speed' and 'direction' attributes. In addition, the 'colormap' attribute is used for selecting stream color according to the speed value.

| Name          | Type     | Default value | Description                                                                 |
| ------------- | -------- | ------------- | --------------------------------------------------------------------------- |
| direction     | (string) | -             | The parameter name for the direction.                                       |
| speed         | (string) | -             | The parameter name for the speed.                                           |
| colormap      | (string) |               | Name of the colormap for "default" painter. This refers to a colormap file. |
| smooth_colors | (bool)   | true          | Should the painter use "smooth colors" i.e. linarly interpolate colors.     |

If the data is given as 'parameter' attribute (without speed information) then the painter needs line colors. 

| Name             | Type     | Default value | Description                                                              |
| ---------------- | -------- | ------------- | ------------------------------------------------------------------------ |
| line_color_land  | (ARGB)   | FF000000      | The stream line color on the land area.                                  |
| line_color_sea   | (ARGB)   | FF000000      | The stream line color on the sea area.                                   |

Attributes for the "border" painter.

| Name              | Type     | Default value | Description                                                             |
| ----------------- | -------- | ------------- | ----------------------------------------------------------------------- |
| borders           | (array)  |               | The array of "border" structures.                                       |

Attributes for the "border" structure.

| Name              | Type     | Default value | Description                                                             |
| ----------------- | -------- | ------------- | ----------------------------------------------------------------------- |
| inside_value_min  | (float)  |               | The values bigger than this are inside the given border.                |
| steps             | (int)    |               | The number of steps (we can paint multiple borders by using steps).     |
| step_increase     | (float)  |               | After each step the "inside_value_min" is increased with this number.   |
| inside_color      | (ARGB)   | 00000000      | The border color inside the curren area.                                |
| outside_color     | (ARGB)   | 00000000      | The border color outside the current area.                              |

Attributes for the "shadow" painter.

| Name              | Type     | Default value | Description                                                             |
| ----------------- | -------- | ------------- | ----------------------------------------------------------------------- |
| shadows           | (array)  |               | The array of "shadow" structures.                                       |

Attributes for the "shadow" structure.

| Name          | Type     | Default value | Description                                                                 |
| --------------| -------- | ------------- | --------------------------------------------------------------------------- |
| value_min     | (float)  |               | The minimum value of the value range.                                       |
| value_max     | (float)  |               | The maximum value of the value range.                                       |
| color_min     | (ARGB)   | 00000000      | The color used with the minimum value (min_value).                          |
| color_max     | (ARGB)   | 40000000      | The color used with the maximum value (max_value).                          |
| dx            | (int)    | 10            | The number of pixels that the shadow painting is shifted in x-direction .   |
| dy            | (int)    | 10            | The number of pixels that the shadow painting is shifted in y-direction .   |



#### LegendLayer


The legend layer can be used  to define legends for the colors or symbols used in the product. 

The table below shows a simple example  on the usage of   the legend layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
   "title": "MyLegendLayer",
   "refs": {
       "myprojection": {
        "xsize": 300, "ysize": 550, "crs": "EPSG:2393",
      "bboxcrs": "EPSG:4326",  "cy": 64.8, "cx": 25.4,
      "resolution": 2.5
       }
   },
      "defs": {
    "styles": {
       ".Label": { "font": "Arial", "font-size": 9 },
       ".Units": { "font": "Arial", "font-size": 11 }
    },
    "layers": [  {
      "tag": "symbol",
      "attributes": { "id": "rect" },
      "layers": [ {
        "tag": "rect",
        "attributes": { "width": "14", "height": "12"}
       } ]
     },
     {
        "tag": "symbol",
        "attributes": { "id": "uptriangle" },
        "layers": [ {
        "tag": "path",
        "attributes": {"d": "M0 12,7 0,14 12Z"}
       } ]
     } ]
   },
   "projection": "ref:refs.myprojection",
   "views": [ { // *** View 1
     "layers": [ {  // *** Layer 1 : Wind speed labels
       "layer_type": "legend",
       "x": 10, "y": 10, "dx": 0, "dy": 12,
       "isobands": "json:isobands/wind.json",
          "symbols": {
        "css": "isobands/wind.css",
        "symbol": "rect",
        "start": "uptriangle",
        "attributes": {"stroke": "black", "stroke-width": "0.5"}
       },
       "labels":  {
        "type": "lolimit",
        "dx": 18,
        "dy": 14,
        "conversions": { "32": "&#62; 32" }
       },
       "attributes": {
         "transform": "translate(-7 0)",
        "id": "windlegend",
         "class": "Label"
       },
       "layers": [ {
         "tag": "text",
            "cdata": "m/s",
         "attributes": { "x": 28, "y": 12, "class": "Units" }
      } ]
    } ]
     } ]
}</sub></code></pre></td><td><img src="images/legend_ex.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for the legend layer in addition to the common layer attributes.

<pre><b>LegendLayer </b></pre>
| Name     | Type            | Default value | Description                                                         |
| -------- | --------------- | ------------- | ------------------------------------------------------------------- |
| x        | int             | 10            | X-coordinate of the first legend symbol.                            |
| y        | int             | 10            | Y-coordinate of the first legend symbol.                            |
| dx       | int             | 0             | X-coordinate offset to the next legend symbol.                      |
| dy       | int             | 20            | Y-coordinate offset to the next legend symbol.                      |
| symbols  | _LegendSymbols_ | -             | Symbols to be used in the legend.                                   |
| labels   | _LegendLabels_  | -             | Label definitions for the legend.                                   |
| isobands | _[Isoband]_     | -             | An array if _Isobands_ structures from which to generate the legend |


#### TimeLayer


The time layer can be used in order to add the date and time information into the view. 

The table below shows a simple example  on the usage of   the time layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
   "title": "MyTimeLayer",
   "projection" :
  {
    "crs": "EPSG:3067",
    "bboxcrs": "WGS84",
    "xsize": 320,
    "ysize": 384,
    "x1": 11.2,
    "x2": 31.5,
    "y1": 53.2,
    "y2": 66.5
   },
  "views": [
  { // *** View 1
       "layers": [
    { // *** Layer 1
       "layer_type" : "time",
         "timestamp" : "origintime",
       "timezone" : "UTC",
       "format" : "%d.%m.%Y/%H:%M:%S",
        "x" : 140,
         "y" : 120,
       "attributes": {
         "text-anchor": "middle",
          "font-size": 18
       }
     } ]
   } ]
}</sub></code></pre></td><td><img src="images/time_ex.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for the time layer in addition to the common layer attributes.

<pre><b>TimeLayer </b></pre>
| Name      | Type                 | Default value    | Description                                                                                                                                                                                                                                                                                                                                             |
| --------- | -------------------- | ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| timezone  | (string)             | UTC              | The time zone for the timestamp.                                                                                                                                                                                                                                                                                                                        |
| prefix    | (string)             | -                | Prefix for the full string to be printed.                                                                                                                                                                                                                                                                                                               |
| suffix    | (string)             | -                | Suffix for the full string to be printed.                                                                                                                                                                                                                                                                                                               |
| timestamp | (string) or [string] | validtime        | Type of the timestamp. Valid values are "validtime", "origintime", "starttime", "endtime" and "wallclock". "now" is an alias for "wallclock". A duration can be shown instead by selecting one of "time_offset", "interval_start", "interval_end", "leadtime", "leadhour", an ISO time duration string or a signed integer with a time indicator suffix |
| format    | (string) or [string] | "%Y-%m-%d %H:%M" | Format of the timestamp. See Boost time formatting flags.                                                                                                                                                                                                                                                                                               |
| x         | (int)                | -                | X-position of the timestamp. If the value is negative then the position is counted from the right edge.                                                                                                                                                                                                                                                 |
| y         | (int)                | -                | Y-position of the timestamp. If the value is negative then the position is counted from the bottom edge.                                                                                                                                                                                                                                                |

Both timestamp and format may be arrays, in which case their lengths should match. The formatted strings will be concatenated to the prefix and the suffix will be appended to the result. This feature can be used to print strings such as origintime + leadtime, starttime - endtime etc.

Times and time durations are formatted according to Boost.Date_time time formatter flag specifications.

#### TagLayer

The tag layer can be used  to add different kinds of SVG elements such as lines, rectangles, circles, text, etc. in  the view.


The table below shows a simple example  on the usage of   the tag layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="20">
<pre><code><sub>
{ // *** Product
   "title": "MyTagDemo",
   "width": 500,
   "height": 500,
   "views": [
    { // *** View 1
     "attributes":
     {
      "id": "view1"
     },
     "layers": [
     { // *** Layer 1: Yellow box with black borders
         "layer_type" : "tag",
         "tag" : "rect",
         "attributes" :
         {
        "x" : 150,
        "y" : 150,
        "width" : 200,
        "height" : 200,
        "fill" :  "yellow",
        "stroke" : "black"
         }
     },
     { // *** Layer 2 : Hello world text
         "layer_type" : "tag",
         "tag" : "text",
         "cdata" : "Hello world",
         "attributes" :
         {
        "x" : 250,
        "y" : 250,
        "font-size" :  18,
        "text-anchor" : "middle"
         }
     } ]
    } ]
}</sub></code></pre></td><td><img src="images/tag_ex.png"></td></tr>
<tr><td><b>Produced SVG file</b></td></tr>
<tr><td rowspan="10">
<pre><code><sub>
&lt;svg width="500" height="500" xmlns="http://www.w3.org/2000/svg"
&nbsp;&nbsp;&nbsp; xmlns:xlink="http://www.w3.org/1999/xlink"&gt;

&lt;title&gt;MyTagDemo&lt;/title&gt;

&lt;defs&gt;
&nbsp;&nbsp;&nbsp;&lt;style type="text/css"&lt;&gt;![CDATA[ ]]&gt;&lt;/style&gt;
&lt;/defs&gt;

&lt;g&gt;
&lt;g id="view1"&gt;

&lt;rect fill="yellow" height="200" style="stroke:black" width="200" x="150" y="150"/&gt;
&lt;text font-size="18" style="text-anchor:middle" x="250" y="250"&gt;Hello world&lt;/text&gt;

&lt;/g&gt;

&lt;/g&gt;


&lt;/svg&gt;
</sub></code></pre>
</td></tr>
</table>

The table below contains a list of attributes that can be defined for the tag layer in addition to the common layer attributes.

<pre><b>TagLayer </b></pre>
| Name  | Type     | Default value | Description                                                                       |
| ----- | -------- | ------------- | --------------------------------------------------------------------------------- |
| tag   | (string) | -             | The SVG tag to generate, for example "text" or "g".                               |
| cdata | (string) | -             | The CDATA section for the tag. This should not be used if the tag is stand-alone. |

#### WKTLayer

The WKT layer can be used insert an arbitrary geometry into the image in the Well Known Text format.

The table below shows a simple example  on the usage of the WKT layer.

<pre><code><sub>
{
    "title": "WKT data",

    "projection":
    {
        "xsize": 500,
        "ysize": 500,
        "crs": "EPSG:3035",
        "bboxcrs": "WGS84",
        "cx": 25,
        "cy": 60,
        "resolution": 10
    },
    "views": [
        {
            "qid": "v1",
            "layers": [
                {
                    "qid": "mymap",
                    "layer_type": "map",
                    "map":
                    {
                        "lines": true,
                        "schema": "natural_earth",
                        "minarea": 50,
                        "table": "admin_0_countries"
                    },
                    "attributes":
                    {
                        "id": "europe",
                        "stroke": "#333",
                        "stroke-width": "0.5px",
                        "fill": "none"
                    }
                },
                {
                    "layer_type": "wkt",
                    "qid": "test1",
                    // non segmented box around Finland
                    "wkt": "POLYGON((30 60, 30 70, 20 70, 20 60, 30 60))",
                    "attributes":
                    {
                        "fill": "none",
                        "stroke": "black",
                        "stroke-width": 1
                    }
                },
                {
                    "layer_type": "wkt",
                    "qid": "test2",
                    // a bit bigger box with segmentation to 100 km resolution
                    "wkt": "POLYGON((40 50, 40 75, 10 75, 10 50, 40 50))",
                    "resolution": 100,
                    "attributes":
                    {
                        "fill": "none",
                        "stroke": "red",
                        "stroke-width": 1
                    }
                },
                {
                    "layer_type": "wkt",
                    "qid": "test3",
                    // biggest box with segmentation to 20 pixel resolution
                    "wkt": "POLYGON((50 40, 50 80, 0 80, 0 40, 50 40))",
                    "relativeresolution": 20,
                    "attributes":
                    {
                        "fill": "none",
                        "stroke": "green",
                        "stroke-width": 1
                    }
                }
            ]
        }
    ]
}
</sub></code></pre>

<img src="images/wkt_ex.png">

The table below contains a list of attributes that can be defined for the WKT layer in addition to the common layer attributes.

<pre><b>WKTLayer </b></pre>
| Name               | Type     | Default value | Description                                |
| ------------------ | -------- | ------------- | ------------------------------------------ |
| wkt                | (string) | -             | The Well Known Text for the geometry.      |
| precision          | (double) | 1.0           | Precision of printed SVG coordinates.      |
| resolution         | (double) | -             | The segmentation resolution in kilometers. |
| relativeresolution | (double) | -             | The segmentation resolution in pixels.     |

By default the WKT is not segmented into smaller linesegments. However, if the CRS of the image is not geographic, long straight lines in the WKT will not curve as expected unless the WKT is segmented into multiple parts in a resolution suitable for the output image. In the example above the black WKT is not segmented at all, the red one is segmented to 100 km resolution, and the green one to 20 pixel resolution.

#### WeatherFrontsLayer

The `fronts` layer renders cold, warm, occluded, and stationary fronts as SVG paths decorated
with meteorological symbols (triangles and semi-circles) at even arc-length intervals.  The
symbols are rendered using a `<textPath>` element referencing a **weather-font** that maps
specific characters to the standard front glyphs.

Two data sources are supported via the `source` field:

| `source`      | Description |
| ------------- | ----------- |
| `"synthetic"` | Curves defined inline in the product JSON (default). Useful for testing and for manually drawn analyses. |
| `"grid"`      | Fronts detected on the fly from querydata using Hewson's Thermal Front Parameter (TFP) method. |

##### Front types

| Type          | JSON string    | Default line class | Default glyph characters |
| ------------- | -------------- | ------------------ | ------------------------ |
| Cold front    | `"cold"`       | `coldfront`        | `C` / `c`                |
| Warm front    | `"warm"`       | `warmfront`        | `W` / `w`                |
| Occluded      | `"occluded"`   | `occludedfront`    | `O` / `o`                |
| Stationary    | `"stationary"` | `stationaryfront`  | `S` / `s`                |
| Trough        | `"trough"`     | `trough`           | `T` / `t`                |
| Ridge         | `"ridge"`      | `ridge`            | `R` / `r`                |

Symbols are rendered as inline SVG shapes (filled triangles and semicircles) — no external
font is required.  Uppercase glyph characters produce symbols on the **left** side of the
directed path; lowercase on the **right** side.  The layer places alternating symbols at
even arc-length intervals using the spacing parameters below.

| Glyph char | Shape |
| ---------- | ----- |
| `C` | Filled triangle pointing left of path (cold front active side) |
| `c` | Filled triangle pointing right of path (cold front passive side) |
| `W` | Filled semicircle bulging left of path (warm front active side) |
| `w` | Filled semicircle bulging right of path (warm front passive side) |

Compound glyphs (`"CW"`, `"Cw"`, etc.) place multiple shapes at the same position, used for
occluded and stationary fronts.

##### WeatherFrontsLayer settings

| Name     | Type   | Default      | Description |
| -------- | ------ | ------------ | ----------- |
| source   | string | `"synthetic"` | Data source: `"synthetic"` or `"grid"`. |
| fronts   | array  | `[]`         | Front curve definitions (used when `source` is `"synthetic"`). See below. |
| grid     | object | –            | Grid-detection configuration (used when `source` is `"grid"`). See below. |
| styles   | object | –            | Per-type style overrides. Keys are front type strings (e.g. `"cold"`). See below. |

##### `fronts` array (synthetic source)

Each element in the `fronts` array defines one front curve:

| Name   | Type   | Default  | Description |
| ------ | ------ | -------- | ----------- |
| type   | string | `"cold"` | Front type: `"cold"`, `"warm"`, `"occluded"`, `"stationary"`, `"trough"`, or `"ridge"`. |
| side   | string | `"left"` | Which side the symbols face: `"left"` or `"right"`. |
| points | array  | –        | Array of `[longitude, latitude]` pairs (WGS84) defining the front path. Required. |

##### `grid` object (grid source)

Fronts are detected by computing Hewson's Thermal Front Parameter (TFP):

```
TFP = -∇(|∇θ|) · (∇θ / |∇θ|)
```

Zero-crossings of TFP with `|∇θ|` above `min_gradient` are extracted as isolines and
classified as cold or warm fronts based on the sign of the low-level temperature advection
`V · ∇θ` at 850 hPa.

| Name            | Type   | Default                  | Description |
| --------------- | ------ | ------------------------ | ----------- |
| producer        | string | (default producer)       | Querydata producer name. |
| theta_param     | string | `"PotentialTemperature"` | Potential temperature (or temperature) parameter name. |
| u_param         | string | `"WindUMS"`              | U-component of wind parameter name. |
| v_param         | string | `"WindVMS"`              | V-component of wind parameter name. |
| level           | double | `850.0`                  | Pressure level in hPa. |
| min_gradient    | double | `2e-6`                   | Minimum `|∇θ|` threshold (K/m). Grid points below this are masked as missing before contouring. |
| min_length_px   | double | `20.0`                   | Minimum front segment length in screen pixels. Shorter segments are discarded. |

##### `styles` object

The `styles` object maps front type names to style overrides.  Any omitted field keeps its
default value.

| Name       | Type   | Default                | Description |
| ---------- | ------ | ---------------------- | ----------- |
| line_css   | string | (type-specific)        | CSS class name applied to the `<use>` stroke path. |
| glyph_css  | string | (type-specific)        | CSS class name applied to the `<text>` glyph group. |
| glyph1     | string | (type-specific)        | Character rendered at odd-numbered symbol positions. |
| glyph2     | string | (type-specific)        | Character rendered at even-numbered symbol positions. |
| font_size  | double | `30.0`                 | Glyph font size in pixels. |
| spacing    | double | `60.0`                 | Arc-length spacing between glyph centres in pixels. |

##### Example 1 – synthetic fronts (cold, warm, occluded, stationary)

<img src="images/fronts_ex.png">

The product JSON that produced the image above:

```json
{
    "title": "Weather fronts – synthetic example",
    "projection": {
        "xsize": 700, "ysize": 500,
        "crs": "EPSG:3857", "bboxcrs": "WGS84",
        "x1": -2, "y1": 47, "x2": 42, "y2": 67
    },
    "views": [{
        "qid": "v1",
        "layers": [
            { "layer_type": "background",
              "attributes": { "fill": "rgb(190,230,255)" } },
            { "qid": "land", "layer_type": "map",
              "map": { "schema": "natural_earth", "table": "admin_0_countries", "minarea": 50 },
              "attributes": { "fill": "rgb(220,220,210)", "stroke": "none" } },
            { "qid": "borders", "layer_type": "map",
              "map": { "lines": true, "schema": "natural_earth",
                       "table": "admin_0_countries", "minarea": 50 },
              "attributes": { "fill": "none", "stroke": "#555", "stroke-width": "0.5px" } },
            {
                "qid": "wf",
                "layer_type": "fronts",
                "css": "fronts/fronts.css",
                "source": "synthetic",
                "fronts": [
                    { "type": "cold", "side": "left",
                      "points": [[6,56.5],[8.5,55.5],[11,54.5],[14.5,54],[18,54.5],[21.5,55.5],[24.5,57],[27,59]] },
                    { "type": "warm", "side": "right",
                      "points": [[6,56.5],[5.5,58.5],[6,60.5],[7.5,62.5],[9,64],[11,65]] },
                    { "type": "occluded", "side": "left",
                      "points": [[6,56.5],[5,54.5],[4.5,52],[5,50],[7,48.5]] },
                    { "type": "stationary", "side": "left",
                      "points": [[27,59],[30,59.5],[33,60.5],[36,62],[38,63.5]] }
                ]
            }
        ]
    }]
}
```

##### Example 2 – automatic front detection from querydata

```json
{
    "title": "NWP front analysis 850 hPa",
    "producer": "ecmwf_skandinavia_painepinta",
    "projection": { "crs": "EPSG:3857", "xsize": 800, "ysize": 600 },
    "views": [{
        "layers": [
            { "layer_type": "map", "map": "finland" },
            {
                "layer_type": "fronts",
                "source": "grid",
                "css": "fronts/fronts.css",
                "grid": {
                    "producer": "ecmwf_skandinavia_painepinta",
                    "theta_param": "PotentialTemperature",
                    "u_param": "WindUMS",
                    "v_param": "WindVMS",
                    "level": 850,
                    "min_gradient": 2e-6,
                    "min_length_px": 30
                },
                "styles": {
                    "cold": { "font_size": 24, "spacing": 48 },
                    "warm": { "font_size": 24, "spacing": 48 }
                }
            }
        ]
    }]
}
```

The CSS file (`fronts/fronts.css`) defines the stroke colours for the front lines and the
fill colour for the symbol shapes:

```css
.coldfront       { fill: none; stroke: #0044cc; stroke-width: 2.5; }
.warmfront       { fill: none; stroke: #cc2200; stroke-width: 2.5; }
.occludedfront   { fill: none; stroke: #880088; stroke-width: 2.5; }
.stationaryfront { fill: none; stroke: #444444; stroke-width: 2.0; }
.trough          { fill: none; stroke: #996600; stroke-width: 1.5; stroke-dasharray: 6 3; }
.ridge           { fill: none; stroke: #006622; stroke-width: 1.5; stroke-dasharray: 6 3; }

.coldfrontglyph       { fill: #0044cc; stroke: none; }
.warmfrontglyph       { fill: #cc2200; stroke: none; }
.occludedfrontglyph   { fill: #880088; stroke: none; }
.stationaryfrontglyph { fill: #444444; stroke: none; }
.troughglyph          { display: none; }
.ridgeglyph           { display: none; }
```

#### HovmoellerLayer

A Hovmöller layer renders a two-dimensional time–space cross-section (or vertical cross-section)
as a grid of coloured rectangles inside an ordinary SVG product.  Unlike all other layers the
coordinate axes are **not** a geographic map projection.  Five slice geometries are supported:

| `direction`   | X axis (columns)               | Y axis (rows)       | Fixed coordinate(s)          |
| ------------- | ------------------------------ | ------------------- | ---------------------------- |
| `time_lon`    | Longitude, evenly spaced       | Time steps          | Latitude, pressure level     |
| `time_lat`    | Latitude, evenly spaced        | Time steps          | Longitude, pressure level    |
| `time_level`  | Pressure levels                | Time steps          | Latitude + longitude         |
| `lon_level`   | Longitude, evenly spaced       | Pressure levels     | Latitude + valid time        |
| `lat_level`   | Latitude, evenly spaced        | Pressure levels     | Longitude + valid time       |

The three `time_*` directions (true Hovmöller diagrams) iterate **all** time steps available in
the querydata; the `time` query-string parameter is ignored for those.  The two `*_level`
directions are vertical cross-sections (not strictly Hovmöller diagrams) that render the single
time step selected by the standard `time=` request parameter.

Each cell is coloured by matching the interpolated value against the first matching `Isoband`
definition, using the same isoband JSON and CSS files as `IsobandLayer`.

The canvas pixel dimensions are taken from the product-level `projection.xsize` /
`projection.ysize`.  No geographic `crs` is required in the projection block.

##### HovmoellerLayer settings

| Name        | Type     | Default   | Description |
| ----------- | -------- | --------- | ----------- |
| parameter   | string   | –         | Parameter name, e.g. `"GeopHeight"` or `"Temperature"`. Required. |
| level       | double   | –         | Pressure level in hPa for `time_lon` and `time_lat`. Inherited from the shared `Properties` block so it can also be set via the `level=` query-string parameter. |
| direction       | string   | `time_lon` | Slice geometry: `time_lon`, `time_lat`, `time_level`, `lon_level`, or `lat_level`. |
| latitude        | double   | `60.0`     | Fixed latitude for `time_lon`, `time_level`, `lon_level`. |
| longitude       | double   | `25.0`     | Fixed longitude for `time_lat`, `time_level`, `lat_level`. |
| lon_min         | double   | `5.0`      | Western longitude bound for `time_lon` and `lon_level`. |
| lon_max         | double   | `35.0`     | Eastern longitude bound for `time_lon` and `lon_level`. |
| lat_min         | double   | `55.0`     | Southern latitude bound for `time_lat` and `lat_level`. |
| lat_max         | double   | `70.0`     | Northern latitude bound for `time_lat` and `lat_level`. |
| nx              | integer  | `50`       | Number of evenly-spaced sample points along the space axis. Ignored for `time_level` (the number of levels in the data is used). |
| time_ascending  | boolean  | `true`     | `true` = oldest time at the top (classic Hovmöller convention, time increases downward). `false` = newest time at the top. Has no effect on `*_level` directions. |
| level_ascending | boolean  | `false`    | `false` = surface (high hPa) on the left / top (meteorological convention). `true` = surface on the right / bottom. Applies to all `*_level` directions. |
| isobands        | string   | –          | Path to the isoband colour definitions, e.g. `"json:isobands/geoph500.json"`. |
| css             | string   | –          | Path to the CSS stylesheet for the isoband classes. |

**Notes on level ordering and sparse data**

For all `*_level` directions the levels are drawn with surface (high hPa) on the left / top by
default (`level_ascending=false`).  The data is commonly stored in ascending pressure order
(300, 500 … 1000 hPa); the layer reverses this automatically so 1000 hPa appears first.

With only six pressure levels (as in the test data) the cells are wide and the diagram is more
schematic than meteorologically detailed, but it clearly shows the vertical structure.  Finer
resolution requires model-level or high-density pressure-level querydata.

##### Example 1 – time × longitude (classic Hovmöller)

Shows how 500 hPa geopotential height ridges and troughs propagate eastward along 60°N
over 10 days.

```json
{
    "title": "Hovmoeller GeopHeight 500 hPa",
    "producer": "ecmwf_skandinavia_painepinta",
    "projection": { "xsize": 700, "ysize": 500 },
    "views": [{
        "layers": [{
            "layer_type": "hovmoeller",
            "parameter": "GeopHeight",
            "level": 500,
            "direction": "time_lon",
            "latitude": 60.0,
            "lon_min": 5.0,
            "lon_max": 35.0,
            "nx": 53,
            "isobands": "json:isobands/geoph500.json",
            "css": "isobands/geoph500.css"
        }]
    }]
}
```

Request: `GET /dali?customer=…&product=hovmoeller_geoph500`

![Hovmöller time × longitude – 500 hPa geopotential height](images/hovmoeller_geoph500.png)

*Time increases downward (oldest at top). X axis: 5°E–35°E along 60°N. Colour scale: blue = trough / low GPH, red = ridge / high GPH.*

##### Example 2 – time × latitude

Shows the meridional propagation of the 500 hPa ridge/trough pattern along 20°E.

```json
{
    "title": "Hovmoeller GeopHeight 500 hPa – time vs latitude",
    "producer": "ecmwf_skandinavia_painepinta",
    "projection": { "xsize": 700, "ysize": 500 },
    "views": [{
        "layers": [{
            "layer_type": "hovmoeller",
            "parameter": "GeopHeight",
            "level": 500,
            "direction": "time_lat",
            "longitude": 20.0,
            "lat_min": 52.0,
            "lat_max": 70.0,
            "nx": 57,
            "isobands": "json:isobands/geoph500.json",
            "css": "isobands/geoph500.css"
        }]
    }]
}
```

Request: `GET /dali?customer=…&product=hovmoeller_geoph500_time_lat`

![Hovmöller time × latitude – 500 hPa geopotential height](images/hovmoeller_geoph500_time_lat.png)

*Time increases downward (oldest at top). X axis: 52°N–70°N along 20°E.*

##### Example 3 – time × pressure level

Shows the vertical thermal evolution at a fixed location (60°N, 20°E) over the forecast
period.  All levels in the querydata are used automatically.

```json
{
    "title": "Hovmoeller Temperature – time vs pressure level",
    "producer": "ecmwf_skandinavia_painepinta",
    "projection": { "xsize": 700, "ysize": 500 },
    "views": [{
        "layers": [{
            "layer_type": "hovmoeller",
            "parameter": "Temperature",
            "direction": "time_level",
            "latitude": 60.0,
            "longitude": 20.0,
            "isobands": "json:isobands/temperature_levels.json",
            "css": "isobands/temperature_levels.css"
        }]
    }]
}
```

Request: `GET /dali?customer=…&product=hovmoeller_temperature_time_level`

![Hovmöller time × pressure level – temperature at 60°N 20°E](images/hovmoeller_temperature_time_level.png)

*Time increases downward (oldest at top). X axis: pressure levels from surface (1000 hPa, left) to upper troposphere (300 hPa, right). Colour scale: red = warm (surface, low levels), blue = cold (upper troposphere).*

#### GraticuleLayer

The graticule layer is used to draw a latitude-longitude grid.

A sample configuration from the tests (graticule_num_cross):

<pre><code><sub>
{
    "title": "Graticule demo",
    "producer": "kap",
    "language": "fi",
    "projection":
    {
        "crs": "data",
        "xsize": 500,
        "ysize": 500
    },
    "views":
    [
        {
            "qid": "v1",
            "layers":
            [
                {
                    "qid": "l1",
                    "layer_type": "map",
                    "map":
                    {
                        "schema": "natural_earth",
                        "table": "admin_0_countries",
                        "minarea": 100
                    },
                    "attributes":
                    {
                        "id": "europe_country_lines",
                        "fill": "none",
                        "stroke": "#222",
                        "stroke-width": "1px"
                    }
                },
                {
                    "qid": "l2",
                    "layer_type": "graticule",
                    "attributes":
                    {
                        "font-family": "Roboto",
                        "font-size": 10,
                        "text-anchor": "middle"
                    },
                    "graticules":
                    [
                        {
                            "layout": "grid",
                            "step": 1,
                            "except": 10,
                            "attributes":
                            {
                                "fill": "none",
                                "stroke": "#888",
                                "stroke-width": "0.2px"
                            }
                        },
                        {
                            "layout": "grid",
                            "step": 10,
                            "attributes":
                            {
                                "fill": "none",
                                "stroke": "#000",
                                "stroke-width": "0.5px"
                            },
                            "labels":
                            {
                                "layout": "cross",
                                "dy": 5,
                                "attributes":
                                {
                                    "fill": "black"
                                },
                                "textattributes":
                                {
                                    "filter": "url(#rectbackground?border=black&background=white&borderwidth=1)"
                                }
                            }
                        }
                    ]
                }
            ]
        }
    ]
}

</sub></code></pre>

The generated image is:
<img src="images/graticule.png">

##### GraticuleLayer settings
 |Name|Type|Default value|Description|
 |----|----|-------------|-----------|
 |graticules|[Graticule]|-|Vector of graticule definitions|
 |mask      |(string)   |-|Optional mask to be used (for example "url(#alphadilation)"|
 |mask_id   |(string)   |-|If empty, qid+mask will be used|
 |precision |(double)   |1.0|Coordinate output precision|

##### Graticule settings

 |Name|Type|Default value|Description|
 |----|----|-------------|-----------|
 |layout|(string)|"grid"|Normal "grid" or "ticks" at the image edges|
 |step  |(integer)|10   |Desired multiples in degrees|
 |except|([int)]) |-    |Undesired multiples in degrees|
 |length|int      |5    |Tick length in pixels when layout=ticks|
 |attributes|(Attributes)|-|Presentation attributes for the lines|
 |labels|(GraticuleLabels)|-|Optional label definitions for the lines|

 ##### GraticuleLabels

|Name|Type|Default value|Description|
|----|----|-------------|-----------|
|layout|(string)|"none"|Label layout algorithm: none|edges|grid|center|left_bottom|cross|
|step|int|-|Desired multiples in degrees, by default inherited from Graticule settings|
|orientation|(string)|"horizontal"|Label orientation: horizontal|auto|
|degree_sign|(bool)|true|If true, a degree sign will be appended to the number|
|minus_sign|(bool)|true|If false, N/S/W/E will be appended at the end|
|dx|(int)|0|X-offset when applicable in the selected layout|
|dy|(int)|0|Y-offset when applicable in the selected layout|
|attributes|(Attributes)|-|Presentation attributes for the group of labels|
|textattributes|(Attributes)|-|Presentation attributes for individual labels|

#### CircleLayer

The circle layer is typically used to draw circles of specific radius around airports to indicate a specific area of interest.

A sample configuration from the plugin tests:

<pre><code><sub>
{
    "title": "Circles",

    "projection":
    {
        "xsize": 500,
        "ysize": 500,
        "crs": "EPSG:3035",
        "bboxcrs": "WGS84",
        "cx": 25,
        "cy": 60,
        "resolution": 1
    },
    
    "views": [
        {
            "qid": "v1",
            "layers": [
                {
                    "qid": "mymap",
                    "layer_type": "map",
                    "map":
                    {
                        "lines": true,
                        "schema": "natural_earth",
                        "minarea": 50,
                        "table": "admin_0_countries"
                    },
                    "attributes":
                    {
                        "id": "europe",
                        "stroke": "#333",
                        "stroke-width": "0.5px",
                        "fill": "none"
                    }
                },
                {
                    "layer_type": "circle",
                    "qid": "c",
                    "places":
                    [
                        "Helsinki",
                        "Tampere",
                        "Turku"
                    ],
                    "circles":
                    [
                        {
                            "radius": 20,
                            "attributes":
                            {
                                "stroke": "black",
                                "stroke-width": 1.5
                            }
                        },
                        {
                            "radius": 50,
                            "attributes":
                            {
                                "stroke": "black",
                                "stroke-width": 0.5
                            }
                        }
                    ],
                    "attributes":
                    {
                        "fill": "none"
                    }
                }
            ]
        }
    ]
}

</sub></code></pre>

The image generated from the configuration file:

<img src="images/circles.png">

##### CircleLayer settings
 |Name|Type|Default value|Description|
 |----|----|-------------|-----------|
 |places|[string]|-|List of location names|
 |geoids|[int]|-|List of geonames.org IDs|
 |keyword|(string)|-|Keyword for the database list of location names|
 |features|(string)|-|List of feature names such as SYNOP,PPL etc (applies to places search only)|
 |lines|(bool)|true|Treat the circles as polylines or as polygons|
 |circles|Circle/[Circle]|-|One or several circle definitions|
 |labels|(CircleLabels)|-|Optional circle radius labels|

##### Circle settings

 |Name|Type|Default value|Description|
 |----|----|-------------|-----------|
 |radius|(double)|-|Circle radius in kilometers|
 |attributes|(Attributes)|-|Presentation attributes for the circle|
 

##### CircleLabels settings

 |Name|Type|Default value|Description|
 |----|----|-------------|-----------|
 |layout|[string]|-|Positions for labels (top, left, right, bottom, east, west, north, south)|
 |attributes|(Attributes)|-|Common attributes for all labels|
 |textattributes|(Attributes)|-|Attributes for individual labels|
 |dx|(int)|0|Coordinate adjustment|
 |dy|(int)|0|Coordinate adjustment|
 |prefix|(string)|-|Prefix for the label, for example "R="|
 |suffix|(string)|-|Suffix for the label, for example " km"|
 


#### TranslationLayer

The translation layer can be used  to create products that support multiple languages. When a product is requested we can use "language" parameter to define which language to use. In this case the "language" parameter overrides the value of the "language" attribute in the product file.

The table below shows a simple example  on the usage of   the translation layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
   "title": "MyTranslationDemo ",
   "language": "en",
   "projection":
   {
    "crs": "data",
    "xsize": 500,
    "ysize": 500
   },
   "attributes":
   {
    "filter": "url(#shadow)"
   },
   "views": [
    {
     "qid": "v1",
     "attributes":
     {
      "id": "view1"
     },
     "layers": [
    {
       "layer_type": "translation",
       "translations":
       {
        "en": "Hello World!",
        "fi": "Moro maailma!"
       },
       "attributes":
       {
        "x": "250",
        "y": "250",
        "font-family": "Verdana",
        "font-weight": "bold",
        "font-size": "55",
        "fill": "yellow",
        "stroke": "black",
        "text-anchor": "middle",
        "filter": "url(#shadow)"
       }
    } ]
    } ]
}</sub></code></pre></td><td><img src="images/hello_ex.png"></td></tr>
<tr><td><img src="images/moro_ex.png"></td></tr>
</table>


The table below contains a list of attributes that can be defined for the translation layer in addition to the common layer attributes.

<pre><b>TranslationLayer </b></pre>
| Name         | Type            | Default value | Description                                                                                                  |
| ------------ | --------------- | ------------- | ------------------------------------------------------------------------------------------------------------ |
| tag          | string          | "text"        | The SVG tag to be used.                                                                                      |
| translations | {string:string} | -             | The text to be selected for the active language. The language code is the key, the value is the translation. |


#### WindRoseLayer

The windrose layer can be used to add "wind roses" into the view. A wind rose shows the average wind speed for different directions over the given time interval.

The table below shows a simple example  on the usage of   the windrose layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{
   "projection": ...,
   "defs": ...,
   "views": [
  {
     "layers": [
    {
       "layer_type": "windrose",
       "timezone": "UTC",   "starttimeoffset": -12, "endtimeoffset": 12,
       "css": "wind/windrose.css",
       "windrose": {
        "minpercentage": 3, "radius": 30, "sectors": 8,
        "symbol": "windrose",
        "attributes": { "class": "WindRose" },
        "connector": {
           "startoffset": 2, "endoffset": 30,
           "attributes": { class": "RoseConnector" }
        },
        "parameter": "max_t(WindSpeed)",
        "limits": [
           { "hilimit": 8,"attributes": { "class": "LightWind" } },
           { "lolimit": 8, "hilimit": 14, "attributes":
             { "class": "ModerateWind" } },
           { "lolimit": 14, "attributes":  { "class": "HeavyWind" } } ]
       },
       "observations": [
      {
        "parameter": "mean_t(T)",
         "label": { "dx": 25,   "dy": 16, "suffix": " &#176;C" },
         "attributes": { class": "RoseTemperature"}
      },
      {
         "parameter": "mean_t(ws_10min)",
         "label": { "dx": 25,  "dy": 3, "suffix": " m/s" },
         "attributes":  { "class": "RoseMeanWind"}
      },
      {
         "parameter": "max_t(ws_10min)",
         "label": { "dx": 25,   "dy": -10, "suffix": " m/s" },
         "attributes": { "class": "RoseMaxWind"}
      } ],
          "stations": [
      {
         "fmisid": 100908, "longitude": 23,  "latitude": 61,
         "symbol": "station",
         "attributes": { "class": "StationMarker"},
         "title": {
          "text": "Utö, "dx": -10,  "dy": -26,
          "attributes": { "class": "StationName" }
         }
      },
      {
           "fmisid": 101673, "longitude": 25.3,  "latitude": 63.8,
         "symbol": "station",
         "attributes": { "class": "StationMarker" },
         "title": {
          "text": "Ulkokalla", "dx": -20, "dy": -26,
          "attributes": { "class": "StationName" }
         }
      } ]
    } ]
  } ]
}</sub></code></pre></td><td><img src="images/windrose_ex.png"></td></tr>
</table>


The table below contains a list of attributes that can be defined for the windrose layer in addition to the common layer attributes.

<pre><b>WindRoseLayer </b></pre>
| Name            | Type            | Default value | Description                                                                                             |
| --------------- | --------------- | ------------- | ------------------------------------------------------------------------------------------------------- |
| timezone        | string          | "UTC"         | The time zone used for the time settings.                                                               |
| starttimeoffset | int             | 0             | Defines the (backward) time period with respect to the valid time of the actual time set for the layer. |
| endtimeoffset   | int             | 24            | Defines the (backward) time period with respect to the valid time of the actual time set for the layer. |
| windrose        | _WindRose_      | -             | Wind rose appearance definitions.                                                                       |
| stations        | _[Station]_     | -             | The stations to use for the observations.                                                               |
| observations    | _[Observation]_ | -             | The observations to be used.                                                                            |



#### OSMLayer

The OSM layer renders OpenStreetMap data loaded into PostGIS via `osm2pgsql`. It supports polygon features (landuse, water, buildings), linear features (roads, coastlines, administrative boundaries) and place-name labels. Each layer response carries an HTTP ETag derived from the OSM data age, enabling efficient HTTP caching. The OGC API – Tiles endpoint also supports Mapbox Vector Tile (MVT/PBF) output via the `addMVTLayer` mechanism.

**Data setup and scale guidance**

OSM data is not bundled; it must be downloaded and imported separately. The appropriate data source depends heavily on the target map scale:

| Scale (km/px) | Recommended layer | Notes |
|---|---|---|
| > 5 km/px (continental/global) | `MapLayer` with Natural Earth | Natural Earth is already part of the GIS engine; no extra import needed. Better generalised than OSM for small-scale overviews. |
| 1–5 km/px (global overview) | Natural Earth or simplified OSM planet | A simplified OSM planet extract (e.g. osmdata.xyz land polygons, ~400 MB) is usable here but offers no significant advantage over Natural Earth. Full planet.osm.pbf (~80 GB uncompressed) is needed for global coverage below ~5 km/px. |
| 0.1–1 km/px (regional) | Regional Geofabrik extract | E.g. `scandinavia-latest.osm.pbf` (~350 MB). OSM PostGIS starts paying off in detail and feature richness below ~500 m/px. |
| < 0.1 km/px (city/local) | Full local OSM extract | Geofabrik city or sub-region extract; can include buildings, POIs, fine road network. |

> **Note:** A simplified OSM planet extract is NOT a good substitute for Natural Earth at global scales (> 5 km/px). Its generalisation is inconsistent and file sizes are large for the quality achieved. Use Natural Earth (via `MapLayer`) for global/continental backgrounds and reserve OSM PostGIS for regional or local detail layers.

Import command using `osm2pgsql`:
```sh
osm2pgsql -d smartmet -U smartmet \
  --schema osm_scandinavia \
  --middle-schema osm_scandinavia \
  --output flex --style osm2pgsql-flex-config.lua \
  scandinavia-latest.osm.pbf
```

`osm2pgsql` creates the standard tables `planet_osm_polygon`, `planet_osm_line`, `planet_osm_point` and `planet_osm_roads` inside the configured schema.

**ETag / caching**

Configure a `timestamp_file` path. After each `osm2pgsql` import, touch that file:
```sh
touch /var/smartmet/osm/scandinavia.timestamp
```
The layer uses the file's modification time as the data version. All tile responses for the same viewport return HTTP `304 Not Modified` until the file is touched again.

**Multi-resolution strategy**

Use `MapLayer` (Natural Earth) for global context and switch to OSMLayer for regional/local detail:

| Resolution range | Layer type | Data source | Purpose |
|---|---|---|---|
| > 5 km/px | `map` (Natural Earth) | Built-in GIS engine | Global/continental background |
| 0.5–5 km/px | `osm` (simplified extract) | osmdata.xyz land polygons or similar | Coarse regional overview |
| < 0.5 km/px | `osm` (full regional extract) | Geofabrik regional `.osm.pbf` | Detailed local map |

**CSS styling and themes**

The `css` field of each feature set specifies the stylesheet file to load. The `attributes.class` field selects which CSS class from that stylesheet applies to the SVG elements rendered for that feature set. This means one CSS theme file can style all feature types:

```json
{ "css": "osm/osm-bright", "attributes": { "class": "water" } }
```

See the [OSM style themes](#osm-style-themes) section below for ready-made CSS files.

**Sample configuration**

The example below combines Natural Earth (global background) with an OSM PostGIS layer (regional detail). Both feature sets use the `osm-bright` theme from `osm/osm-bright.css`.

```json
{
  "layer_type": "group",
  "layers": [
    {
      "comment": "Global background from Natural Earth — no OSM import needed",
      "layer_type": "map",
      "minresolution": 2.0,
      "map": { "schema": "natural_earth", "table": "admin_0_countries", "minarea": 50 },
      "attributes": { "fill": "rgb(220,220,210)", "stroke": "none" }
    },
    {
      "comment": "Regional OSM detail — requires osm_scandinavia PostGIS schema",
      "layer_type": "osm",
      "maxresolution": 2.0,
      "timestamp_file": "/var/smartmet/osm/scandinavia.timestamp",
      "feature_sets": [
        {
          "pgname": "postgis",
          "schema": "osm_scandinavia",
          "table": "planet_osm_polygon",
          "where": "natural = 'water'",
          "mvt_layer_name": "water",
          "css": "osm/osm-bright",
          "attributes": { "class": "water" }
        },
        {
          "pgname": "postgis",
          "schema": "osm_scandinavia",
          "table": "planet_osm_polygon",
          "where": "landuse IN ('forest','wood')",
          "mvt_layer_name": "landuse_forest",
          "css": "osm/osm-bright",
          "attributes": { "class": "forest" }
        },
        {
          "pgname": "postgis",
          "schema": "osm_scandinavia",
          "table": "planet_osm_line",
          "where": "highway IN ('motorway','trunk','primary','secondary')",
          "lines": true,
          "mvt_layer_name": "roads_major",
          "css": "osm/osm-bright",
          "attributes": { "class": "primary" }
        },
        {
          "pgname": "postgis",
          "schema": "osm_scandinavia",
          "table": "planet_osm_point",
          "where": "place IN ('city','town','village')",
          "fieldnames": ["name", "place"],
          "labels": true,
          "mvt_layer_name": "place_labels",
          "css": "osm/osm-bright",
          "attributes": { "class": "place_label" }
        }
      ]
    }
  ]
}
```

> **Note:** A rendered sample image will be added once OSM data has been imported. See the data setup instructions above.

OSMLayer supports two backends. The `backend` attribute selects which one is used; the required companion attributes differ between them.

**PostGIS backend** (`"backend": "postgis"`, default): renders from `osm2pgsql`-imported PostGIS tables via the GIS engine. Requires `feature_sets`.

**PMTiles backend** (`"backend": "pmtiles"`): renders from a memory-mapped `.pmtiles` file managed by the OSM engine (`engines/osm`). Requires the OSM engine to be configured and loaded. For OGC Tiles / MVT requests the raw pre-encoded tile bytes are passed through directly — zero decoding or re-encoding. For WMS/SVG requests the tile is decoded via OGR. Requires `source` (the OSM engine source name).

The table below lists OSMLayer-specific attributes (in addition to the common layer attributes).

<pre><b>OSMLayer</b></pre>
| Name             | Type              | Default      | Description |
| ---------------- | ----------------- | ------------ | ----------- |
| backend          | string            | `"postgis"`  | Data backend: `"postgis"` (via GIS engine) or `"pmtiles"` (via OSM engine mmap). |
| source           | string            | -            | **Required for pmtiles backend.** Source name as defined in the OSM engine configuration (e.g. `"scandinavia"`). |
| timestamp_file   | string            | -            | **PostGIS backend only.** Path to a file whose modification time represents the OSM data age. Touch after each `osm2pgsql` import to invalidate HTTP ETags. For the pmtiles backend the ETag is derived from the `.pmtiles` file's own modification time automatically. |
| precision        | double            | 1.0          | Coordinate precision for SVG path output (PostGIS backend only). |
| feature_sets     | _[FeatureSet]_    | -            | **Required for postgis backend.** Array of feature set definitions (see below). |

<pre><b>OSMLayer FeatureSet</b></pre>
| Name             | Type              | Default         | Description |
| ---------------- | ----------------- | --------------- | ----------- |
| pgname           | string            | -               | **Required.** PostGIS connection name as defined in the GIS engine configuration. |
| schema           | string            | -               | **Required.** Database schema (e.g. `osm_scandinavia`). |
| table            | string            | -               | **Required.** Table within the schema (e.g. `planet_osm_polygon`, `planet_osm_line`, `planet_osm_point`). |
| where            | string            | -               | Optional SQL WHERE clause to filter rows (e.g. `"natural = 'water'"`). |
| fieldnames       | [string]          | []              | OSM tag columns to fetch and include as MVT feature attributes (e.g. `["name","highway"]`). `"name"` is added automatically when `labels` is true. |
| lines            | boolean           | false           | `true` → apply line clipping (for road/river geometries); `false` → apply polygon clipping. |
| labels           | boolean           | false           | Render the `name` attribute as SVG `<text>` labels. Simple pixel-distance collision avoidance skips overlapping labels. In MVT output the `name` attribute is always included as a feature property for client-side label rendering. |
| minarea          | double            | -               | Minimum polygon area (m²) filter passed to the GIS engine. |
| mindistance      | double            | -               | Minimum distance simplification filter passed to the GIS engine. |
| mvt_layer_name   | string            | table name      | Name of the layer in MVT output. Defaults to the `table` value. |
| css              | string            | -               | CSS stylesheet file to load for this feature set. Combine with `attributes.class` to select which class from the file is applied to SVG elements. |
| attributes       | _Attributes_      | -               | SVG attributes for the `<g>` group containing this feature set. Use `"class"` here to name the CSS class from the stylesheet; other attributes (e.g. `"opacity"`) override or supplement the CSS. |

**OSM style themes**

Several ready-made CSS theme files are provided under `layers/osm/`. Each file defines CSS classes for common OSM feature types. Reference the same file in every feature set and set `attributes.class` to the appropriate feature class name.

Available themes (see file headers for full attribution):

| File | Based on | Character |
|---|---|---|
| `osm/osm-bright.css` | OpenMapTiles OSM Bright | Colorful, classic cartographic style with coloured road hierarchy |
| `osm/positron.css` | CartoDB Positron | Very light, minimal greyscale — designed as a neutral data backdrop |
| `osm/dark-matter.css` | CartoDB Dark Matter | Dark/night style — high contrast for coloured overlays on dark background |
| `osm/osm-liberty.css` | OSM Liberty | Free OSM Bright alternative; slightly cooler tones, same class names |

> **Note:** These CSS files define the visual style but cannot be tested until OSM data has been imported into PostGIS. Class names are identical across all themes so switching themes is a one-line change (edit the `css` field).

**CSS class reference**

All theme files define the following CSS classes:

| Class | OSM source | Geometry |
|---|---|---|
| `water` | `natural=water`, `waterway=*` | Polygon |
| `forest` | `landuse=forest`, `landuse=wood`, `natural=wood` | Polygon |
| `scrub` | `natural=scrub`, `natural=heath` | Polygon |
| `grass` | `landuse=grass`, `landuse=meadow`, `natural=grassland` | Polygon |
| `beach` | `natural=beach`, `natural=sand` | Polygon |
| `glacier` | `natural=glacier` | Polygon |
| `residential` | `landuse=residential` | Polygon |
| `commercial` | `landuse=commercial`, `landuse=retail` | Polygon |
| `industrial` | `landuse=industrial` | Polygon |
| `park` | `leisure=park`, `leisure=recreation_ground`, `leisure=nature_reserve` | Polygon |
| `hospital` | `amenity=hospital` | Polygon |
| `school` | `amenity=school`, `amenity=university`, `amenity=college` | Polygon |
| `airport` | `aeroway=aerodrome` | Polygon |
| `buildings` | `building=*` | Polygon |
| `motorway` | `highway=motorway` | Line |
| `trunk` | `highway=trunk` | Line |
| `primary` | `highway=primary` | Line |
| `secondary` | `highway=secondary` | Line |
| `tertiary` | `highway=tertiary` | Line |
| `road` | `highway IN (residential, unclassified, living_street)` | Line |
| `service_road` | `highway IN (service, track)` | Line |
| `footway` | `highway IN (footway, cycleway, path)` | Line |
| `railway` | `railway=rail` | Line |
| `railway_transit` | `railway IN (subway, light_rail, tram)` | Line |
| `coastline` | `natural=coastline` | Line |
| `admin_country` | `boundary=administrative AND admin_level=2` | Line |
| `admin_state` | `boundary=administrative AND admin_level IN (3,4)` | Line |
| `place_label` | `place IN (city, town, village, suburb)` | Point / label |
| `road_label` | Road labels | Line / label |

#### PostGISLayer

The PostGIS layer can be used  to create image layers based on data queried from the PostGIS database. 

The table below shows a simple example  on the usage of   the PostGIS layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
   "title": "Ice map",
   "projection":
   {
    "crs": "EPSG:2393",  // YKJ
    "bboxcrs": "WGS84",
    "xsize": 800, "ysize": 800, "resolution": 1.75,
    "cx": 19,
    "cy": 60
   },
   "views": [
  { // *** View 1
     "attributes": { "id": "view1"},
     "layers": [
    { // *** Layer 1: Ice map
       "layer_type": "postgis",
       "lines": false,
       "pgname": "icemap",
       "schema": "icemap",
       "table": "seaice",
       "time_column": "publicationdate",
       "time_truncate": "day",
       "geometry_column": "geom",
       "css": "ice/icemap.css",
       "filters": "json:ice/icemap.json",
       "attributes":  { "id": "iceareas"  }
    },
    { // *** Layer 2 : Country borders
       "layer_type": "map",
       "enable": "png",
       "map":
       {
        "lines": true,
        "schema": "natural_earth",
        "table": "europe_country_wgs84",
        "minarea": 20,
        "mindistance": 5
       },
       "attributes":
       {
        "id": "europe_country_line",
        "class": "Border"
       }
    } ]
  } ]
}</sub></code></pre></td><td><img src="images/postgis_ex.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for the PostGIS layer in addition to the common layer attributes.

<pre><b>PostGISLayer </b></pre>
| Name            | Type      | Default value | Description                                                                                                                                                                                                  |
| --------------- | --------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| lines           | boolean   | false         | Should the data be handled as if it would be stroked or filled. This will alter how the data will be clipped to the view - polygons will either be preserved for filling or cut into polylines for stroking. |
| pgname          | (string)  | -             | Identifier of PostGIS database. The GIS-engine configuration file defines connection info (host,database,username,password) for pgname.                                                                      |
| schema          | (string)  | -             | The database schema used in the database query.                                                                                                                                                              |
| table           | (string)  | -             | The database table used in the database query.                                                                                                                                                               |
| geometry_column | (string)  | -             | The name of the geometry column in the specified schema.table                                                                                                                                                |
| time_column     | string    | -             | The name of the time column in the specified schema.table.                                                                                                                                                   |
| time_truncate   | string    | -             |                                                                                                                                                                                                              |
| filters         | _Filters_ | -             |                                                                                                                                                                                                              |
| precision       | (double)  | 1.0           | Precision of printed SVG coordinates.                                                                                                                                                                        |



#### IceMapLayer

IceMapLayer inherits database-related functionality such as PostGISLayer definitions and filters from the PostGISLayer.

The table below shows a simple example  on the usage of  the PostGIS layer.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{ // *** Product
   "title": "Ice map",
   "projection":
   {
    "crs": "EPSG:2393",  // YKJ
    "bboxcrs": "WGS84",
    "xsize": 800, "ysize": 800, "resolution": 1.75,
    "cx": 19,
    "cy": 60
   },
   "views": [
  {
     "attributes": { "id": "view1"},
     "layers": [
    {
       "layer_type": "postgis",
       "pgname": "icemap",
       "schema": "icemap",
       "table": "seaice",
       "time_column": "publicationdate",
       "time_truncate": "day",
       "geometry_column": "geom",
       "css": "ice/icemap.css",
       "filters": "json:ice/icemap.json",
       "attributes":
       {
        "id": "iceareas"
       }
      },
      {
         "qid": "l3-",
         "enable": "png",
         "layer_type": "map",
         "map":
         {
          "lines": true,
          "schema": "natural_earth",
          "table": "europe_country_wgs84",
          "minarea": 20,
          "mindistance": 5
         },
         "attributes":
         {
          "id": "europe_country_line",
          "class": "Border"
         }
      }
     ]
  }
 ]
}</sub></code></pre></td><td><img src="images/icemap_ex.png"></td></tr>
</table>


The table below contains a list of attributes that can be defined for the ice map layer in addition to the common layer attributes.

<pre><b>IceMapLayer </b></pre>
<table>
<tr>
<th>Name </th>
<th>Type </th>
<th>Default Value </th>
<th colspan="2"> Description </th></tr>
<tr><td>attribute_columns</td><td>(string)</td><td>-</td><td colspan="2">Comma-separated list of columns in specified schema.table.</td></tr>
<tr><td>firstname_column</td><td>string</td><td>-</td><td colspan="2">Column name in specified schema.table. Primary name of the location, e.g. 'Helsinki'. This is used by 'named_location'-layer.</td></tr>
<tr><td>secondname_column</td><td>(string)</td><td>-</td><td colspan="2">Column name in specified schema.table.Secondary name of the location, e.g. 'Helsingfors'. This is used by 'named_location'-layer.</td></tr>
<tr><td>symbol</td><td>A symbol. Position of symbol is fetched from database</td><td></td><td colspan="2">The first parameter for PostGreSQL DATE_TRUNC-function. Supported values are 'minute', 'hour', 'day', 'week' ,'month', 'year'. If time_truncate is defined it is applied to requested datetime.</td></tr>
<tr><td rowspan="9">layer_subtype</td><td rowspan="9">(string)</td><td rowspan="9">"geometry"</td><td colspan="2">Specifies the actual layer type. Full list of supported types is listed in the table below. If layer_subtype is missing geometry type is assumed.</td></tr>
<tr><td><b>Name</b></td><td><b>Description</b></td></tr>
<tr><td>symbol</td><td>A symbol. Position of symbol is fetched from database.</td></tr>
<tr><td>named_location</td><td>Location with name. Position of location is fetched from database. symbol-property can be defined to mark the location on map. firstname_column, secondname_column can be defined to show location name on map.</td></tr>
<tr><td>coordinate_grid</td><td>Coordinate grid. Grid is drawn on requested geometry. If result set contains no geometry, grid is drawn on whole.</td></tr>
<tr><td>degree_of_pressure</td><td>Degree of pressure for ice.</td></tr>
<tr><td>mean_temperature</td><td>Ellipse type of label, where we have background ellipse and text written on .</td></tr>
<tr><td>label</td><td>Rectangle type of label, where we have a background rectangle and text written on it. Attributes for background rectangle and foreground text is defined in filters' 'attribute' and 'text_attribute' sections. '*'-character is a wildcard-character: The beginning of the name can be anything, as long as it ends with 'label'-string.</td></tr>
<tr><td>temperature_isotherm_label</td><td>Same as *label except that position (fetched from database) of label is modified a bit.</td></tr>
<tr><td>symbol</td><td>string</td><td>-</td><td colspan="2">Name of file containing the symbol-definition in SVG-format. This is needed only if value of layer_subtype-property is 'symbol'. The requested geometry determines the position of the symbol.</td></tr>
<tr><td>pattern</td><td>string</td><td>0.0</td><td colspan="2">Name of the file containing the pattern definition in SVG-format. The pattern is applied on the requested geometry, for example diagonal hatching on polygonal area.</td></tr>
<tr><td>precision</td><td>(double)</td><td>1.0</td><td colspan="2">Precision of printed SVG coordinates.</td></tr>
</table>

#### FinnishRoadObservationLayer

FinnishRoadObservationLayer can be used to show weather conditions at road weather stations all over Finland.

The table below shows a simple example on the usage of FinnishRoadObservationLayer layer. Note! synop-font must be used to show the weather symbols correctly.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{
    "title": "Road observations",
    "producer": "road",
    "timestep": 15,
    "language": "fi",
    "projection":
    {
    },
    "views": [
        {
	    "qid": "v1",
	    "attributes":
	    {
		"id": "view1"
	    },
            "layers": [
		{
		    "qid": "finland",
		    "layer_type": "map",
		    "map":
		    {
			"schema": "natural_earth",
			"table": "admin_0_countries",
			"where": "iso_a2 IN ('FI','AX')"
		    },
		    "attributes":
		    {
			"id": "finland_country",
			"fill": "rgb(255, 255, 204)"
		    }
		},
                {
                    "qid": "finland-roads",
                    "layer_type": "map",
                    "map": {
                        "schema": "natural_earth",
                        "table": "europe_roads_eureffin",
                        "where": "cntryname='Finland'",
                        "lines": true
                    },
                    "attributes": {
			"fill": "none",
			"stroke": "rgb(150,150,150)",
			"stroke-width": "0.6px"
                    }
                },
		{
		    "tag": "g",
		    "layers": [
			{
			    "qid": "l4",
			    "layer_type": "map",
			    "map":
			    {
				"schema": "natural_earth",
				"table": "admin_0_countries",
				"minarea": 100
			    },
			    "attributes":
			    {
				"id": "europe_country_lines",
				"fill": "none",
				"stroke": "#222",
				"stroke-width": "0.3px"
			    }
			},
			{
			    "qid": "road_weather_stations_observations",
			    "layer_type": "finnish_road_observation",
			    "keyword": "road_weather_stations_master",
			    "mindistance": 20,
			    "attributes":
			    {
				"font-family": "synop",
				"font-style": "normal",
				"font-weight": "normal",
				"font-size": 48
			    }
			}
		    ]
		}
            ]
        }
    ]
}
</sub></code></pre></td><td><img src="images/finnish_road_observations.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for finnish road observation layer.

<pre><b>FinnishRoadObservationLayer</b></pre>

| Name        | Type        | Default value | Description                                                                       |
| ----------- | ----------- | ------------- | --------------------------------------------------------------------------------- |
| positions   | _Positions_ | -             | The positions for the symbols.                                                    |
| label       | _Label_     | -             | Label definitions.                                                                |
| maxdistance | double      | 5             | Maximum distance for a station to be accepted close enough to the position.       |
| mindistance | int         | -             | Minimum distance in pixels between symbols.                                       |
| missing     | int         | 106           | Synop-font symbol for missing observations. Zero disables showing missing values. |


##### Algorithm to deduce weather condition symbol

Step 1)  Get the following parameters at requested timestep:

1) Mean air temperature (ILMA) of the observations from previous 24 hours
2) Precipitation type (SADE) of the nearest observation, maximum 20 minutes away
3) Precipitation form (RST) of the nearest observation, maximum 20 minutes away

Step 2) Get symbol for each station by using the following function, where 'r' is precipitation type and 'rform' is precipitation form:

<pre>if (r == 1)
    {
      if (rform == 9) return 51;         // drizzle, not freezing, intermittent, slight
      if (rform == 10) return 52;        // rain, not freezing, intermittent, slight
      if (rform == 11) return 53;        // intermittent fall of snowflakes, slight
      if (rform == 18) return 213;       // drizzle, freezing, slight
      if (rform == 19) return 223;       // rain, freezing, slight
    }
  if (r == 2)
    {
      if (rform == 9) return 209;        // drizzle, not freezing, intermittent, moderate
      if (rform == 10) return 219;       // rain, not freezing, intermittent, moderate
      if (rform == 11) return 229;       // intermittent fall of snowflakes, moderate
      if (rform == 18) return 214;       // drizzle, freezing, moderate or heavy
      if (rform == 19) return 224;       // rain, freezing, moderate or heavy
    }
  if (r == 3)
    {
      if (rform == 9) return 212;       // drizzle, not freezing, continous, heavy
      if (rform == 10) return 222;      // rain, not freezing, continous, heavy
      if (rform == 11) return 232;      // continous flall of snowflakes, heavy
      if (rform == 18) return 214;      // drizzle, freezing, moderate or heavy
      if (rform == 19) return 224;      // rain, freezing, moderate or heavy
    }
  if (rform == 13) return 225;          // rain or drizzle and snow, slight
  if (rform == 14) return 244;          // shower(s) of snow pellets or snow hail
  if (rform == 15) return 233;          // ice needles (with or without fog) 
  if (rform == 16) return 234;          // snow grains (with or without fog)
  if (rform == 17) return 236;          // ice pellets (sleet)

  // r 4,5,6 are redundant but re-check in case there is no match in the the cases above
  if (r == 4) return 53;        // intermittent fall of snowflakes, slight
  if (r == 5) return 229;       // intermittent fall of snowflakes, moderate
  if (r == 6) return 232;      // continous flall of snowflakes, heavy
</pre>

Step 3) Get priority for each station by using the following function, where symbol is the symbol of the station and t2m is mean temperature of the station:

<pre>
 if(t2m <= 0)
	{
	  // WINTER: t2m < 0
	  switch (symbol)
		{
		case 106:
		  return 0;
		  break;
		case 53:
		case 229:
		  return 1; 
		  break;
		case 232:
		case 233:
		case 234:
		case 236:
		case 244:
		  return 2; 
		  break;
		case 225:
		  return 5;
		  break;
		case 51:
		case 213:
		  return 6;
		  break;
		case 209:
		  return 7;
		  break;
		case 52:
		case 212:
		case 214:
		case 223:
		  return 8;
		  break;
		case 219:
		  return 9;
		  break;
		case 222:
		case 224:
		  return 10;
		  break;
		}

	}
  else if(t2m < 10)
	{
	  // SPRING OR AUTUMN: 0 > t2m < 10
	  switch (symbol)
		{
		case 106:
		  return 0;
		  break;
		case 51:
		case 52:
		case 53:
		case 213:
		case 223:
		case 225:
		  return 1;        // slight
		  break;
		case 209:
		case 219:
		case 229:
		case 233:
		case 234:
		case 236:
		case 244:
		  return 2;     // moderate
		  break;
		case 212:
		case 214:
		case 222:
		case 224:
		case 232:
		  return 4;      // heavy
		  break;
		}
	}
  else
	{
	  // SUMMER: t2m >= 10
	  switch (symbol)
		{
		case 51:
		case 52:
		case 106:
		  return 0;
		  break;
		case 209:
		case 219:
		  return 1;
		  break;
		case 212:
		case 222:
		  return 2;
		  break;
		case 213:
		  return 3;
		  break;
		case 214:
		  return 4;
		  break;
		case 225:
		  return 5;
		  break;
		case 53:
		case 223:
		  return 6;
		  break;
		case 224:
		case 229:
		  return 7;
		  break;
		case 232:
		  return 8;
		  break;
		case 233:
		case 234:
		case 236:
		case 244:
		  return 9;
		  break;
		}
	}
</pre> 

Step 4) Show stations on the map starting from the highest priority. If there already is a station nearby on the map (mindistance parameter), don't show the station.


#### PresentWeatherObservationLayer

PresentWeatherObservationLayer can be used to show present weather conditions at Automatic Weather Stations (AWS).

The table below shows a simple example on the usage of PresentWeatherObservationLayer layer. Note! synop-font must be used to show the weather symbols correctly.

<table>
<tr>
<th>Product configuration file </th>
<th> Produced image layer</th>
<tr><td rowspan="40">
<pre><code><sub>
{
    "title": "Present weather observations",
    "producer": "opendata",
    "timestep": 10,
    "language": "fi",
    "projection":
    {
    },
    "views": [
        {
	    "qid": "v1",
	    "attributes":
	    {
		"id": "view1"
	    },
            "layers": [
		{
		    "qid": "finland",
		    "layer_type": "map",
		    "map":
		    {
			"schema": "natural_earth",
			"table": "admin_0_countries",
			"where": "iso_a2 IN ('FI','AX')"
		    },
		    "attributes":
		    {
			"id": "finland_country",
			"fill": "rgb(255, 255, 204)"
		    }
		},
                {
                    "qid": "finland-roads",
                    "layer_type": "map",
                    "map": {
                        "schema": "natural_earth",
                        "table": "europe_roads_eureffin",
                        "where": "cntryname='Finland'",
                        "lines": true
                    },
                    "attributes": {
			"fill": "none",
			"stroke": "rgb(150,150,150)",
			"stroke-width": "0.6px"
                    }
                },
		{
		    "tag": "g",
		    "layers": [
			{
			    "qid": "l4",
			    "layer_type": "map",
			    "map":
			    {
				"schema": "natural_earth",
				"table": "admin_0_countries",
				"minarea": 100
			    },
			    "attributes":
			    {
				"id": "europe_country_lines",
				"fill": "none",
				"stroke": "#222",
				"stroke-width": "0.3px"
			    }
			},
			{
			    "qid": "wawa_observations",
			    "layer_type": "present_weather_observation",
			    "keyword": "synop_fi",
			    "paramater": "WW_AWS",
			    "mindistance": 20,
			    "attributes":
			    {
				"font-family": "synop",
				"font-style": "normal",
				"font-weight": "normal",
				"font-size": 48
			    }
			}
		    ]
		}
            ]
        }
    ]
}
</sub></code></pre></td><td><img src="images/present_weather_observations.png"></td></tr>
</table>

The table below contains a list of attributes that can be defined for present weather observation layer.

<pre><b>PresentWeatherObservationLayer</b></pre>

| Name        | Type        | Default value | Description                                                                       |
| ----------- | ----------- | ------------- | --------------------------------------------------------------------------------- |
| positions   | _Positions_ | -             | The positions for the symbols.                                                    |
| label       | _Label_     | -             | Label definitions.                                                                |
| maxdistance | double      | 5             | Maximum distance for a station to be accepted close enough to the position.       |
| mindistance | int         | -             | Minimum distance in pixels between symbols.                                       |
| missing     | int         | 106           | Synop-font symbol for missing observations. Zero disables showing missing values. |

##### Algorithm to deduce present weather symbol

Step 1) Get WaWa code (ww_aws) for the desired timestep

Step 2) Get symbol for each station by using the following get_symbol(float wawa-code) function:

<pre>
float convert_possible_wawa_2_ww(float theValue)
{
    static const float wwCodeArray[100] =   
{0, 1, 2, 3, 4, 5, 0, 0, 0, 0,
 10, 0,13, 0, 0, 0, 0, 0,18, 0,
 28,21,20,21,22,24,29, 0, 0, 0,
 42,41,43,45,47,48, 0, 0, 0, 0,
 61,63,65,61,65,71,75,66,67, 0,
 50,51,53,55,56,57,57,58,59, 0,
 60,61,63,65,66,67,67,68,69, 0,
 70,71,73,75,79,79,79,77,78, 0,
 80,80,81,81,82,85,86,86, 0,89,
 92,17,93,96,17,97,99, 0, 0, 8};

    if(theValue >= 100 && theValue <= 199)
        return wwCodeArray[static_cast<int>(theValue-100)];
    return theValue;
}

int get_symbol(float wawa)
{
  float value = convert_possible_wawa_2_ww(wawa);
  if(value != kFloatMissing && value > 3)
	{
	  if(value == 99)
		return 48; // synop fontin viimeinen arvo 255 osuu synop arvolle 98, onneksi myös 99 löytyy fontista, mutta sen sijainti on vain 48
	  else
		return (157 + value);
	}
  return 106; // Missing symbol 'Z'
}
</pre>

Step 3) Get priority for each station by using the following function:

<pre>
int get_symbol_priority(int symbol)
{
  switch (symbol)
	{
	case 106:
	  return 0;
	  break;
	case 170:
	case 171:
	case 172:
	case 173:
	  return 1;
	  break;
	case 162:
	case 163:
	case 164:
	case 165:
	case 167:
	case 168:
	case 169:
	case 174:
	case 177:
	case 178:
	case 179:
	case 180:
	case 181:
	case 182:
	case 183:
	case 184:
	case 185:
	case 186:
	case 198:
	case 207:
	case 208:
	case 213:
	case 215:
	case 217:
	case 218:
	case 227:
	case 235:
	case 242:
	case 244:
	case 246:
	case 248:
	case 249:
	case 250:
	case 251:
	  return 5;
	  break;
	case 187:
	case 188:
	case 189:
	case 193:
	case 195:
	case 197:
	case 199:
	case 200:
	case 209:
	case 210:
	case 219:
	case 220:
	case 225:
	case 228:
	case 229:
	case 233:
	case 234:
	case 236:
	case 237:
	case 240:
	  return 7;
	  break;
	case 161:
	case 175:
	case 211:
	case 216:
	case 221:
	case 223:
	case 230:
	case 231:
	case 238:
	case 252:
	case 253:
	  return 8;
	  break;
	case 201:
	case 202:
	case 203:
	case 204:
	case 205:
	case 206:
	case 212:
	case 239:
	case 241:
	case 243:
	case 245:
	case 247:
	  return 9;
	  break;
	case 166:
	case 176:
	case 190:
	case 191:
	case 192:
	case 194:
	case 196:
	case 214:
	case 222:
	case 224:
	case 226:
	case 232:
	case 254:
	case 255:
	case 48:
	  return 10;
	  break;
	}
  
  return 0;
}
</pre>
#### MetarLayer

MetarLayer renders METAR aviation weather observations as standard meteorological station plots, one plot per airport. It queries raw TAC METAR (or SPECI) messages from the AVI engine and decodes them directly, so no pre-processing pipeline is required.

Each station plot can include:
- **Wind barb** — direction and speed in knots with standard meteorological barb notation
- **Temperature / dew point** — displayed in METAR convention (e.g. `03` / `M02`)
- **Visibility** — in metres, capped at 9999
- **Present weather** — raw TAC tokens (e.g. `-RA`, `TSRA`, `+SN`)
- **Sky condition** — cloud layers as amount+base (e.g. `BKN025`) or `CAVOK`/`SKC`
- **QNH pressure** — in hPa
- **Wind gust** — in knots when reported
- **Aviation status box** — colour-coded LIFR / IFR / MVFR / VFR category

> **Requires:** The AVI engine (`engines/avi`) must be configured and loaded. MetarLayer is compiled out when `WITHOUT_AVI` is defined.

**Aviation flight rule categories**

The status box colour is derived from ceiling and visibility:

| Category | Colour | Ceiling | Visibility |
|---|---|---|---|
| LIFR (Low IFR) | magenta | < 500 ft | < 1600 m |
| IFR | red | 500–999 ft | 1600–4999 m |
| MVFR (Marginal VFR) | blue | 1000–2999 ft | 5000–7999 m |
| VFR | green | ≥ 3000 ft | ≥ 8000 m |

**Sample configuration**

```json
{
  "qid": "metar_obs",
  "layer_type": "metar",
  "message_type": "METAR",
  "exclude_specis": true,
  "font_size": 10,
  "plot_size": 60,
  "mindistance": 50,
  "show_temperature": true,
  "show_dewpoint": true,
  "show_visibility": true,
  "show_wind": true,
  "show_pressure": true,
  "show_gust": true,
  "show_weather": true,
  "show_sky_info": true,
  "show_status": true,
  "attributes": { "id": "metar_layer" }
}
```

To restrict the plot to specific airports or countries:

```json
{
  "layer_type": "metar",
  "icaos": ["EFHK", "ESSA", "EKCH"],
  "countries": ["FI", "SE", "NO", "DK"]
}
```

<pre><b>MetarLayer</b></pre>

| Name              | Type       | Default    | Description |
| ----------------- | ---------- | ---------- | ----------- |
| message_type      | string     | `"METAR"`  | AVI message type to query: `"METAR"`, `"SPECI"`, `"AWSMETAR"`, etc. |
| message_format    | string     | `"TAC"`    | Message format. Currently only `"TAC"` (raw METAR text) is supported. |
| exclude_specis    | boolean    | `true`     | Skip SPECI (special) reports when `message_type` is `"METAR"`. |
| font_size         | int        | `10`       | Base font size in pixels for all text elements in the station plot. |
| plot_size         | int        | `0`        | Bounding-box side length in pixels. `0` = auto (6 × `font_size`). |
| show_temperature  | boolean    | `true`     | Show temperature in the upper-left quadrant of the station plot. |
| show_dewpoint     | boolean    | `true`     | Show dew point below the temperature. |
| show_visibility   | boolean    | `true`     | Show visibility in metres in the lower-left quadrant. |
| show_wind         | boolean    | `true`     | Draw a wind barb at the station centre. |
| show_pressure     | boolean    | `true`     | Show QNH pressure (hPa) in the upper-right quadrant. |
| show_gust         | boolean    | `true`     | Show wind gust speed (kt) below the pressure when reported. |
| show_weather      | boolean    | `true`     | Show present-weather TAC tokens (e.g. `-RA`, `TSRA`) below the station symbol. |
| show_sky_info     | boolean    | `true`     | Show sky-condition string (`CAVOK`, `SKC`, or cloud layer list) in the lower-right quadrant. |
| show_status       | boolean    | `true`     | Draw a colour-coded aviation status box (LIFR/IFR/MVFR/VFR) at the station centre. |
| color             | string     | -          | Override all element colours with a single SVG colour value. When omitted each element uses its default colour. |
| mindistance       | int        | `0`        | Minimum pixel distance between rendered station plots. Plots closer than this threshold are suppressed (most-recent observation wins). |
| icaos             | [string]   | -          | Restrict rendering to the listed ICAO station codes. All stations are shown when omitted. |
| countries         | [string]   | -          | Restrict rendering to stations in the listed ISO 3166-1 alpha-2 country codes (e.g. `"FI"`, `"SE"`). All countries are shown when omitted. |

### Structure definitions 

The product, view and layer definitions contain several attributes of type  structure. This section  defines attributes used in these structures.  

#### Attribute structure 


The _Attributes_ structure is used  to define attributes for the current  Scalable Vector Graphics (SVG) element. This structure has quite many elements  such as Product, View,  Layer, etc. In this way we can define the drawing colors or the line widths that are used when the related SVG element is created. 

The _Attribute_ definitions are just plain mappings from strings to strings, i.e.. the attribute names and values are directly copied into the related XML element in the SVG file. 


Using an attribute name or a value that is not in the  the SVG specification may result in errors in the SVG file created.

The _Attributes_ structure can contain one or several attribute definitions. For example:

<pre><code>
	"attributes":
	{
   		"stroke": "black",
   		"stroke-width": "0.1"
	}
</code></pre>

If the attribute value is a number, it can be given without the quotation marks. For example:

<pre><code>
	"attributes":
	{
    		"stroke": "black",
    		"stroke-width": 0.1
	}
</code></pre>

However, it should be noted that the number will then also be printed as a number into the SVG properties. In this particular case it would be wiser to use the string form, since the number 0.1 is not exactly an IEEE-754 number, and we  actually get something a bit different in the output. Note that the  integers  in general are safe to be used without quotes.


#### AttributeSelection structure 

Sometimes there is a need to use different SVG attributes and symbols according to field values that are used for the image drawing. For example, if there is sunny weather in location X and rainy weather in location Y, we probably want to use different weather symbols and SVG attributes for these locations in our weather map. 

The _AttributeSelection_ structure encapsulates an _Attributes_ structure, a symbol and its  usage condition in the same structure. The usage condition defines when the symbol and the attributes can be used. There is usually one AttributeSelection structure available for each condition. For example, if we have a data field that can have five different values then we probably have the AttributeSelection structure for each of these values.

The table below contains a list of attributes that can be defined in the AttributeSelection structure.

<pre><b>AttributeSelection </b></pre>
| Name       | Type         | Default value | Description                                      |
| ---------- | ------------ | ------------- | ------------------------------------------------ |
| value      | (double)     | -             | The value to be matched.                         |
| lolimit    | (double)     | -             | The lower limit to be matched.                   |
| hilimit    | (double)     | -             | The upper limit to be matched.                   |
| symbol     | (string)     | -             | The symbol name to be associated with the match. |
| attributes | _Attributes_ | -             | The SVG attributes associated with the match.    |

<b>EXAMPLE: Selecting a precipitation symbol according to a value: </b>

<pre><code>
	[
       		 {
            		"value": 0,
            		"symbol": "drizzle"
        	},
        	{
            		"value": 1,
            		"symbol": "rain"
        	},
        	...
        	{
            		"value": 6,
            		"symbol": "hail"
        	}
	]

</code></pre>

<b>EXAMPLE: Selecting a weather symbols and attributes:</b>

<pre><code>
	[
        	{
            		"value": 1,
            		"symbol": "sunny",
            		"attributes":
            		{
             			"class": "Sunny"
            		}
      		},
     		{
            		"value": 2,
            		"symbol": "partly_cloudy",
            		"attributes":
            		{
             			"class": "PartlyCloudy"
            		}
      		},
       	 ...
	]


</code></pre>


<b>EXAMPLE: Selecting attributes according to a value range: </b>

<pre><code>
	[
        	{
            		"lolimit": 3,
            		"hilimit": 5,
            		"attributes":
            		{
             			"class": "ModerateWindArrow_3_5"
            		}
        	},
      		{
 	     		"lolimit": 5,
            		"hilimit": 7,
            		"attributes":
            		{
             			"class": "ModerateWindArrow_5_7"	
            		}
        	},
      		{
            		"lolimit": 7,
            		"hilimit": 10,
            		"attributes":
            		{
             			"class": "FreshWindArrow_7_10"
           		}
      		},
        	...
	]

</code></pre>

####  Projection structure

The Projection structure can be used to define a projection for a product, views or layers. The "projection" attribute is one of the shared attributes that is inherited from top to down in the product hierarchy. So, if the "projection" attribute is defined on the product level then all the views and the layers are using it, unless they do not override the value of this attribute.

The table below contains a list of attributes that can be defined in the Projection structure. 

<pre><b>Projection</b></pre>
| Name       | Type     | Default value | Description                                                                                                                                                                                                                        |
| ---------- | -------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| crs        | (string) | -             | The projection description as understood by GDAL, or "data" implying the projection of the selected producer.                                                                                                                      |
| bboxcrs    | (string) | -             | The projection description for defining the bounding box coordinates only. If not defined, the bounding box coordinates are expected to be in the same projection as the image.                                                    |
| xsize      | (int)    | -             | Width of the projection in pixels.                                                                                                                                                                                                 |
| ysize      | (int)    | -             | Height of the projection in pixels.                                                                                                                                                                                                |
| x1         | (double) | -             | Bottom left X-coordinate of the bounding box.                                                                                                                                                                                      |
| y1         | (double) | -             | Bottom left Y-coordinate of the bounding box.                                                                                                                                                                                      |
| x2         | (double) | -             | Top right X-coordinate of the bounding box.                                                                                                                                                                                        |
| y2         | (double) | -             | Top right Y-coordinate of the bounding box.                                                                                                                                                                                        |
| cx         | (double) | -             | Center coordinate of the bounding box.                                                                                                                                                                                             |
| cy         | (double) | -             | Center coordinate of the bounding box.                                                                                                                                                                                             |
| resolution | (double) | -             | The resolution of the image in kilometers. A resolution of 2.5 means one pixel is approximately 2.5 kilometers in the projection used. The description is not valid if the projection is geographic (latlon) instead of geometric. |

The bounding box of the projection can be defined in the following ways:

<b>1. Bounding box with corners</b>
A bounding box is defined using the  corner points and it should consist of "x1", "y1", "x2", "y2" and either "xsize" or "ysize". If both "xsize" and "ysize" are given, the aspect of the image may become distorted.

<b>2. Bounding box with a center point</b>
A bounding box is defined using a center coordinate and it should  consist of "cx", "cy", "resolution" and both "xsize" and "ysize".

<b>3. Bounding box from data</b>
If "crs" = "data"  is given and the a bounding box coordinates are not given, the bounding of the data will be used. The size still has to be defined separately.


<b>CRS definitions </b>

OGR/GDAL supports numerous ways to define a spatial reference. Currently OGRSpatialReference::SetFromUserInput supports the following methods:

1.	Well Known Text definition - passed on to importFromWkt().
2.	"EPSG:n" - number passed on to importFromEPSG().
3.	"EPSGA:n" - number passed on to importFromEPSGA().
4.	"AUTO:proj_id,unit_id,lon0,lat0" - WMS auto projections.
5.	"urn:ogc:def:crs:EPSG::n" - ogc urns
6.	PROJ.4 definitions - passed on to importFromProj4().
7.	Filename - file read for WKT, XML or PROJ.4 definition.
8.	Well known name accepted by SetWellKnownGeogCS(), such as NAD27, NAD83, WGS84 or WGS72.
9.	WKT (directly or in a file) in ESRI format should be prefixed with ESRI:: to trigger an automatic morphFromESRI().
10.	"IGNF:xxx" - "+init=IGNF:xxx" passed on to importFromProj4().


#### Defs structure 

The _Defs_ structure is used  to define header information such as styles, symbols, paths not to be drawn directly, etc.  for the generated SVG image. 

The table below contains a list of attributes used in this structure.

<pre><b>Defs</b></pre>
| Name   | Type                   | Default value | Description                                                                                                                                                                                                                                                                                                                                                          |
| ------ | ---------------------- | ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| qid    | (string)               | -             | An identifier for the definitions section. Usually this can be left empty, unless the query string is to be used, or some times when path data is generated into the definitions section to avoid conflicting names with the paths in the body section.                                                                                                              |
| styles | {string:string:string} | -             | A map from CSS class names such as ".Temperature" to a map of attribute name-value pairs. This is mostly useful when testing different styles before exporting the settings to a CSS file, or if the style is so particular to the current product that it is not desirable to put it into a shared external file.                                                   |
| layers | _[Layer]_              | -             | An array of Layer structures which generate content to the header only. Many layers change their behavior when defined in the definitions section. For example, normally the IsobandLayer module would generate both path data into the definitions section and a respective use-tag into the body section. In the definitions section no use-tag will be generated. |


#### Smoother structure

Currently only the  2D Savitzky-Golay filters are supported. The filter is good at preserving local minima and maxima  if at least the degree 2 is used. Larger sizes with degree 2 tend to smooth data well while preserving extrema, higher degrees preserve original data better.

The table below contains a list of attributes used in this structure.

<pre><b>Smoother</b></pre>
| Name   | Type | Default value | Description                                                                      |
| ------ | ---- | ------------- | -------------------------------------------------------------------------------- |
| size   | int  | -             | Size of the filter. Implies 2*N+1 adjacent points are used in the weighted mean. |
| degree | int  | -             | Degree of the polynomial to fit to the data.                                     |


### Sampling structure

Data to be contoured may be interpolated to another resolution. When used for temperature and topographic data is available, this may be used to provide higher apparent resolution by applying a lapse rate correction to the temperature. The desired resolution may be absolute or relative to the image resolution.

| Name               | Type   | Default value | Description                                                                         |
| ------------------ | ------ | ------------- | ----------------------------------------------------------------------------------- |
| resolution         | double | -             | Desired resolution in kilometers per pixel                                          |
| relativeresolution | double | -             | Desired relative resolution with respect to the resolution of the image resolution. |
| minresolution      | double | -             | Minimum resolution for applying sampling                                            |
| maxresolution      | double | -             | Maximum resolution for applying sampling                                            |

#### Heatmap structure

Heatmap settings for "flash" producer's data. Currently parameter's (e.g. peak power) values are not used when building heatmap (they could be used for weighting). Heatmap library (https://github.com/lucasb-eyer/heatmap) supports use of custom kernels for stamp generation. Currently three kernels are implemented.

The table below contains a list of attributes used in this structure.

<pre><b>Heatmap</b></pre>
| Name       | Type     | Default value | Description                                                 |
| ---------- | -------- | ------------- | ----------------------------------------------------------- |
| resolution | int      | -             | Heatmap resolution.                                         |
| radius     | int      | -             | Stamp radius.                                               |
| kernel     | (string) | exp           | Stamp generation: linear, sqrt or exp (standard deviation). |
| deviation  | (double) | 10.0          | Deviation for exp.                                          |


#### Isoband structure

The table below contains a list of attributes used in the Isoband structure.

<pre><b>Isoband</b></pre>
| Name       | Type         | Default value | Description                                                                                                        |
| ---------- | ------------ | ------------- | ------------------------------------------------------------------------------------------------------------------ |
| qid        | string       | -             | The identifier for the isoband.                                                                                    |
| lolimit    | (double)     | -             | The low limit value for the isoband. If omitted, -infinity is implied.                                             |
| hilimit    | (double)     | -             | The high limit value for the isoband. If omitted, +infinity is implied.                                            |
| value      | (double)     | -             | Set both low and high limits for the isoband. Usually used for discrete data with nearest neighbour interpolation. |
| attributes | _Attributes_ | -             | The SVG attributes for the isoband. Usually only a class is given.                                                 |
| label      | (_Text_)     | -             | Translatable label for the isoband.                                                                                |

If both lolimit and hilimit are omitted, the missing values isoband will be generated.

#### Intersection structure

The table below contains a list of attributes used in the Intersection structure.

<pre><b>Intersection</b></pre>
| Name            | Type       | Default value | Description                                                                               |
| --------------- | ---------- | ------------- | ----------------------------------------------------------------------------------------- |
| lolimit         | (double)   | -             | The low limit value for the intersecting isoband. If omitted, -infinity is implied.       |
| hilimit         | (double)   | -             | The high limit value for the intersecting isoband. If omitted, +infinity is implied.      |
| level           | (double)   | -             | The level from which to pick the intersecting isoband.                                    |
| producer        | (string)   | -             | The model from which to take the isoband, if not the same model as in the isoband itself. |
| parameter       | string     | -             | The parameter whose isoband is used to intersect the actual isoband.                      |
| smoother        | (Smoother) | -             | How to smoothen the intersecting isoband.                                                 |
| unit_conversion | (string)   | -             | Name of desired unit conversion. Unit conversions are listed in the configuration file.   |
| multiplier      | (double)   | 1.0           | A multiplier for valid data values for unit conversion purposes.                          |
| offset          | (double)   | 0.0           | An offset for valid data for unit conversion purposes.                                    |

Note: Intersections work also for observation data. Instead of logical geometry operations the values are merely compared to the limits. For example, one may choose to render observed wind speed only when temperature is below zero.

#### Isoline structure

The table below contains a list of attributes used in the Isoline structure.

<pre><b>Isoline</b></pre>
| Name                                                         | Type         | Default value | Description                                                                                             |
| ------------------------------------------------------------ | ------------ | ------------- | ------------------------------------------------------------------------------------------------------- |
| qid                                                          | string       | -             | The identifier for the isoline.                                                                         |
| value                                                        | double       | -             | The isoline value. The default is NaN, which means calculating the line between missing and valid data. |
| attributes                                                   | _Attributes_ | -             | The SVG attributes for the isoline. Often omitted completely, and the _IsolineLayer_                    | label | (_Text_) | - | Translatable label for the isoline. |
| level attributes are used instead commonly for all isolines. |

#### Isofilter structure

Isolines and isobands can be smoothened by postprocessing the calculated polygons with a low pass filter.

<pre><b>Isofilter</b></pre>
| Name       | Type    | Default value | Description                                                                                                |
| ---------- | ------- | ------------- | ---------------------------------------------------------------------------------------------------------- |
| type       | string  | none          | Smoother type: none, average, linear, gaussian, tukey. Gaussian filtering seems to work best.              |
| radius     | double  | 0             | Filtering distance along the isoline in pixels. Zero disables filtering. Depending on the roughness of the data good values tend to be in the range 10-30 pixels. |
| iterations | integer | 1             | Number of passes. Zero disables filtering. Using 2-3 passes tends to remove small details better than simply increasing the radius. |

Note that zooming into an image reduces the amount of smoothing since the set radius now covers a smaller area of the original data, and hence original details can be seen better.

#### LegendLabels structure

The table below contains a list of attributes used in the LegendLabels structure.

<pre><b>LegendLabels</b></pre>
<table>
<tr>
<th>Name </th>
<th>Type </th>
<th>Default Value </th>
<th colspan="2"> Description </th></tr>
<tr><td rowspan="6">type</td><td rowspan="6">(string)</td><td rowspan="6">"range"</td><td colspan="2">Legend label type.</td></tr>
<tr><td><b>Value</b></td><td><b>Description</b></td></tr>
<tr><td>none</td><td>No labels will be generated.</td></tr>
<tr><td>lolimit</td><td>The lower limit of the isoband will be used, unless the lolimit is missing.</td></tr>
<tr><td>hilimit</td><td>The upper limit of the isoband will be used, unless the hilimit is missing.</td></tr>
<tr><td>range</td><td>A range of values will be used. If lolimit is missing, "&lt; hilimit" is used. If hilimit is missing, "&gt; lolimit" is used. If both limits are missing, the text "MISSING" is returned. Otherwise a label of the form "lolimit...hilimit" is used where the string separating the limits is configurable separately.</td></tr>
<tr><td>format</td><td>(string)</td><td>-</td><td colspan="2">printf-type format for the labels. Normally not needed, but if decimals are involved it may be best to define a format such as "%.1f" since not all numbers (such as 0.1) can be represented exactly as IEEE-754 numbers.</td></tr>
<tr><td>dx</td><td>int</td><td>30</td><td colspan="2">X-coordinate offset from the respective legend symbol.</td></tr>
<tr><td>dy</td><td>int</td><td>30</td><td colspan="2">Y-coordinate offset from the respective legend symbol. Depending on the type of the legend, one may often have to define this value to be the height of the respective symbol plus possibly half of the used font size.</td></tr>
<tr><td>separator</td><td>(string)</td><td>"\u2013" (endash)</td><td colspan="2">The string separating the isoband lolimit and hilimit when type=range is used.</td></tr>
<tr><td>conversions</td><td>{string:string}</td><td>-</td><td colspan="2">Conversions from automatically generated labels to alternate strings. Typically one may wish to change the values at the bottom and at the top of the legend to strings such as "&lt; lolimit" or "&gt; hilimit". The value may also be a map from language codes to translations.</td></tr>
<tr><td>attributes</td><td><i>Attributes</i></td><td>-</td><td colspan="2">The SVG attributes for the labels.</td></tr>
</table>


#### LegendSymbols structure

The table below contains a list of attributes used in the LegendSymbols structure.

<pre><b>LegendSymbols</b></pre>
| Name       | Type       | Default value | Description                                                                                                                                                                                                |
| ---------- | ---------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| css        | (string)   | -             | External CSS file to be included for the symbols.                                                                                                                                                          |
| symbol     | (string)   | -             | The default symbol for the legend.                                                                                                                                                                         |
| start      | (string)   | -             | The optional symbol to be used for the start of the legend.                                                                                                                                                |
| end        | (string)   | -             | The optional symbol to be used for the end of the legend.                                                                                                                                                  |
| missing    | (string)   | -             | The optional symbol to be used for missing values. If not defined, no symbol will be generated for the isoband nor the respective label, meaning no translation will be necessary for the "MISSING" label. |
| attributes | Attributes | -             | The extra SVG attributes for the symbol beyond the ones defined in the isoband.                                                                                                                            |

#### Text structure

The text structure is used to define translatable strings with optional styling attributes. The structure may for example be used to define fixed labels for isobands.

<pre><b>Text</b></pre>
| Name         | Type       | Default value | Description                                  |
| ------------ | ---------- | ------------- | -------------------------------------------- |
| (any string) | string     | -             | Translation for specific language            |
| default      | (string)   | -             | The default translation                      |
| attributes   | Attributes | -             | The extra SVG attributes for the translation |

#### Map structure

The table below contains a list of attributes used in the Map structure.


<pre><b>Map</b></pre>
| Name        | Type    | Default value | Description                                                                                                                                                                                                  |
| ----------- | ------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| schema      | string  | ""            | The database schema.                                                                                                                                                                                         |
| table       | string  | ""            | The database table.                                                                                                                                                                                          |
| where       | string  | ""            | The optional where clause for the PostGIS query. For example "cntryname='Finland'"                                                                                                                           |
| lines       | boolean | false         | Should the data be handled as if it would be stroked or filled. This will alter how the data will be clipped to the view - polygons will either be preserved for filling or cut into polylines for stroking. |
| minarea     | double  | -             | Minimum area for polygons in square kilometers. For Finland useful values are around 10-100.                                                                                                                 |
| mindistance | double  | -             | Feature generalization tolerance in kilometers. For Finland useful values are around 1-4.                                                                                                                    |

#### MapStyles structure

Individual map features can be styled based on a forecast value assigned for the area. For example, one can colour regions based on the forecast warning level for the area.

The table below contains a list of attributes used in the MapStyles structure.

<pre><b>MapStyles</b></pre>
| Name       | Type               | Default value | Description                                                                                              |
| ---------- | ------------------ | ------------- | -------------------------------------------------------------------------------------------------------- |
| field      | string             | ""            | The database field used to identify the region (region number or name).                                  |
| parameter  | string             | ""            | The forecast parameter name.                                                                             |
| features   | [MapFeature]       | -             | Optional mapping from field names to station numbers. If omitted, database field must be a region number |
| attributes | AttributeSelection | -             | Mapping from forecast value to presentation attributes                                                   |

Note: Optional FeatureMapping attributes override common MapLayer attributes, and attributes based on the actual data value override both.

#### MapFeature structure

Feature mappings are used to make database shape names to region numbers to be picked from forecast data. Feature mappings are not needed if the database contains region numbers directly. However, feature mappings can also be used to style individual regions.

The table below contains a list of attributes used in the MapFeature structure.

<pre><b>MapFeature</b></pre>
| Name       | Type         | Default value | Description                                                                                       |
| ---------- | ------------ | ------------- | ------------------------------------------------------------------------------------------------- |
| value      | string       | -             | The value of the database field to be mapped to a region number, typically the name of the region |
| number     | int          | -             | The region number to be picked from the forecast.                                                 |
| attributes | [Attributes] | -             | Region specific presentation attributes                                                           |

Note: Optional FeatureMapping attributes override common MapLayer attributes, and attributes based on the actual data value override both.

#### Positions structure 

The Positions structure can be used  to define a list of positions which are needed in quite many layers. For example, in the NumberLayer this structure defines positions for the printed number, in the SymbolLayer it defines positions for the symbols and in the ArrowLayer it defines positions for the arrows. 

The table below contains a list of attributes used in this structure.

<pre><b>Positions</b></pre>
<table>
<tr>
<th>Name </th>
<th>Type </th>
<th>Default Value </th>
<th colspan="2"> Description </th></tr>
<tr><td rowspan="8">layout</td><td rowspan="8">(string)</td><td rowspan="8">grid</td><td colspan="2">Layout type. If can have one of the following values:</td></tr>
<tr><td><b>Value</b></td><td><b>Description</b></td></tr>
<tr><td>grid</td><td>This layout places symbols in a fixed step pixel grid. Sometimes not the best choice for arrow-layers, experts prefer graticule-layouts.</td></tr>
<tr><td>data</td><td>This layout places symbols at the coordinates defined by the used querydata or the stations themselves if observations are used.</td></tr>
<tr><td>keyword</td><td>This layout places symbols at the coordinates of the locations associated with the given database keyword.</td></tr>
<tr><td>station</td><td>This layout places symbols at the stations listed by the user.</td></tr>
<tr><td>latlon</td><td>This layout places symbols at the locations listed by the user.</td></tr>
<tr><td>graticule</td><td>This layout places symbols along the edges of a NxN graticule grid with a fixed step size in degrees.</td></tr>
<tr><td>graticulefill</td><td>This layout places symbols along the edges of a NxN graticule grid plus inside the graticule cell while satisfying the set minimum distance condition. This layout is recommended for arrow-layers.</td></tr>
<tr><td>xmargin</td><td>uint</td><td>0</td><td colspan="2">Extra margin for coordinate generation</td></tr>
<tr><td>ymargin</td><td>uint</td><td>0</td><td colspan="2">Extra margin for coordinate generation</td></tr>
<tr><td>x</td><td>uint</td><td>5</td><td colspan="2">Grid layout: X-coordinate of the first number</td></tr>
<tr><td>y</td><td>uint</td><td>5</td><td colspan="2">Grid layout: Y-coordinate of the first number</td></tr>
<tr><td>dx</td><td>uint</td><td>20</td><td colspan="2">Grid layout: X-coordinate offset to the next number on the same row</td></tr>
<tr><td></td><td>uint</td><td>-</td><td colspan="2">Other layouts: X-coordinate adjustment for the selected position</td></tr>
<tr><td>dy</td></td>uint</td><td>20<td><td colspan="2">Grid layout: Y-coordinate offset to the next number on the next row</td></tr>
<tr><td></td><td>uint</td><td>-</td><td colspan="2">Other layouts: Y-coordinate adjustment for the selected position</td></tr>
<tr><td>ddx</td><td>uint</td><td>0</td><td colspan="2">Grid layout: X-coordinate adjustment for the next row in grid layout</td></tr>
<tr><td>size</td><td>uint</td><td>10</td><td colspan="2">Graticule/GraticuleFill layout: size of the graticule cell in degrees</td></tr>
<tr><td>step</td><td>uint>/td><td>1</td><td colspan="2">Graticule layout: step size for the graticule in degrees</td></tr>
<tr><td>mindistance</td><td>double</td><td>50</td><td colspan="2">GraticuleFill layout: minimum distance between parallel and meridian symbols in pixels</td></tr>
<tr><td>keyword</td><td>string</td><td>-</td><td colspan="2">Keyword layout: the database keyword</td></tr>
<tr><td>stations</td><td>XXXXX</td><td>-</td><td colspan="2">Station layout: the user listed stations</td></tr>
<tr><td>locations</td><td>XXXXX</td><td>-</td><td colspan="2">LatLon layout: the user listed locations</td></tr>
<tr><td>direction</td><td>string</td><td>-</td><td colspan="2">Direction parameter. Specify a direction parameter or both "u" and "v" attributes</td></tr>
<tr><td>u</td><td>string</td><td>-</td><td colspan="2">U-component of the direction parameter</td></tr>
<tr><td>v</td><td>string</td><td>-</td><td colspan="2">V-component of the direction parameter</td></tr>
<tr><td>directionoffset</td><td>int</td><td>0</td><td colspan="2">Extra displacement in the direction of the specified parameter</td></tr>
<tr><td>rotate</td><td>int</td><td>0</td><td colspan="2">Extra rotational displacement</td></tr>
<tr><td>outside</td><td><i>Map</i></td><td>-</td><td colspan="2">Only coordinates outside this shape will generate output</td></tr>
<tr><td>inside</td><td><i>Map</i></td><td>-</td><td colspan="2">Only coordinates inside this shape will generate output</td></tr>
<tr><td>intersect</td><td><i>[Intersection]</i> or <i>Intersection</i></td><td>-</td><td colspan="2">Isobands in which the positions must be</td></tr>
</table>

Note: The direction is positive for wind. For currents one may use "rotate"=90 or a negative "directionoffset". If an extra rotation is desired, it is recommended to use a negative offset.

If the layout is not "data" and the producer refers to observations, the generated candidate locations will be snapped to the nearest stations if the "maxdistance" condition set for the layer is satisfied. This way one may for example place candidate points into a grid and get an evenly distributed sample of the full set of stations.

The margins can be used to generate point locations outside the image area. This is useful for example if the symbols to be placed at the locations can be expected to overflow to adjacent WMS tiles. xmargin and ymargin can be set simultaneously using the "margin" setting.

#### Label structure 

The table below contains a list of attributes used in the Label structure.


<pre><b>Label</b></pre>
| Name            | Type     | Default value | Description                                                                                     |
| --------------- | -------- | ------------- | ----------------------------------------------------------------------------------------------- |
| dx              | int      | 0             | X-coordinate offset of the number from the actual coordinate.                                   |
| dy              | int      | 0             | Y-coordinate offset of the number from the actual coordinate.                                   |
| unit_conversion | (string) | -             | Name of desired unit conversion. Unit conversions are listed in the configuration file.         |
| multiplier      | double   | 1             | Multiplier for the number for unit conversion purposes.                                         |
| offset          | double   | 0             | Offset for the number for unit conversion purposes.                                             |
| missing         | string   | "-"           | Label for missing values. No text is output if the value is empty.                              |
| precision       | int      | 0             | Number of decimals.                                                                             |
| multiple        | double   | 0             | If nonzero, round value to multiple of set value                                                |
| rounding        | string   | "tonearest"   | Rounding mode: tonearest, towardzero, upward or downward.                                       |
| locale          | string   | -             | Locale for printing the number, for example fi_FI.                                              |
| prefix          | string   | ""            | Prefix for the number.                                                                          |
| suffix          | string   | ""            | Suffix for the number, usually for units.                                                       |
| plusprefix      | string   | ""            | Prefix replacing the sign for non-negative numbers.                                             |
| minusprefix     | string   | ""            | Prefix replacing the sign for negative numbers.                                                 |
| orientation     | string   | "horizontal"  | Horizontal or auto. Currently used only by IsolabelLayer                                        |
| padding_char    | string   | -             | Padding character for numbers.                                                                  |
| padding_length  | int      | -             | Padding length for numbers. Padding is done if length of number is shorter than padding_length. |


Note: signed prefixes are needed in aviation charts where negative temperatures are typically shown without a sign. In the exceptional case where positive numbers are actually needed, they are displayed using a plus sign or a "PS"-prefix.

#### Location structure

The table below contains a list of attributes used in the Location structure.

<pre><b>Location</b></pre>
| Name      | Type   | Default value | Description                               |
| --------- | ------ | ------------- | ----------------------------------------- |
| longitude | double | -             | The longitude of the coordinate.          |
| latitude  | double | -             | The latitude of the coordinate.           |
| dx        | int    | -             | X-coordinate adjustment for the location. |
| dy        | int    | -             | Y-coordinate adjustment for the location. |


#### Observation structure

The table below contains a list of attributes used in the Observation structure.

<pre><b>Observation</b></pre>
| Name       | Type         | Default value | Description                                      |
| ---------- | ------------ | ------------- | ------------------------------------------------ |
| parameter  | string       | -             | The parameter name of the observation.           |
| label      | _Label_      | -             | How to render the observed value.                |
| attributes | _Attributes_ | -             | SVG rendering attributes for the generated text. |




#### Station structure

The table below contains a list of attributes used in the Station structure.

<pre><b>Station</b></pre>
| Name       | Type       | Default value | Description                                                                                                    |
| ---------- | ---------- | ------------- | -------------------------------------------------------------------------------------------------------------- |
| fmisid     | (int)      | -             | fmisid of the station.                                                                                         |
| wmo        | (int)      | -             | wmo-number of the station.                                                                                     |
| lpnn       | (int)      | -             | LPNN-number of the station (Finland only)                                                                      |
| geoid      | (int)      | -             | geonames database identifier for the station                                                                   |
| longitude  | (double)   | -             | The longitude for the location of the wind rose. Default is to place the wind rose at the station coordinates. |
| latitude   | (double)   | -             | The latitude for the location of the wind rose. Default is to place the wind rose at the station coordinates.  |
| symbol     | (string)   | -             | An optional symbol to be placed at the station.                                                                |
| attributes | Attributes | -             | The SVG rendering attributes for the symbol                                                                    |
| title      | Title      | -             | The title for the wind rose.                                                                                   |
| dx         | (int)      | -             | Station specific positional adjustment for the symbol attached to the station.                                 |
| dy         | (int)      | -             | Station specific positional adjustment for the symbol attached to the station.                                 |

Note that both longitude and latitude have to be specified in order to alter the location where a number or something else will be displayed instead of the station coordinates.


#### WindRose structure

The table below contains a list of attributes used in the WindRose structure.

<pre><b>WindRose</b></pre>
| Name          | Type                 | Default value | Description                                                                                                                        |
| ------------- | -------------------- | ------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| minpercentage | int                  | 0             | Required percentage of observations for a sector to be shown. This can be used to prevent very small sectors which are irrelevant. |
| radius        | int                  | 20            | The radius of the wind rose.                                                                                                       |
| sectors       | int                  | 8             | The number of wind direction sectors in the wind rose                                                                              |
| symbol        | (string)             | -             | The symbol for the wind rose, usually either a circle or omitted.                                                                  |
| attributes    | _Attributes_         | -             | The SVG attributes for the symbol.                                                                                                 |
| connector     | _(Connector)_        | -             | How to connect the wind rose to the location of the station.                                                                       |
| parameter     | (string)             | -             | The wind direction parameter name.                                                                                                 |
| limits        | _AttributeSelection_ | -             | SVG styling based on the average wind speed in the sector.                                                                         |


#### Connector structure

The table below contains a list of attributes used in the Connector structure.

<pre><b>Connector</b></pre>
| Name        | Type         | Default value | Description                                                                                   |
| ----------- | ------------ | ------------- | --------------------------------------------------------------------------------------------- |
| startoffset | int          | 0             | Start offset for the connecting line. Usually zero, unless a symbol is placed at the station. |
| endoffset   | int          | 0             | End offset for the connecting line. Usually matches the radius of the wind rose.              |
| attributes  | _Attributes_ | -             | The SVG rendering attributes for the connecting line.                                         |



#### Filters structure

The table below contains a list of attributes used in the Filter structure.

<pre><b>Filters</b></pre>
| Name            | Type         | Default value | Description                                          |
| --------------- | ------------ | ------------- | ---------------------------------------------------- |
| where           | (string)     |               | WHERE condition to be appended to the database query |
| attributes      | _Attributes_ |               | SVG-attributes for geometry                          |
| text_attributes | _Attributes_ |               | SVG-attributes for text                              |


# Using dynamically created grids

### Introduction

A layer usually contains the "parameter" attribute definition, which refers to the grid data that is needed in order to draw the current layer. Typically the value of the "parameter" attribute is a parameter name that points to   fixed grid data, which means that this data is pre-calculated. In this case the biggest drawback is that if we want to get grids that are calculated differently then we need a pre-calculation process that calculates these grids and stores them in such way that they can be accessed. 

Luckily now there is a way to caculate new grids on the fly and use their data as they were fixed grids. The basic idea is that the "parameter" attibute can contain a function that defines how the new grid should be calculated. This feature is available only for producers that use grid-engine. 

There are two different ways to create new grids:

1) We can create new grids that uses data from other grids that have the same timestamp. 
2) We can create grids that uses data from multiple timesteps. For example, calculating max temperature values for a day.

### Grids with the same timestamp

<b>Here are some examples that calculate a new grid from other grids that have the same timestamp:</b>

The new grid contains maximum values from 3 ensemble grids (1,3,5):
<pre>
    "parameter" : "MAX{Temperature:MEPS:1093:6:0:3:1;Temperature:MEPS:1093:6:0:3:3;Temperature:MEPS:1093:6:0:3:5}"
</pre>

The new grid contains average values counted from 14 ensemble grids (1-14):
<pre>
    "parameter" : "AVG{Temperature:MEPS:1093:6:0:3:1-14}"
</pre>

The new grid contains probability percentage that the values in 14 ensemble grids (1-14) are below zero (= in range -1000 .. 0):
<pre>
   "parameter": "IN_PRCNT{-1000;0;Temperature:MEPS:1093:6:0:3:1-14}"
</pre>
	
### Grids calculated over multiple timesteps 

The basic idea is that, we can have multiple timesteps before and after the requested time. The timestep size (in minutes) can be used in order to calculate timestamp for the requested data. The requested data is expressed in timestep index range (not actual time lengths). The index number 0 refers to the requested time, the first timestep before the requested time is refered with the index number -1, and the first timestep after the requested time is refered with the index number 1. If functions require additional parameters (like value range), these parameters are added after the timestep range and timestep size defintions.

<b>Here are some examples:</b>

The new grid contains maximum values from the previous 24 timesteps.Timestep size = 60 minutes, range of grid indexes = [-23 .. 0] counted from the requested time.
<pre>
    "parameter" : "/MAX{Temperature;-23;0;60}"
</pre>
	
The new grid contains probability percentage that the temperature drops below the zero (= is in range -1000..0) during the following 48 hours.
<pre>
    "parameter": "/IN_PRCNT{Temperature;0;47;60;-1000;0}"
</pre>


### Functions

Dynamic grid caculation uses C++ or LUA functions for creating new grids. C++ functions are faster than LUA functions and that's why we try to implement most common functions with C++. At least the following functions are currently available for producing new grids:
<pre>
    AVG		=> Calculate average/mean values.
    MAX		=> Calculate maximum values.
    MIN		=> Calculate minimum values.
    MUL		=> Multiplies grid values with a multiplyer. This can be used also for dividing grid values (i.e. multiplier = 1/divider).
    SUM		=> Add 1-N grids together. If there are additional numbers in the function then these numbers are added to each grid value.
    IN_PRCNT	=> Returns the probability percent that the value in the given grids is inside the given value range.
    OUT_PRCNT	=> Returns the probability percent that the value in the given grids is outside the given value range.
</pre>


# JSON references and includes

Product JSON files support four mechanisms for composing and reusing configuration fragments.

## `json:` value references

Any JSON string value that starts with `json:` causes the corresponding file to be loaded and
the string replaced with the parsed JSON content.  Paths are resolved relative to the customer
`layers/` directory (e.g. `<root>/customers/<customer>/layers/`), or if the path starts with
`/`, relative to the Dali root directory.

```json
{
    "isobands": "json:isobands/temperature.json",
    "label":    "json:numbers/integers.json"
}
```

When a `json:` value appears as an element of an array, the loaded JSON replaces that element
(useful for including complete layer definitions):

```json
"layers": [
    "json:paths/legendmask.json",
    { "layer_type": "map", "..." }
]
```

## `ref:` cross-references

Any string value of the form `ref:<dotted.path>` is replaced with the value found by following
the dotted path in the current JSON document.  The top-level `refs` object is a conventional
place to collect reusable fragments:

```json
{
    "refs": {
        "finland": {
            "schema": "natural_earth",
            "table":  "admin_0_countries",
            "where":  "iso_a2 IN ('FI','AX')"
        },
        "myprojection": "json:maps/finlandprojection.json"
    },
    "projection": "ref:refs.myprojection",
    "views": [{
        "layers": [{
            "layer_type": "isoband",
            "inside": "ref:refs.finland"
        }]
    }]
}
```

## JSON Pointer (`$ref`) dereferencing

Standard JSON Pointer references of the form `{"$ref": "#/path/to/value"}` are resolved after
`json:` and `ref:` substitution.  This follows the JSON Reference / JSON Schema convention.

## Processing pipeline

The product JSON goes through the following stages, which can be inspected individually
by adding `stage=<n>` to the request:

| Stage | `stage=` value | Operation |
|-------|---------------|-----------|
| Raw JSON | `1` | File loaded as-is, no substitution |
| Reference substitution | `2` | Query-string `json:` and `ref:` replacements applied |
| Include expansion | `3` | `json:` and `ref:` values resolved from files |
| JSON Pointer dereferencing | `4` | `$ref` pointers resolved |
| Variable expansion | (default) | Query-string variable overrides applied |


# Symbol substitutions via URI

SVG resources referenced via `url(#name)` — markers, fill patterns, filters, and gradients —
can be parameterised by appending a query string to the fragment identifier:

```
url(#<symbol-name>?<param1>=<value1>&<param2>=<value2>)
```

The plugin loads the SVG resource file, substitutes every `$(variableName=defaultValue)`
placeholder with the supplied value (or the default if the parameter is absent), and injects
the result into the SVG `<defs>` section.

**Example — coloured spearhead marker:**

In the JSON attributes:
```json
"marker-end": "url(#spearhead?fill=#606060)"
```

In the marker SVG file (`markers/spearhead.svg`):
```svg
<marker id="spearhead" markerWidth="9" markerHeight="9"
        viewBox="0 0 10 10" orient="auto" refX="1" refY="5">
  <path d="M 0 0 L 10 5 L 0 10 z" fill="$(fill=black)"/>
</marker>
```

The placeholder `$(fill=black)` is replaced with `#606060`.  If `fill` is not supplied in the
URL, the default value `black` is used.

Multiple parameters are separated by `&`:
```json
"filter": "url(#rectbackground?border=black&background=white&borderwidth=1)"
```

**Supported SVG attributes for parameterised references:**

| Attribute | Resource type |
|-----------|--------------|
| `filter` | SVG filter element |
| `marker` | SVG marker element |
| `marker-start` | SVG marker element |
| `marker-mid` | SVG marker element |
| `marker-end` | SVG marker element |
| `fill` | SVG pattern element |
| `linearGradient` | SVG linearGradient element |
| `radialGradient` | SVG radialGradient element |

Each resource is included in the `<defs>` block only once per unique name, regardless of how
many times it is referenced.  If the same symbol name is used with different parameters in the
same product, only the first reference wins — use distinct names in that case.

**File search order** for SVG resources:

1. `<root>/customers/<customer>/<subdir>/<name>.svg`
2. `<root>/<name>.svg`
3. `<root>/resources/layers/<subdir>/<name>.svg`
4. `<root>/resources/<subdir>/<name>.svg`
5. `<root>/resources/<name>.svg`

The `<subdir>` is `markers/`, `patterns/`, `filters/`, or `gradients/` depending on the
attribute type.  An absolute path (starting with `/`) skips the customer directory and
searches from the Dali root directly.


# Dali querystring parameters

The Dali endpoint (default URL `/dali`) accepts the following query parameters.

**Required parameter:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `product` | string | Path to the product JSON file relative to `customers/<customer>/products/`. Example: `product=temperature` loads `.../products/temperature.json`. |

**Content and data selection:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `customer` | string | config default | Customer name; selects the sub-tree under `<root>/customers/`. |
| `type` | string | `svg` | Output format: `svg`, `png`, `pdf`, `ps`, `xml`, `geojson`, `topojson`, `kml`, `geotiff`, `mvt`. |
| `producer` | string | config default | Data producer (querydata model name). |
| `time` | string | latest | Valid time in any format accepted by the MacGyver time parser (ISO 8601, SQL, epoch, offset from now). |
| `time_offset` | int | 0 | Minutes added to `time`. |
| `origintime` | string | latest | Querydata origin time (model run time). |
| `interval_start` | int | – | Start of observation time interval in minutes before `time + time_offset`. |
| `interval_end` | int | – | End of observation time interval in minutes after `time + time_offset`. |
| `timestep` | int | – | Timestep in minutes. |
| `level` | double | – | Pressure level in hPa. |
| `levelId` / `levelid` | int | – | Level type ID. |
| `forecastNumber` | int | – | Ensemble member number. |
| `forecastType` | int | – | Forecast type. |
| `geometryId` | int | – | Grid geometry ID. |
| `source` | string | config default | Primary forecast source override. |
| `language` | string | config default | Language code, e.g. `fi`, `en`, `sv`. |
| `tz` | string | `UTC` | Timezone used when parsing `time`. |

**Layout and clipping:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | int | 1000 | Image width in pixels (20–10000). |
| `height` | int | 1000 | Image height in pixels (20–10000). |
| `margin` | int | – | Sets both `xmargin` and `ymargin`. |
| `xmargin` | int | 0 | X-margin for symbol clipping (pixels). |
| `ymargin` | int | 0 | Y-margin for symbol clipping (pixels). |
| `clip` | bool | false | Wrap each layer in a `<clipPath>`. |

**Overriding JSON structure fields:**

| Parameter | Type | Description |
|-----------|------|-------------|
| `title` | string | Override the product `title` field. |
| `projection` | JSON | Override the top-level `projection` block. |
| `views` | JSON | Override the top-level `views` array. |
| `defs` | JSON | Override the top-level `defs` block. |
| `attributes` | JSON | Override the top-level `attributes` block. |
| `animation` | JSON | Override the `animation` block. |
| `png` | JSON | Override the `png` output options block. |
| `svg_tmpl` | string | Select a CTPP2 template by name (see [Template selection](#template-selection)). |

## Product modification via querystring

Any field inside the product JSON can be overridden at request time by using dotted
`qid.fieldname` parameter names.  A `qid` must first be assigned to the layer or object to
be modified.

**Example:** override the weather parameter and producer of a layer whose `qid` is `"temp_layer"`:

```
GET /dali?product=temperature&temp_layer.parameter=DailyMeanTemperature&temp_layer.producer=daily00
```

This is equivalent to editing the JSON as:
```json
{ "qid": "temp_layer", "parameter": "DailyMeanTemperature", "producer": "daily00", "..." }
```

Dotted parameters work at any nesting depth as long as each intermediate object has a `qid`.

## Template selection

Products are rendered through a CTPP2 template.  The template is selected in the following
order of precedence:

1. The `svg_tmpl` query-string parameter.
2. The `svg_tmpl` field in the product JSON.
3. The entry in the `templates` config block matching the requested `type` (e.g. `geojson`,
   `topojson`, `kml`).
4. The `templates.default` config entry.
5. Hard-coded fallback: `"svg"`.

Template files are `.c2t` files stored in the directory configured by `templatedir` (default
`/usr/share/smartmet/wms`).

**Example `templates` block in the plugin configuration:**

```
templates:
{
    default  = "svg";
    geojson  = "geojson";
    topojson = "topojson";
    kml      = "kml";
}
```

## Debugging parameters

The following parameters are useful during development and are silently ignored in production
or stripped by the allowed-parameter filter if not in the `allowed_keys` set.

| Parameter | Description |
|-----------|-------------|
| `printjson=1` | Print the fully expanded product JSON to the server console. |
| `printhash=1` | Print the CTPP2 CDT object to the server console. |
| `printparams=1` | Print the grid parameter list used by the product. |
| `timer=1` | Print timing information per product generation stage. |
| `stage=1`–`4` | Return the intermediate JSON at the given [pipeline stage](#processing-pipeline) instead of rendering. |


# WMS querystring parameters

The WMS endpoint (default URL `/wms`) implements OGC WMS 1.3.0.

### GetCapabilities

```
GET /wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetCapabilities
```

Returns the capabilities XML document listing all available layers, CRS, and formats.
The optional `FORMAT` parameter selects `text/xml` (default) or `application/json`.

### GetMap

Required parameters:

| Parameter | Description |
|-----------|-------------|
| `SERVICE=WMS` | Must be `WMS`. |
| `VERSION=1.3.0` | WMS version (also `1.1.1` / `1.1.0` are accepted for backward compatibility). |
| `REQUEST=GetMap` | Request type. |
| `LAYERS` | Comma-separated list of layer names. |
| `STYLES` | Comma-separated list of styles (may be empty: `STYLES=`). |
| `CRS` (WMS 1.3) or `SRS` (WMS 1.1) | Coordinate reference system identifier, e.g. `EPSG:4326`. |
| `BBOX` | Bounding box: `minx,miny,maxx,maxy`. Axis order follows the CRS definition in WMS 1.3. |
| `WIDTH` | Image width in pixels. |
| `HEIGHT` | Image height in pixels. |
| `FORMAT` | MIME type: `image/png`, `image/svg+xml`, `application/pdf`, `application/geo+json`, `application/topo+json`. |

Optional parameters:

| Parameter | Description |
|-----------|-------------|
| `TRANSPARENT=TRUE\|FALSE` | Whether the background is transparent. |
| `BGCOLOR=0xRRGGBB` | Background colour (used only when `TRANSPARENT=FALSE`). |
| `EXCEPTIONS=XML\|application/json` | Exception reporting format. |
| `TIME` | ISO 8601 time value or comma-separated list of times. |
| `ELEVATION` | Elevation value (forwarded to the layer as the `level` parameter). |
| `INTERVAL_START` | Extension: start of observation time interval in minutes before `TIME`. |
| `INTERVAL_END` | Extension: end of observation time interval in minutes after `TIME`. |

### GetLegendGraphic

```
GET /wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetLegendGraphic&LAYER=<name>&FORMAT=image/png
```

Returns a legend image for the named layer.


# WMS GetMap and GetCapabilities configuration

A product definition may at the top level include the following settings which directly affect WMS GetCapabilities and GetMap requests:

<pre><b>WMS settings</b></pre>
| Name         | Type              | Default value | Description                                                                                        |
| ------------ | ----------------- | ------------- | -------------------------------------------------------------------------------------------------- |
| hidden       | (bool)            | false         | If true the WMS will not appear in GetCapabilities responses but GetMap requests continue to work. |
| title        | _Text_            | -             | Obligatory title for the WMS layer                                                                 |
| name         | string            | -             | Name of the WMS layer. By default the name is deduced from the path of the configuration file.     |
| abstract     | (_Text_)          | -             | Abstract for the layer.                                                                            |
| keywords     | (list of strings) | -             | Keywords for the layer.                                                                            |
| opaque       | (int)             | 0             | Is the WMS layer opaque.                                                                           |
| queryable    | (int)             | 0             | Does the layer support GetFeatureInfo requests.                                                    |
| cascaded     | (int)             | 0             | How many times the layer has been cascaded.                                                        |
| no_subsets   | (int)             | 0             | Can only the full bounding box be requested.                                                       |
| fixed_width  | (int)             | 0             | Nonzero implies the size cannot be changed.                                                        |
| fixed_height | (int)             | 0             | Nonzero implies the size cannot be changed                                                         |
| capabilities_start | (duration)  | -             | Limit start time of GetCapabilities                                                                |
| capabilities_end   | (duration)  | -             | Limit end time of GetCapabilities                                                                  |

The GetCapabilities list of available times occasionally needs to be limited. For example, one might have weather observations for more than one hunder years, and one would not want to generate a full many or time slider for all available times. Typically the observation engine provides metadata for observation producers which are more useful for example for time series requests than WMS requests.

The time duration can be

 * an integer, which are understood to be minutes
 * a string with an ISO-8601 time duration such as "P10D", "P1Y", "PT24H" etc
 * an integer string not starting with P, and ending with a time suffix such as "100d"

Using ISO-8601 is recommended, the other syntaxes are supported mostly for backward compatibility as the same time duration parser is used in several other places.


## WMS layer variants

In meteorology it is common to have multiple weather models with the same parameters available. For example for temperature the WMS layer settings would typically differ only in the name, title and abstract, and the product settings in the name of the producer and occasionally the name of the weather parameter (road temperature, ground temperature).

In such cases one use the same JSON file for the product settings, and define how the different variants are change in the settings. Below is an example how one might define some variants. It is also possible to change individual settings deeper in the JSON structure as long as a <code>qid</code> has been set. Substitutions then work as if the setting has been changed via the <code>qid</code> using a querystring. For example if a _IsobandLayer_ layer has a <code>qid</code> with value <code>temp_layer</code>, one can change the parameter used by the layer by using <code>temp_layer.parameter=SomeTemperature</code> in the querystring, or by using <code>"temp_layer.parameter": "SomeTemperature"</code> in the variant settings.

<pre>Sample variant configuration<code>
{
    "parameter": "Temperature", // default
    "producer": "pal", // default

    // all variants to be generated
    "variants":
    [
	{
	    "name": "fmi:pal:temperature",
	    "title": "FMI Forecast Temperature",
	    "abstract": "Terrain corrected temperature using atmospheric lapse rate and terrain elevation produced by FMI."
	},
	{
	    "name": "fmi:pal:dailymeantemperature",
	    "parameter": "DailyMeanTemperature",
	    "producer": "daily00",
	    "title": "FMI Forecast Mean Temperature",
	    "abstract": "Daily mean temperature produced by FMI."
	},
	{
	    "name": "fmi:roadmodel:roadtemperature",
	    "parameter": "RoadTemperature",
	    "producer": "roadmodel_skandinavia_pinta",
	    "title":
	    {
    		"en": "Roadmodel Troad",
     		"fi": "Tienpintalämpötila"
	    },
	    "abstract":
	    {
		    "en": "Road temperature produced by roadmodel",
    		"fi": "Tiesäämallin laskema tienpintalämpötila"
	    }
	}
    ],

    // Common settings
    "projection": {},
    "styles":
    [
	...
    ],
    "views":
    [
	...
    ]
}
</code></pre>

# Configuration 
In order to use the Dali plugin you need to edit two configuration files. These files are the main configuration file of the SmartMet Server environment and the Dali plugin specific configuration file.

## Main configuration file

The main configuration file is named as "smartmet.conf". The main purpose of this configuration file is to define which plugins and engines need to be loaded when the server is started. In addition, it defines which configuration files these plugins and engines are using.

In order to use the Dali plugin you need to do the following steps:

1. Check that there is the "dali" entry listed in the "plugins" attribute. If not, you should add it.

<pre><code>
	plugins = ["dali"]
</code></pre>


2. Check that the "engines" attribute contains all required engines such as AuthEngine, Contour, GeoEngine, GIS, ObsEngine, QEngine". Notice that the "geoengine" entry must be before the "obsengine" entry because of some linking reasons such as the  ObsEngine uses the  GeoEngine's functions.

<pre><code>
	engines = ["authengine","contour","geoengine","gis","obsengine","qengine"]
</code></pre>


3. Check that there is a dedicated configuration entry for the Dali plugin. In this entry you can disable or enable the usage of the Dali plugin. In addition, we  can define the name and path of the configuration file for the Dali plugin.

<pre><code>
		dali:
		{
			disabled = false;
			configfile = "/etc/smartmet/plugins/dali.conf"
		}

</code></pre>


## Plugin configuration file

The plugin configuration file uses libconfig++ syntax.  The path to this file is set in
`smartmet.conf` under the `dali.configfile` key.

### Required settings

| Setting | Description |
|---------|-------------|
| `root` | Root directory for Dali product files.  Customer-specific content lives under `<root>/customers/<customer>/`. |
| `wms.root` | Root directory for WMS product files.  Separate from `root` to allow different access controls. |
| `templates` | Group mapping output type names to CTPP2 template base names (without `.c2t`).  A `default` entry is required. |
| `regular_attributes` | Array of allowed SVG regular attribute names (see the SVG specification appendix). |
| `presentation_attributes` | Array of allowed SVG presentation attribute names. |

### Optional settings

| Setting | Default | Description |
|---------|---------|-------------|
| `url` | `/dali` | URL path of the native Dali endpoint. |
| `model` | – | Default querydata producer name. |
| `language` | – | Default language code. |
| `languages` | – | Comma-separated list of supported language codes. |
| `customer` | – | Default customer name. |
| `primaryForecastSource` | – | Default primary forecast source (`querydata` or `grid`). |
| `template` | `svg` | Default CTPP2 template base name (deprecated in favour of `templates.default`). |
| `templatedir` | `/usr/share/smartmet/wms` | Directory containing CTPP2 `.c2t` template files. |
| `css_cache_size` | 1000 | Maximum number of cached CSS stylesheets. |
| `max_image_size` | – | Maximum allowed image area in pixels (width × height). |
| `wms.url` | `/wms` | URL path of the WMS endpoint. |
| `wms.max_layers` | 10 | Maximum number of WMS layers per GetMap request (DDoS protection). |
| `wms.quiet` | `false` | If true, suppress WMS error stack traces in the log. |
| `wmts.url` | `/wmts` | URL path of the WMTS endpoint. |
| `wmts.tile_width` | 1024 | Default WMTS tile width. |
| `wmts.tile_height` | 1024 | Default WMTS tile height. |
| `tiles.url` | `/tiles` | URL path of the native tile endpoint. |
| `authenticate` | `false` | Enable API-key authentication (requires the authentication engine). |
| `observation_disabled` | `false` | Disable the observation engine (for deployments without ObsEngine). |
| `gridengine_disabled` | `false` | Disable the grid engine (for deployments without GridEngine). |
| `heatmap.max_points` | – | Maximum number of points in a heatmap layer. |

### `cache` group

Controls the image cache used for binary formats (PNG, WebP, PDF).

| Setting | Default | Description |
|---------|---------|-------------|
| `cache.directory` | – | Filesystem cache directory path. |
| `cache.memory_bytes` | – | Maximum memory cache size in bytes (also accepts strings like `"100M"`). |
| `cache.filesystem_bytes` | – | Maximum filesystem cache size in bytes. |

### `templates` group

Maps product type strings to CTPP2 template names.  The `default` entry is used when no
specific entry matches.

```
templates:
{
    default  = "svg";
    geojson  = "geojson";
    topojson = "topojson";
    kml      = "kml";
}
```

### `precision` group

Overrides the default SVG coordinate precision (number of decimal places) per layer type.
The key `default` sets the fallback.

```
precision:
{
    default  = 0.3;
    geojson  = 5.0;
    topojson = 5.0;
}
```

### `unit_conversion` group

Defines named unit conversions that can be referenced in product JSON.  Each conversion
applies `output = input * multiplier + offset`.

```
unit_conversion:
{
    ms_to_knots:          { multiplier = 1.94384449244; }
    celsius_to_fahrenheit:{ multiplier = 1.8; offset = 32.0; }
    celsius_to_kelvin:    { offset = 273.15; }
}
```

### `wms` group

The `wms` group contains a `get_capabilities` sub-group for configuring the
`GetCapabilities` response (service metadata, contact information, supported CRS list,
supported output formats, INSPIRE extended capabilities, etc.) and a `get_legend_graphic`
sub-group for legend layout and symbol translations.  See the test configuration file for
a fully annotated example.

### Sample configuration file

Below is a minimal but complete plugin configuration file:

```
// Dali plugin configuration

url       = "/dali";
model     = "pal_skandinavia";
language  = "en";
languages = "en,fi,sv";
customer  = "default";

root = "/var/smartmet/dali";

templatedir = "/usr/share/smartmet/wms";

templates:
{
    default  = "svg";
    geojson  = "geojson";
    topojson = "topojson";
    kml      = "kml";
}

precision:
{
    default  = 0.3;
    geojson  = 5.0;
    topojson = 5.0;
}

cache:
{
    directory        = "/var/cache/smartmet/dali";
    memory_bytes     = "100M";
    filesystem_bytes = "500M";
}

max_image_size = 20971520;  // 20 M pixels

unit_conversion:
{
    ms_to_knots:           { multiplier = 1.94384449244; }
    celsius_to_fahrenheit: { multiplier = 1.8; offset = 32.0; }
    celsius_to_kelvin:     { offset = 273.15; }
}

// WMS endpoint settings
wms:
{
    url        = "/wms";
    root       = "/var/smartmet/wms";
    max_layers = 10;
    quiet      = false;

    get_capabilities:
    {
        version         = "1.3.0";
        disable_updates = false;
        expiration_time = 60;

        service:
        {
            title = "SmartMet Web Map Service";
        };

        capability:
        {
            request:
            {
                getcapabilities: { format = ["text/xml"]; dcptype: ({ http: { get: { online_resource = "https://example.com/wms"; }; }; }); };
                getmap:          { format = ["image/png", "image/svg+xml"]; dcptype: ({ http: { get: { online_resource = "https://example.com/wms"; }; }; }); };
            };
            exception = ["XML"];
            master_layer: { title = "SmartMet WMS"; queryable = 0; opaque = 1; };
        };
    };
};

// SVG attribute white-lists (abbreviated; full lists in the standard configuration)
regular_attributes      = ["id", "class", "width", "height", "viewBox", "transform", /* ... */];
presentation_attributes = ["fill", "stroke", "stroke-width", "opacity", "font-size", /* ... */];
```


---

# Map Projections

Any PROJ projection string can be supplied as the `p.crs` URL parameter on the `/dali` endpoint.  The test suite exercises the full set of projections supported by the installed PROJ library using the `map` test product.

## Test Product

The `map` product ([`test/dali/customers/test/products/map.json`](../test/dali/customers/test/products/map.json)) renders:

- A blue ocean polygon covering the full world extent
- Natural Earth `admin_0_countries` coastlines and land fills
- A 10° graticule grid

Additional per-test parameters override the projection's center (`p.cx`, `p.cy`), output size (`p.xsize`, `p.ysize`), and resolution (`p.resolution`, in km/pixel).  Test inputs are in [`test/input/map_*.get`](../test/input/) and PNG renderings in [`docs/images/maps/`](images/maps/).

## Unsupported Projections

The following projections are listed in [`test/input/.testignore`](../test/input/.testignore) and excluded from the test suite.

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
| `map_merc` | `+proj=merc` | [Mercator](https://proj.org/en/stable/operations/projections/merc.html) — conformal, shapes preserved, area greatly distorted near poles | ![map_merc](images/maps/map_merc.png) |
| `map_webmercator` | `EPSG:3857` | [Web Mercator](https://proj.org/en/stable/operations/projections/merc.html) — de-facto standard for web tile services | ![map_webmercator](images/maps/map_webmercator.png) |
| `map_tmerc` | `+proj=tmerc` | [Transverse Mercator](https://proj.org/en/stable/operations/projections/tmerc.html) — conformal, accurate along a central meridian | ![map_tmerc](images/maps/map_tmerc.png) |
| `map_gstmerc` | `+proj=gstmerc` | [Gauss-Schreiber Transverse Mercator](https://proj.org/en/stable/operations/projections/gstmerc.html) | ![map_gstmerc](images/maps/map_gstmerc.png) |
| `map_tobmerc` | `+proj=tobmerc` | [Tobler-Mercator](https://proj.org/en/stable/operations/projections/tobmerc.html) — variation with reduced polar distortion | ![map_tobmerc](images/maps/map_tobmerc.png) |
| `map_eqc` | `EPSG:4087` | [Equidistant Cylindrical](https://proj.org/en/stable/operations/projections/eqc.html) — plate carrée variant, meridians and parallels equally spaced | ![map_eqc](images/maps/map_eqc.png) |
| `map_cc` | `+proj=cc` | [Central Cylindrical](https://proj.org/en/stable/operations/projections/cc.html) — perspective projection from the Earth's centre onto a cylinder | ![map_cc](images/maps/map_cc.png) |
| `map_cea` | `+proj=cea` | [Equal-Area Cylindrical](https://proj.org/en/stable/operations/projections/cea.html) — Lambert cylindrical, preserves area | ![map_cea](images/maps/map_cea.png) |
| `map_mill` | `+proj=mill` | [Miller Cylindrical](https://proj.org/en/stable/operations/projections/mill.html) — compromise between Mercator and equal-area | ![map_mill](images/maps/map_mill.png) |
| `map_comill` | `+proj=comill` | [Compact Miller](https://proj.org/en/stable/operations/projections/comill.html) — reduced polar extent | ![map_comill](images/maps/map_comill.png) |
| `map_gall` | `+proj=gall` | [Gall Stereographic](https://proj.org/en/stable/operations/projections/gall.html) — cylindrical with standard parallels at ±45° | ![map_gall](images/maps/map_gall.png) |
| `map_patterson` | `+proj=patterson` | [Patterson Cylindrical](https://proj.org/en/stable/operations/projections/patterson.html) — modern compromise projection | ![map_patterson](images/maps/map_patterson.png) |
| `map_ocea` | `+proj=ocea` | [Oblique Cylindrical Equal-Area](https://proj.org/en/stable/operations/projections/ocea.html) | ![map_ocea](images/maps/map_ocea.png) |

---

### Conic

Conic projections are well-suited for mid-latitude regions.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_lcc` | `+proj=lcc +lon_0=-90 +lat_1=33 +lat_2=45` | [Lambert Conformal Conic](https://proj.org/en/stable/operations/projections/lcc.html) — conformal, standard for aviation and NWP | ![map_lcc](images/maps/map_lcc.png) |
| `map_lcca` | `+proj=lcca +lat_0=35` | [Lambert Conformal Conic Alternative](https://proj.org/en/stable/operations/projections/lcca.html) | ![map_lcca](images/maps/map_lcca.png) |
| `map_aea` | `+proj=aea +lat_1=29.5 +lat_2=42.5` | [Albers Equal-Area Conic](https://proj.org/en/stable/operations/projections/aea.html) — equal-area, used for US thematic maps | ![map_aea](images/maps/map_aea.png) |
| `map_leac` | `+proj=leac` | [Lambert Equal-Area Conic](https://proj.org/en/stable/operations/projections/leac.html) | ![map_leac](images/maps/map_leac.png) |
| `map_eqdc` | `+proj=eqdc +lat_1=55 +lat_2=60` | [Equidistant Conic](https://proj.org/en/stable/operations/projections/eqdc.html) — preserves distances along meridians | ![map_eqdc](images/maps/map_eqdc.png) |
| `map_bonne` | `+proj=bonne +lat_1=10` | [Bonne](https://proj.org/en/stable/operations/projections/bonne.html) — pseudo-conic equal-area | ![map_bonne](images/maps/map_bonne.png) |
| `map_cass` | `+proj=cass` | [Cassini-Soldner](https://proj.org/en/stable/operations/projections/cass.html) — transverse equidistant cylindrical | ![map_cass](images/maps/map_cass.png) |
| `map_poly` | `+proj=poly` | [American Polyconic](https://proj.org/en/stable/operations/projections/poly.html) | ![map_poly](images/maps/map_poly.png) |
| `map_rpoly` | `+proj=rpoly` | [Rectangular Polyconic](https://proj.org/en/stable/operations/projections/rpoly.html) | ![map_rpoly](images/maps/map_rpoly.png) |
| `map_euler` | `+proj=euler +lat_1=67 +lat_2=75` | [Euler](https://proj.org/en/stable/operations/projections/euler.html) | ![map_euler](images/maps/map_euler.png) |
| `map_tissot` | `+proj=tissot +lat_1=60 +lat_2=65` | [Tissot Conic](https://proj.org/en/stable/operations/projections/tissot.html) | ![map_tissot](images/maps/map_tissot.png) |
| `map_murd1` | `+proj=murd1 +lat_1=30 +lat_2=50` | [Murdoch I](https://proj.org/en/stable/operations/projections/murd1.html) | ![map_murd1](images/maps/map_murd1.png) |
| `map_murd3` | `+proj=murd3 +lat_1=30 +lat_2=50` | [Murdoch III](https://proj.org/en/stable/operations/projections/murd3.html) | ![map_murd3](images/maps/map_murd3.png) |

---

### Azimuthal

Azimuthal projections preserve directions from the projection centre.

| Test | PROJ String / EPSG | Description | Output |
|------|-------------------|-------------|--------|
| `map_laea` | `+proj=laea` | [Lambert Azimuthal Equal-Area](https://proj.org/en/stable/operations/projections/laea.html) — equal-area, used for continental maps | ![map_laea](images/maps/map_laea.png) |
| `map_stere` | `+proj=stere +lat_0=90 +lat_ts=75` | [Polar Stereographic](https://proj.org/en/stable/operations/projections/stere.html) — conformal, standard for polar regions | ![map_stere](images/maps/map_stere.png) |
| `map_sterea` | `+proj=sterea +lat_0=90` | [Oblique Stereographic Alternative](https://proj.org/en/stable/operations/projections/sterea.html) — Dutch variant used for the Netherlands | ![map_sterea](images/maps/map_sterea.png) |
| `map_arctic` | `EPSG:3995` | [Arctic Polar Stereographic](https://proj.org/en/stable/operations/projections/stere.html) — standard for Arctic products | ![map_arctic](images/maps/map_arctic.png) |
| `map_antarctic` | `EPSG:3031` | [Antarctic Polar Stereographic](https://proj.org/en/stable/operations/projections/stere.html) — standard for Antarctic products | ![map_antarctic](images/maps/map_antarctic.png) |
| `map_ups` | `+proj=ups` | [Universal Polar Stereographic](https://proj.org/en/stable/operations/projections/ups.html) — polar component of the UPS military system | ![map_ups](images/maps/map_ups.png) |
| `map_ortho` | `+proj=ortho` | [Orthographic](https://proj.org/en/stable/operations/projections/ortho.html) — perspective view from infinity; globe appearance | ![map_ortho](images/maps/map_ortho.png) |
| `map_airy` | `+proj=airy` | [Airy](https://proj.org/en/stable/operations/projections/airy.html) — minimum-error azimuthal | ![map_airy](images/maps/map_airy.png) |
| `map_rouss` | `+proj=rouss` | [Roussilhe Stereographic](https://proj.org/en/stable/operations/projections/rouss.html) | ![map_rouss](images/maps/map_rouss.png) |
| `map_tpers` | `+proj=tpers +h=5500000 +lat_0=40` | [Tilted Perspective](https://proj.org/en/stable/operations/projections/tpers.html) — satellite view from finite altitude with tilt | ![map_tpers](images/maps/map_tpers.png) |
| `map_tpeqd` | `+proj=tpeqd +lat_1=60 +lat_2=65` | [Two-Point Equidistant](https://proj.org/en/stable/operations/projections/tpeqd.html) | ![map_tpeqd](images/maps/map_tpeqd.png) |
| `map_aitoff` | `+proj=aitoff` | [Aitoff](https://proj.org/en/stable/operations/projections/aitoff.html) — modified azimuthal, world map | ![map_aitoff](images/maps/map_aitoff.png) |
| `map_hammer` | `+proj=hammer` | [Hammer & Eckert-Greifendorff](https://proj.org/en/stable/operations/projections/hammer.html) — equal-area modified azimuthal | ![map_hammer](images/maps/map_hammer.png) |

---

### Pseudocylindrical

Pseudocylindrical projections have straight parallels but curved meridians.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_sinu` | `+proj=sinu` | [Sinusoidal (Sanson-Flamsteed)](https://proj.org/en/stable/operations/projections/sinu.html) — equal-area, simple formula | ![map_sinu](images/maps/map_sinu.png) |
| `map_moll` | `+proj=moll` | [Mollweide](https://proj.org/en/stable/operations/projections/moll.html) — equal-area ellipse | ![map_moll](images/maps/map_moll.png) |
| `map_robin` | `+proj=robin` | [Robinson](https://proj.org/en/stable/operations/projections/robin.html) — compromise, formerly used by National Geographic | ![map_robin](images/maps/map_robin.png) |
| `map_natearth` | `+proj=natearth` | [Natural Earth](https://proj.org/en/stable/operations/projections/natearth.html) — smooth compromise, widely used for thematic maps | ![map_natearth](images/maps/map_natearth.png) |
| `map_natearth2` | `+proj=natearth2` | [Natural Earth II](https://proj.org/en/stable/operations/projections/natearth2.html) — variation with flatter poles | ![map_natearth2](images/maps/map_natearth2.png) |
| `map_natearth_shifted` | `+proj=natearth +lon_0=90` | Natural Earth (Pacific-centred) | ![map_natearth_shifted](images/maps/map_natearth_shifted.png) |
| `map_eqearth` | `+proj=eqearth` | [Equal Earth](https://proj.org/en/stable/operations/projections/eqearth.html) — equal-area, published 2018 | ![map_eqearth](images/maps/map_eqearth.png) |
| `map_wintri` | `+proj=wintri` | [Winkel Tripel](https://proj.org/en/stable/operations/projections/wintri.html) — compromise minimising distance/area/angle distortions; used by National Geographic since 1998 | ![map_wintri](images/maps/map_wintri.png) |
| `map_wink1` | `+proj=wink1` | [Winkel I](https://proj.org/en/stable/operations/projections/wink1.html) | ![map_wink1](images/maps/map_wink1.png) |
| `map_wink2` | `+proj=wink2` | [Winkel II](https://proj.org/en/stable/operations/projections/wink2.html) | ![map_wink2](images/maps/map_wink2.png) |
| `map_eck1` | `+proj=eck1` | [Eckert I](https://proj.org/en/stable/operations/projections/eck1.html) | ![map_eck1](images/maps/map_eck1.png) |
| `map_eck2` | `+proj=eck2` | [Eckert II](https://proj.org/en/stable/operations/projections/eck2.html) | ![map_eck2](images/maps/map_eck2.png) |
| `map_eck3` | `+proj=eck3` | [Eckert III](https://proj.org/en/stable/operations/projections/eck3.html) | ![map_eck3](images/maps/map_eck3.png) |
| `map_eck4` | `+proj=eck4` | [Eckert IV](https://proj.org/en/stable/operations/projections/eck4.html) — equal-area | ![map_eck4](images/maps/map_eck4.png) |
| `map_eck5` | `+proj=eck5` | [Eckert V](https://proj.org/en/stable/operations/projections/eck5.html) | ![map_eck5](images/maps/map_eck5.png) |
| `map_eck6` | `+proj=eck6` | [Eckert VI](https://proj.org/en/stable/operations/projections/eck6.html) — equal-area | ![map_eck6](images/maps/map_eck6.png) |
| `map_wag1` | `+proj=wag1` | [Wagner I](https://proj.org/en/stable/operations/projections/wag1.html) — equal-area | ![map_wag1](images/maps/map_wag1.png) |
| `map_wag2` | `+proj=wag2` | [Wagner II](https://proj.org/en/stable/operations/projections/wag2.html) | ![map_wag2](images/maps/map_wag2.png) |
| `map_wag3` | `+proj=wag3` | [Wagner III](https://proj.org/en/stable/operations/projections/wag3.html) | ![map_wag3](images/maps/map_wag3.png) |
| `map_wag4` | `+proj=wag4` | [Wagner IV](https://proj.org/en/stable/operations/projections/wag4.html) — equal-area | ![map_wag4](images/maps/map_wag4.png) |
| `map_wag5` | `+proj=wag5` | [Wagner V](https://proj.org/en/stable/operations/projections/wag5.html) | ![map_wag5](images/maps/map_wag5.png) |
| `map_wag6` | `+proj=wag6` | [Wagner VI](https://proj.org/en/stable/operations/projections/wag6.html) | ![map_wag6](images/maps/map_wag6.png) |
| `map_wag7` | `+proj=wag7` | [Wagner VII](https://proj.org/en/stable/operations/projections/wag7.html) — equal-area azimuthal hybrid | ![map_wag7](images/maps/map_wag7.png) |
| `map_kav5` | `+proj=kav5` | [Kavraiskiy V](https://proj.org/en/stable/operations/projections/kav5.html) | ![map_kav5](images/maps/map_kav5.png) |
| `map_kav7` | `+proj=kav7` | [Kavraiskiy VII](https://proj.org/en/stable/operations/projections/kav7.html) | ![map_kav7](images/maps/map_kav7.png) |
| `map_crast` | `+proj=crast` | [Craster Parabolic](https://proj.org/en/stable/operations/projections/crast.html) — equal-area | ![map_crast](images/maps/map_crast.png) |
| `map_boggs` | `+proj=boggs` | [Boggs Eumorphic](https://proj.org/en/stable/operations/projections/boggs.html) — equal-area | ![map_boggs](images/maps/map_boggs.png) |
| `map_hatano` | `+proj=hatano` | [Hatano Asymmetrical Equal-Area](https://proj.org/en/stable/operations/projections/hatano.html) — different standard parallels in N and S hemispheres | ![map_hatano](images/maps/map_hatano.png) |
| `map_fouc` | `+proj=fouc` | [Foucaut](https://proj.org/en/stable/operations/projections/fouc.html) — equal-area | ![map_fouc](images/maps/map_fouc.png) |
| `map_fouc_s` | `+proj=fouc_s` | [Foucaut Sinusoidal](https://proj.org/en/stable/operations/projections/fouc_s.html) — blend of sinusoidal and cylindrical | ![map_fouc_s](images/maps/map_fouc_s.png) |
| `map_qua_aut` | `+proj=qua_aut` | [Quartic Authalic](https://proj.org/en/stable/operations/projections/qua_aut.html) — equal-area | ![map_qua_aut](images/maps/map_qua_aut.png) |
| `map_nell` | `+proj=nell` | [Nell](https://proj.org/en/stable/operations/projections/nell.html) | ![map_nell](images/maps/map_nell.png) |
| `map_nell_h` | `+proj=nell_h` | [Nell-Hammer](https://proj.org/en/stable/operations/projections/nell_h.html) | ![map_nell_h](images/maps/map_nell_h.png) |
| `map_loxim` | `+proj=loxim` | [Loximuthal](https://proj.org/en/stable/operations/projections/loxim.html) | ![map_loxim](images/maps/map_loxim.png) |
| `map_putp1` | `+proj=putp1` | [Putnins P1](https://proj.org/en/stable/operations/projections/putp1.html) | ![map_putp1](images/maps/map_putp1.png) |
| `map_putp2` | `+proj=putp2` | [Putnins P2](https://proj.org/en/stable/operations/projections/putp2.html) | ![map_putp2](images/maps/map_putp2.png) |
| `map_putp3` | `+proj=putp3` | [Putnins P3](https://proj.org/en/stable/operations/projections/putp3.html) | ![map_putp3](images/maps/map_putp3.png) |
| `map_putp3p` | `+proj=putp3p` | [Putnins P3'](https://proj.org/en/stable/operations/projections/putp3p.html) | ![map_putp3p](images/maps/map_putp3p.png) |
| `map_putp4p` | `+proj=putp4p` | [Putnins P4'](https://proj.org/en/stable/operations/projections/putp4p.html) | ![map_putp4p](images/maps/map_putp4p.png) |
| `map_putp5` | `+proj=putp5` | [Putnins P5](https://proj.org/en/stable/operations/projections/putp5.html) | ![map_putp5](images/maps/map_putp5.png) |
| `map_putp5p` | `+proj=putp5p` | [Putnins P5'](https://proj.org/en/stable/operations/projections/putp5p.html) | ![map_putp5p](images/maps/map_putp5p.png) |
| `map_putp6` | `+proj=putp6` | [Putnins P6](https://proj.org/en/stable/operations/projections/putp6.html) | ![map_putp6](images/maps/map_putp6.png) |
| `map_putp6p` | `+proj=putp6p` | [Putnins P6'](https://proj.org/en/stable/operations/projections/putp6p.html) | ![map_putp6p](images/maps/map_putp6p.png) |
| `map_collg` | `+proj=collg` | [Collignon](https://proj.org/en/stable/operations/projections/collg.html) — equal-area triangle shape | ![map_collg](images/maps/map_collg.png) |
| `map_denoy` | `+proj=denoy` | [Denoyer Semi-Elliptical](https://proj.org/en/stable/operations/projections/denoy.html) | ![map_denoy](images/maps/map_denoy.png) |
| `map_fahey` | `+proj=fahey` | [Fahey](https://proj.org/en/stable/operations/projections/fahey.html) | ![map_fahey](images/maps/map_fahey.png) |
| `map_gn_sinu` | `+proj=gn_sinu +m=2 +n=3` | [General Sinusoidal Series](https://proj.org/en/stable/operations/projections/gn_sinu.html) | ![map_gn_sinu](images/maps/map_gn_sinu.png) |
| `map_urmfps` | `+proj=urmfps +n=0.5` | [Urmaev Flat-Polar Sinusoidal](https://proj.org/en/stable/operations/projections/urmfps.html) | ![map_urmfps](images/maps/map_urmfps.png) |
| `map_urm5` | `+proj=urm5 +n=0.9 +alpha=2 +q=4` | [Urmaev V](https://proj.org/en/stable/operations/projections/urm5.html) | ![map_urm5](images/maps/map_urm5.png) |
| `map_weren` | `+proj=weren` | [Werenskiold I](https://proj.org/en/stable/operations/projections/weren.html) | ![map_weren](images/maps/map_weren.png) |
| `map_larr` | `+proj=larr` | [Larrivée](https://proj.org/en/stable/operations/projections/larr.html) | ![map_larr](images/maps/map_larr.png) |
| `map_lask` | `+proj=lask` | [Laskowski](https://proj.org/en/stable/operations/projections/lask.html) | ![map_lask](images/maps/map_lask.png) |
| `map_times` | `+proj=times` | [Times](https://proj.org/en/stable/operations/projections/times.html) | ![map_times](images/maps/map_times.png) |
| `map_mbt_fps` | `+proj=mbt_fps` | [McBryde-Thomas Flat-Polar Sinusoidal](https://proj.org/en/stable/operations/projections/mbt_fps.html) — equal-area | ![map_mbt_fps](images/maps/map_mbt_fps.png) |
| `map_mbtfps` | `+proj=mbtfps` | [McBryde-Thomas Flat-Polar Sinusoidal (alt.)](https://proj.org/en/stable/operations/projections/mbtfps.html) — equal-area | ![map_mbtfps](images/maps/map_mbtfps.png) |
| `map_mbt_s` | `+proj=mbt_s` | [McBryde-Thomas Flat-Polar Sinusoidal (var.)](https://proj.org/en/stable/operations/projections/mbt_s.html) — equal-area | ![map_mbt_s](images/maps/map_mbt_s.png) |
| `map_mbtfpp` | `+proj=mbtfpp` | [McBryde-Thomas Flat-Polar Parabolic](https://proj.org/en/stable/operations/projections/mbtfpp.html) — equal-area | ![map_mbtfpp](images/maps/map_mbtfpp.png) |
| `map_mbtfpq` | `+proj=mbtfpq` | [McBryde-Thomas Flat-Polar Quartic](https://proj.org/en/stable/operations/projections/mbtfpq.html) — equal-area | ![map_mbtfpq](images/maps/map_mbtfpq.png) |

---

### Globular and Polyhedral

These projections use unconventional boundaries for artistic or special purposes.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_adams_hemi` | `+proj=adams_hemi` | [Adams Hemisphere-in-a-Square](https://proj.org/en/stable/operations/projections/adams_hemi.html) — conformal, entire hemisphere in a square | ![map_adams_hemi](images/maps/map_adams_hemi.png) |
| `map_adams_ws1` | `+proj=adams_ws1` | [Adams World in a Square I](https://proj.org/en/stable/operations/projections/adams_ws1.html) — conformal world map in a square | ![map_adams_ws1](images/maps/map_adams_ws1.png) |
| `map_adams_ws2` | `+proj=adams_ws2` | [Adams World in a Square II](https://proj.org/en/stable/operations/projections/adams_ws2.html) | ![map_adams_ws2](images/maps/map_adams_ws2.png) |
| `map_apian` | `+proj=apian` | [Apian Globular I](https://proj.org/en/stable/operations/projections/apian.html) | ![map_apian](images/maps/map_apian.png) |
| `map_bacon` | `+proj=bacon` | [Bacon Globular](https://proj.org/en/stable/operations/projections/bacon.html) | ![map_bacon](images/maps/map_bacon.png) |
| `map_ortel` | `+proj=ortel` | [Ortelius Oval](https://proj.org/en/stable/operations/projections/ortel.html) | ![map_ortel](images/maps/map_ortel.png) |
| `map_august` | `+proj=august` | [August Epicycloidal](https://proj.org/en/stable/operations/projections/august.html) — conformal world-in-a-circle | ![map_august](images/maps/map_august.png) |
| `map_lagrng` | `+proj=lagrng` | [Lagrange](https://proj.org/en/stable/operations/projections/lagrng.html) — conformal in a circle | ![map_lagrng](images/maps/map_lagrng.png) |
| `map_gins8` | `+proj=gins8` | [Ginsburg VIII](https://proj.org/en/stable/operations/projections/gins8.html) | ![map_gins8](images/maps/map_gins8.png) |
| `map_healpix` | `+proj=healpix` | [HEALPix](https://proj.org/en/stable/operations/projections/healpix.html) — hierarchical equal-area pixelization; used in astrophysics and CMB analysis | ![map_healpix](images/maps/map_healpix.png) |

---

### Van der Grinten

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_vandg` | `+proj=vandg` | [Van der Grinten I](https://proj.org/en/stable/operations/projections/vandg.html) — world in a circle | ![map_vandg](images/maps/map_vandg.png) |
| `map_vandg2` | `+proj=vandg2` | [Van der Grinten II](https://proj.org/en/stable/operations/projections/vandg2.html) | ![map_vandg2](images/maps/map_vandg2.png) |
| `map_vandg3` | `+proj=vandg3` | [Van der Grinten III](https://proj.org/en/stable/operations/projections/vandg3.html) | ![map_vandg3](images/maps/map_vandg3.png) |
| `map_vandg4` | `+proj=vandg4` | [Van der Grinten IV](https://proj.org/en/stable/operations/projections/vandg4.html) | ![map_vandg4](images/maps/map_vandg4.png) |

---

### Interrupted

Interrupted projections break the map along selected meridians to reduce distortion in landmasses or oceans.

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_igh` | `+proj=igh` | [Goode Interrupted Homolosine](https://proj.org/en/stable/operations/projections/igh.html) — equal-area, interrupts over oceans | ![map_igh](images/maps/map_igh.png) |
| `map_igh_o` | `+proj=igh_o +lon_0=-160` | [Goode Interrupted Homolosine (Oceans)](https://proj.org/en/stable/operations/projections/igh_o.html) — interrupts over land, shows ocean continuity | ![map_igh_o](images/maps/map_igh_o.png) |
| `map_igh_shifted` | `+proj=igh +lon_0=90` | Goode Interrupted Homolosine (shifted 90° E) | ![map_igh_shifted](images/maps/map_igh_shifted.png) |
| `map_goode` | `+proj=goode` | [Goode Homolosine (uninterrupted)](https://proj.org/en/stable/operations/projections/goode.html) | ![map_goode](images/maps/map_goode.png) |

---

### Satellite and Perspective

| Test | PROJ String | Description | Output |
|------|------------|-------------|--------|
| `map_geos` | `+proj=geos +h=35785831 +lon_0=-60 +sweep=y` | [Geostationary Satellite View](https://proj.org/en/stable/operations/projections/geos.html) — view from 35786 km altitude over the Atlantic | ![map_geos](images/maps/map_geos.png) |
| `map_tpers` | `+proj=tpers +h=5500000 +lat_0=40` | [Tilted Perspective](https://proj.org/en/stable/operations/projections/tpers.html) — view from 5500 km altitude with tilt | ![map_tpers](images/maps/map_tpers.png) |
| `map_mil_os` | `+proj=mil_os` | [Miller Oblated Stereographic](https://proj.org/en/stable/operations/projections/mil_os.html) — used for European maps | ![map_mil_os](images/maps/map_mil_os.png) |

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

# Dali Test Examples

The Dali tests exercise the `/dali` endpoint directly, bypassing the WMS/WMTS layers.  Test inputs are `.get` files in [`test/input/`](../test/input/) and expected outputs in [`test/output/`](../test/output/).  Product JSON configurations are under [`test/dali/customers/`](../test/dali/customers/).

Each request URL is decomposed into a table showing every query parameter and its effect.  Layer-specific URL overrides follow the pattern `l.{qid}.{setting}` and view overrides follow `v{n}.{setting}`.

---

## Output Formats

Dali can produce SVG (default), PNG, PDF, GeoJSON, and KML from the same product definition.

### t2m_p — SVG (default)

**Input:** [`test/input/t2m_p.get`](../test/input/t2m_p.get)

```
GET /dali?customer=test&product=t2m_p&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `customer` | `test` | Customer directory |
| `product` | `t2m_p` | Product JSON: [`test/dali/customers/test/products/t2m_p.json`](../test/dali/customers/test/products/t2m_p.json) |
| `time` | `200808050300` | Valid time: 2008-08-05 03:00 UTC |

Renders Scandinavian temperature isobands and isolines from the `kap` producer in the data's native CRS (500×500 px).  The product uses a `defs` block to define SVG symbols and CSS styles.  Output is SVG (default when no `type` is specified).

**Output:**

![t2m_p](images/dali/t2m_p.png)

---

### png — PNG output

**Input:** [`test/input/png.get`](../test/input/png.get)

```
GET /dali?customer=test&product=t2m_p&type=png&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `customer` | `test` | Customer directory |
| `product` | `t2m_p` | Same product as above |
| `type` | `png` | Forces 8-bit palette PNG raster output |
| `time` | `200808050300` | Valid time: 2008-08-05 03:00 UTC |

Any product can be rasterised to PNG by supplying `type=png`.  Output is 500×500 px, 8-bit colour-mapped PNG (default ≤256 colours).

**Output:**

![png](images/dali/png.png)

---

### png_truecolor — 24-bit PNG

**Input:** [`test/input/png_truecolor.get`](../test/input/png_truecolor.get)

```
GET /dali?customer=test&product=t2m_p&type=png&time=200808050300&png.truecolor=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `type` | `png` | PNG output |
| `png.truecolor` | `1` | Forces 24-bit true-colour PNG instead of palette |

**Output:**

![png_truecolor](images/dali/png_truecolor.png)

---

### png_maxcolors_20 — Limited palette PNG

**Input:** [`test/input/png_maxcolors_20.get`](../test/input/png_maxcolors_20.get)

```
GET /dali?customer=test&product=t2m_p&type=png&time=200808050300&png.maxcolors=20 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `type` | `png` | PNG output |
| `png.maxcolors` | `20` | Caps the palette at 20 colours; reduces file size but introduces posterisation |

**Output:**

![png_maxcolors_20](images/dali/png_maxcolors_20.png)

---

### pdf — PDF output

**Input:** [`test/input/pdf.get`](../test/input/pdf.get)

```
GET /dali?customer=test&product=t2m_p&time=200808050300&type=pdf HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `type` | `pdf` | Produces a vector PDF document |

Output is a scalable PDF (no rasterisation).

---

### t2m_p_geojson — GeoJSON output

**Input:** [`test/input/t2m_p_geojson.get`](../test/input/t2m_p_geojson.get)

```
GET /dali?customer=test&product=t2m_p&time=200808050300&type=geojson HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `type` | `geojson` | Exports isoband/isoline geometries as GeoJSON |

Polygon and line features are reprojected to WGS84 (EPSG:4326) in the output.

---

### t2m_p_kml — KML output

**Input:** [`test/input/t2m_p_kml.get`](../test/input/t2m_p_kml.get)

```
GET /dali?customer=test&product=t2m_p&time=200808050300&type=kml HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `type` | `kml` | Exports isoband/isoline geometries as KML (Google Earth) |

Note: listed in `.testignore` — output precision can vary between RHEL 7 and RHEL 8 environments.

---

### autoclass — Automatic CSS class names

**Input:** [`test/input/autoclass.get`](../test/input/autoclass.get)

```
GET /dali?customer=test&product=autoclass&time=200808050300&type=svg HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `autoclass` | Product JSON: [`test/dali/customers/test/products/autoclass.json`](../test/dali/customers/test/products/autoclass.json) |
| `type` | `svg` | Forces SVG output |

The `autoclass` feature generates CSS class names automatically from isoband/isoline limit values using a template such as `"Temperature_{}_{}"`; the lower and upper limits substitute the placeholders (using `None` for open-ended bands).  This eliminates the need to assign class names manually in every isoband specification.

**Output:**

![autoclass](images/dali/autoclass.png)

---

### autoqid — Automatic query IDs

**Input:** [`test/input/autoqid.get`](../test/input/autoqid.get)

```
GET /dali?customer=test&product=autoqid&time=200808050300&format=svg HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `autoqid` | Product JSON: [`test/dali/customers/test/products/autoqid.json`](../test/dali/customers/test/products/autoqid.json) |
| `format` | `svg` | Alternative to `type=svg` for specifying SVG output |

The `autoqid` feature auto-generates `qid` values for each isoband/isoline from the band limits using a template such as `"temperature_{}_{}"`; the resulting `qid` can then be referenced in URL parameter overrides.  Unlike `autoclass` (which generates CSS class names), `autoqid` generates the query IDs used for URL-based overrides.

**Output:**

![autoqid](images/dali/autoqid.png)

---

### disable_svg — Conditionally disabled layer

**Input:** [`test/input/disable_svg.get`](../test/input/disable_svg.get)

```
GET /dali?customer=test&product=disable_svg&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `disable_svg` | Product JSON: [`test/dali/customers/test/products/disable_svg.json`](../test/dali/customers/test/products/disable_svg.json) |

A layer carrying `"disable": "svg"` is skipped entirely when rendering to SVG but rendered for PNG/PDF.  Useful for layers (e.g. pressure isolines) that are visually acceptable as vector but are too slow or heavy in SVG.

**Output:**

![disable_svg](images/dali/disable_svg.png)

---

### enable_png — Conditionally enabled layer

**Input:** [`test/input/enable_png.get`](../test/input/enable_png.get)

```
GET /dali?customer=test&product=enable_png&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `enable_png` | Product JSON: [`test/dali/customers/test/products/enable_png.json`](../test/dali/customers/test/products/enable_png.json) |

A layer carrying `"enable": ["png","pdf"]` is rendered only for those output formats and suppressed for SVG.  The product adds pressure isolines only for raster/PDF output.

**Output:**

![enable_png](images/dali/enable_png.png)

---

## Isoband and Isoline

### feelslike — FeelsLike parameter

**Input:** [`test/input/feelslike.get`](../test/input/feelslike.get)

```
GET /dali?customer=test&product=feelslike&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `feelslike` | Product JSON: [`test/dali/customers/test/products/feelslike.json`](../test/dali/customers/test/products/feelslike.json) |

Renders isobands and isolines of the `FeelsLike` apparent-temperature parameter from the `pal_skandinavia` producer, using the same colour scale as regular temperature.

**Output:**

![feelslike](images/dali/feelslike.png)

---

### precipitation — Precipitation isobands

**Input:** [`test/input/precipitation.get`](../test/input/precipitation.get)

```
GET /dali?customer=test&product=precipitation&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `precipitation` | Product JSON: [`test/dali/customers/test/products/precipitation.json`](../test/dali/customers/test/products/precipitation.json) |

Renders precipitation isobands and isolines from `pal_skandinavia` in the data's native CRS.

**Output:**

![precipitation](images/dali/precipitation.png)

---

### precipitation_warm — Precipitation warm season

**Input:** [`test/input/precipitation_warm.get`](../test/input/precipitation_warm.get)

```
GET /dali?customer=test&product=precipitation_warm&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `precipitation_warm` | Product JSON: [`test/dali/customers/test/products/precipitation_warm.json`](../test/dali/customers/test/products/precipitation_warm.json) |

Uses a warm-season precipitation colour scale with finer class resolution in the low-intensity range.

**Output:**

![precipitation_warm](images/dali/precipitation_warm.png)

---

### precipitation_minarea — Suppress small polygons (geographic area)

**Input:** [`test/input/precipitation_minarea.get`](../test/input/precipitation_minarea.get)

```
GET /dali?customer=test&product=precipitation&time=200808050300&l1.minarea=5000&l2.minarea=5000 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `l1.minarea` | `5000` | Drops isoband polygons smaller than 5000 m² for layer `l1` |
| `l2.minarea` | `5000` | Same for layer `l2` |

The `minarea` setting removes noise from scattered small polygons.  The default unit is m².

**Output:**

![precipitation_minarea](images/dali/precipitation_minarea.png)

---

### precipitation_minarea_px — Suppress small polygons (pixel area)

**Input:** [`test/input/precipitation_minarea_px.get`](../test/input/precipitation_minarea_px.get)

```
GET /dali?customer=test&product=precipitation&time=200808050300&l1.minarea=1000&l2.minarea=1000&l1.areaunit=px^2&l2.areaunit=px^2 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `l1.minarea` | `1000` | Minimum polygon area threshold for layer `l1` |
| `l1.areaunit` | `px^2` | Interprets the area threshold in pixels² (screen-space) instead of m² |

Using `areaunit=px^2` makes the filter resolution-independent: useful when the product may be rendered at different map scales.

**Output:**

![precipitation_minarea_px](images/dali/precipitation_minarea_px.png)

---

### radar — Radar precipitation

**Input:** [`test/input/radar.get`](../test/input/radar.get)

```
GET /dali?customer=test&product=radar&time=20130910T1000 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `radar` | Product JSON: [`test/dali/customers/test/products/radar.json`](../test/dali/customers/test/products/radar.json) |
| `time` | `20130910T1000` | Valid time: 2013-09-10 10:00 UTC |

Renders radar-derived precipitation rate (`PrecipitationRate`) from the `tutka_suomi_rr` producer with a blue sea background and country-fill.  Demonstrates isoband rendering on a 500×800 px product in the data's native CRS.

**Output:**

![radar](images/dali/radar.png)

---

### tmax_smooth — Smoothed maximum temperature

**Input:** [`test/input/tmax_smooth.get`](../test/input/tmax_smooth.get)

```
GET /dali?customer=test&product=tmax_smooth&time=200101011200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `tmax_smooth` | Product JSON: [`test/dali/customers/test/products/tmax_smooth.json`](../test/dali/customers/test/products/tmax_smooth.json) |

Applies a contouring smoother (`"smoother": {"degree": 1, "size": 1}`) to maximum temperature before rendering isobands; reduces jagged edges in coarse-resolution data.

**Output:**

![tmax_smooth](images/dali/tmax_smooth.png)

---

### t2m_p_smooth — Smoothed temperature

**Input:** [`test/input/t2m_p_smooth.get`](../test/input/t2m_p_smooth.get)

```
GET /dali?customer=test&product=t2m_p_smooth&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_p_smooth` | Product JSON: [`test/dali/customers/test/products/t2m_p_smooth.json`](../test/dali/customers/test/products/t2m_p_smooth.json) |

Same as `t2m_p` but with a stronger smoother applied to the temperature field, producing fewer and smoother contours.

**Output:**

![t2m_p_smooth](images/dali/t2m_p_smooth.png)

---

### crs_bbox — CRS bounding box handling

**Input:** [`test/input/crs_bbox.get`](../test/input/crs_bbox.get)

```
GET /dali?customer=test&product=crs_bbox&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `crs_bbox` | Product JSON: [`test/dali/customers/test/products/crs_bbox.json`](../test/dali/customers/test/products/crs_bbox.json) |

Tests that the bounding box specified in the product is interpreted correctly in the product's CRS.  The product uses `"crs": "data"` with a smoother applied.

**Output:**

![crs_bbox](images/dali/crs_bbox.png)

---

### t2m_p_noqid — Layers without qid

**Input:** [`test/input/t2m_p_noqid.get`](../test/input/t2m_p_noqid.get)

```
GET /dali?customer=test&product=t2m_p_noqid&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_p_noqid` | Product JSON: [`test/dali/customers/test/products/t2m_p_noqid.json`](../test/dali/customers/test/products/t2m_p_noqid.json) |

Like `t2m_p` but the layers omit `qid` fields.  Without a `qid`, layers cannot be overridden via URL parameters but the product still renders correctly.

**Output:**

![t2m_p_noqid](images/dali/t2m_p_noqid.png)

---

## Isoband Labels

Labels can be placed along isolines using the dedicated `isolabel` layer type, or directly in the isoband definition with `t2m_isoband_labels`.

### t2m_isoband_labels — Labels embedded in isoband layer

**Input:** [`test/input/t2m_isoband_labels.get`](../test/input/t2m_isoband_labels.get)

```
GET /dali?customer=test&product=t2m_isoband_labels&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_isoband_labels` | Product JSON: [`test/dali/customers/test/products/t2m_isoband_labels.json`](../test/dali/customers/test/products/t2m_isoband_labels.json) |

Demonstrates value labels embedded directly in the isoband layer specification.  Labels are drawn along the contour lines at a configured spacing.

**Output:**

![t2m_isoband_labels](images/dali/t2m_isoband_labels.png)

---

### isolabel — Basic isolabels

**Input:** [`test/input/isolabel.get`](../test/input/isolabel.get)

```
GET /dali?customer=test&product=isolabel&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `isolabel` | Product JSON: [`test/dali/customers/test/products/isolabel.json`](../test/dali/customers/test/products/isolabel.json) |

Uses the dedicated `isolabel` layer type to draw contour labels at a fixed spacing along isoline paths.

**Output:**

![isolabel](images/dali/isolabel.png)

---

### isolabel_angles — Angle-following labels

**Input:** [`test/input/isolabel_angles.get`](../test/input/isolabel_angles.get)

```
GET /dali?customer=test&product=isolabel_angles&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `isolabel_angles` | Product JSON: [`test/dali/customers/test/products/isolabel_angles.json`](../test/dali/customers/test/products/isolabel_angles.json) |

Labels are rotated to follow the tangent direction of the isoline.

**Output:**

![isolabel_angles](images/dali/isolabel_angles.png)

---

### isolabel_horizontal — Horizontal labels only

**Input:** [`test/input/isolabel_horizontal.get`](../test/input/isolabel_horizontal.get)

```
GET /dali?customer=test&product=isolabel_horizontal&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `isolabel_horizontal` | Product JSON: [`test/dali/customers/test/products/isolabel_horizontal.json`](../test/dali/customers/test/products/isolabel_horizontal.json) |

All labels are rendered horizontally regardless of isoline orientation; suitable when readability is prioritised over alignment with the contour.

**Output:**

![isolabel_horizontal](images/dali/isolabel_horizontal.png)

---

### isolabel_cut — Cut-through labels

**Input:** [`test/input/isolabel_cut.get`](../test/input/isolabel_cut.get)

```
GET /dali?customer=test&product=isolabel_cut&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `isolabel_cut` | Product JSON: [`test/dali/customers/test/products/isolabel_cut.json`](../test/dali/customers/test/products/isolabel_cut.json) |

The isoline is cut at each label position so the label appears embedded in the line rather than on top of it.

**Output:**

![isolabel_cut](images/dali/isolabel_cut.png)

---

### isolabel_styles — Multiple label styles

**Input:** [`test/input/isolabel_styles.get`](../test/input/isolabel_styles.get)

```
GET /dali?customer=test&product=isolabel_styles&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `isolabel_styles` | Product JSON: [`test/dali/customers/test/products/isolabel_styles.json`](../test/dali/customers/test/products/isolabel_styles.json) |

Demonstrates per-isoline label styling: each contour level can have its own font size, colour, and background.

**Output:**

![isolabel_styles](images/dali/isolabel_styles.png)

---

## SVG Customisation

### t2m_p_shadow — SVG drop shadow

**Input:** [`test/input/t2m_p_shadow.get`](../test/input/t2m_p_shadow.get)

```
GET /dali?customer=test&product=t2m_p_shadow&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_p_shadow` | Product JSON: [`test/dali/customers/test/products/t2m_p_shadow.json`](../test/dali/customers/test/products/t2m_p_shadow.json) |

Adds an SVG `<filter>` drop-shadow (`feDropShadow`) to the product's root element via the top-level `"attributes": {"filter": "url(#shadow)"}` setting.

**Output:**

![t2m_p_shadow](images/dali/t2m_p_shadow.png)

---

### t2m_p_display_none — Hidden layer with optimizesize

**Input:** [`test/input/t2m_p_display_none.get`](../test/input/t2m_p_display_none.get)

```
GET /dali?customer=test&product=t2m_p&time=200808050300&l3.attributes.display=none&optimizesize=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `l3.attributes.display` | `none` | Sets `display=none` on layer `l3` via URL override, making it invisible |
| `optimizesize` | `1` | When set, layers with `display=none` are omitted from the output entirely (reducing file size) |

The `optimizesize` flag strips out hidden layers rather than leaving them as invisible elements in the SVG.

**Output:**

![t2m_p_display_none](images/dali/t2m_p_display_none.png)

---

### t2m_pattern — SVG pattern fill

**Input:** [`test/input/t2m_pattern.get`](../test/input/t2m_pattern.get)

```
GET /dali?customer=test&product=t2m_pattern&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_pattern` | Product JSON: [`test/dali/customers/test/products/t2m_pattern.json`](../test/dali/customers/test/products/t2m_pattern.json) |

Uses SVG `<pattern>` elements defined in the `defs` section to fill isoband polygons with a hatching or repeat pattern rather than a solid colour.

**Output:**

![t2m_pattern](images/dali/t2m_pattern.png)

---

### defs_circle — SVG defs with reusable shapes

**Input:** [`test/input/defs_circle.get`](../test/input/defs_circle.get)

```
GET /dali?customer=test&product=defs_circle&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `defs_circle` | Product JSON: [`test/dali/customers/test/products/defs_circle.json`](../test/dali/customers/test/products/defs_circle.json) |

Shows how `defs.layers` can define reusable SVG elements (filters, named circles, clip paths, text nodes) that are referenced later via `xlink:href`.

**Output:**

![defs_circle](images/dali/defs_circle.png)

---

### clip — Explicit clipPath layer

**Input:** [`test/input/clip.get`](../test/input/clip.get)

```
GET /dali?customer=test&product=clip&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `clip` | Product JSON: [`test/dali/customers/test/products/clip.json`](../test/dali/customers/test/products/clip.json) |

Uses a `tag: "clipPath"` layer containing a `tag: "circle"` to create an SVG `<clipPath>` element inline.  A `tag: "g"` wrapper layer with `clip-path: url(#mycircle)` then applies it to the isoband layers, while the product-level `filter: url(#shadow)` adds a drop shadow.

**Output:**

![clip](images/dali/clip.png)

---

### circle — Circular clip with isobands

**Input:** [`test/input/circle.get`](../test/input/circle.get)

```
GET /dali?customer=test&product=circle&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `circle` | Product JSON: [`test/dali/customers/test/products/circle.json`](../test/dali/customers/test/products/circle.json) |
| `time` | `200808050300` | Valid time: 2008-08-05 03:00 UTC |

Projects Scandinavian temperature data using the data's own native CRS, renders temperature isobands and isolines, and clips the output with an SVG `<circle>` element centred at (250, 250) with radius 250.

**Output:**

![circle](images/dali/circle.png)

---

## Clipping to Geographic Boundaries

### t2m_inside_finland — Clip to country polygon

**Input:** [`test/input/t2m_inside_finland.get`](../test/input/t2m_inside_finland.get)

```
GET /dali?customer=test&product=t2m_inside_finland&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_inside_finland` | Product JSON: [`test/dali/customers/test/products/t2m_inside_finland.json`](../test/dali/customers/test/products/t2m_inside_finland.json) |

Uses `"inside"` on the isoband layer referencing a PostGIS query for the Finland border (`iso_a2='FI'`) to clip the rendered isobands to inside Finland.

**Output:**

![t2m_inside_finland](images/dali/t2m_inside_finland.png)

---

### t2m_inside_simplified_finland — Clip to simplified country polygon

**Input:** [`test/input/t2m_inside_simplified_finland.get`](../test/input/t2m_inside_simplified_finland.get)

```
GET /dali?customer=test&product=t2m_inside_simplified_finland&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_inside_simplified_finland` | Product JSON: [`test/dali/customers/test/products/t2m_inside_simplified_finland.json`](../test/dali/customers/test/products/t2m_inside_simplified_finland.json) |

Same as above but uses a `mindistance` tolerance to simplify the clip polygon, producing a smoother (less precise) boundary and faster rendering.

**Output:**

![t2m_inside_simplified_finland](images/dali/t2m_inside_simplified_finland.png)

---

### t2m_outside_finland — Clip to outside country

**Input:** [`test/input/t2m_outside_finland.get`](../test/input/t2m_outside_finland.get)

```
GET /dali?customer=test&product=t2m_outside_finland&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_outside_finland` | Product JSON: [`test/dali/customers/test/products/t2m_outside_finland.json`](../test/dali/customers/test/products/t2m_outside_finland.json) |

Uses `"outside"` to clip isobands to the area outside Finland — the inverse of `t2m_inside_finland`.

**Output:**

![t2m_outside_finland](images/dali/t2m_outside_finland.png)

---

### t2m_inside_rain — Clip isobands to rain area

**Input:** [`test/input/t2m_inside_rain.get`](../test/input/t2m_inside_rain.get)

```
GET /dali?customer=test&product=t2m_inside_rain&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_inside_rain` | Product JSON: [`test/dali/customers/test/products/t2m_inside_rain.json`](../test/dali/customers/test/products/t2m_inside_rain.json) |

Clips temperature isobands to areas where precipitation exceeds 0 mm/h (the rain area).  The `inside` polygon is derived dynamically from another data layer rather than from a static geographic boundary.

**Output:**

![t2m_inside_rain](images/dali/t2m_inside_rain.png)

---

### t2m_lines_inside_rain — Isolines clipped to rain area

**Input:** [`test/input/t2m_lines_inside_rain.get`](../test/input/t2m_lines_inside_rain.get)

```
GET /dali?customer=test&product=t2m_lines_inside_rain&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_lines_inside_rain` | Product JSON: [`test/dali/customers/test/products/t2m_lines_inside_rain.json`](../test/dali/customers/test/products/t2m_lines_inside_rain.json) |

Temperature isolines (not isobands) are clipped to the rain area.  Shows that the `inside` clipping mechanism works equally for line layers.

**Output:**

![t2m_lines_inside_rain](images/dali/t2m_lines_inside_rain.png)

---

### frost_inside_simplified_finland — Frost probability inside Finland

**Input:** [`test/input/frost_inside_simplified_finland.get`](../test/input/frost_inside_simplified_finland.get)

```
GET /dali?customer=test&product=frost_inside_simplified_finland&time=201604140000 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `frost_inside_simplified_finland` | Product JSON: [`test/dali/customers/test/products/frost_inside_simplified_finland.json`](../test/dali/customers/test/products/frost_inside_simplified_finland.json) |
| `time` | `201604140000` | Valid time: 2016-04-14 00:00 UTC |

Renders `FrostProbability` isobands from the `ground` producer at 1200×1200 px in EPSG:4326.  Uses `"inside": "ref:refs.finland"` where the `refs.finland` entry queries the Natural Earth countries table with `iso_a2 IN ('FI','AX')` and a `mindistance` simplification tolerance.

**Output:**

![frost_inside_simplified_finland](images/dali/frost_inside_simplified_finland.png)

---

## Pressure Level and Elevation Selection

### t2m_level_* — Named pressure levels

**Input files:** [`test/input/t2m_level_1000.get`](../test/input/t2m_level_1000.get), [`test/input/t2m_level_850.get`](../test/input/t2m_level_850.get), [`test/input/t2m_level_300.get`](../test/input/t2m_level_300.get)

```
GET /dali?customer=test&product=t2m_pressurelevel&time=20080909T1200&level=1000 HTTP/1.0
GET /dali?customer=test&product=t2m_pressurelevel&time=20080909T1200&level=850  HTTP/1.0
GET /dali?customer=test&product=t2m_pressurelevel&time=20080909T1200&level=300  HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_pressurelevel` | Product JSON: [`test/dali/customers/test/products/t2m_pressurelevel.json`](../test/dali/customers/test/products/t2m_pressurelevel.json) |
| `time` | `20080909T1200` | Valid time: 2008-09-09 12:00 UTC |
| `level` | `1000` / `850` / `300` | Selects the pressure level in hPa from the `ecmwf_skandinavia_painepinta` producer |

The `level` parameter selects a pressure level slice; the producer must have data for that level.

| Level | Output |
|-------|--------|
| 1000 hPa | ![t2m_level_1000](images/dali/t2m_level_1000.png) |
| 850 hPa | ![t2m_level_850](images/dali/t2m_level_850.png) |
| 300 hPa | ![t2m_level_300](images/dali/t2m_level_300.png) |

---

### t2m_elevation_* — Elevation in metres

**Input files:** [`test/input/t2m_elevation_1000.get`](../test/input/t2m_elevation_1000.get), [`test/input/t2m_elevation_850.get`](../test/input/t2m_elevation_850.get), [`test/input/t2m_elevation_300.get`](../test/input/t2m_elevation_300.get)

```
GET /dali?customer=test&product=t2m_pressurelevel&time=20080909T1200&elevation=1000 HTTP/1.0
GET /dali?customer=test&product=t2m_pressurelevel&time=20080909T1200&elevation=850  HTTP/1.0
GET /dali?customer=test&product=t2m_pressurelevel&time=20080909T1200&elevation=300  HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `elevation` | `1000` / `850` / `300` | Selects the level using the `elevation` parameter (in metres or hPa depending on data convention) |

The `elevation` parameter is an alternative to `level`; which one is applicable depends on the data source's level type convention.

| Elevation | Output |
|-----------|--------|
| 1000 m | ![t2m_elevation_1000](images/dali/t2m_elevation_1000.png) |
| 850 m | ![t2m_elevation_850](images/dali/t2m_elevation_850.png) |
| 300 m | ![t2m_elevation_300](images/dali/t2m_elevation_300.png) |

---

### high_resolution — High resolution rendering

**Input:** [`test/input/high_resolution.get`](../test/input/high_resolution.get)

```
GET /dali?customer=test&product=high_resolution&time=202006051200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `high_resolution` | Product JSON: [`test/dali/customers/test/products/high_resolution.json`](../test/dali/customers/test/products/high_resolution.json) |
| `time` | `202006051200` | Valid time: 2020-06-05 12:00 UTC |

Tests rendering of a high-resolution model grid (e.g. HARMONIE/AROME).  Listed in `.testignore` as a debugging test case with non-deterministic output in CI.

*No reference output image available.*

---

## Multiple Views and Side-by-Side Products

### t2m_twice — Two views side by side

**Input:** [`test/input/t2m_twice.get`](../test/input/t2m_twice.get)

```
GET /dali?customer=test&product=t2m_twice&time=200808050300&v2.time_offset=1440 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `t2m_twice` | Product JSON: [`test/dali/customers/test/products/t2m_twice.json`](../test/dali/customers/test/products/t2m_twice.json) |
| `v2.time_offset` | `1440` | Advances the valid time of view 2 by 1440 minutes (24 hours) |

The product defines two 500×500 px views (`v1`, `v2`) side by side in a 1030×520 canvas.  Each view includes temperature isobands, isolines, and a time label.  The URL overrides `v2.time_offset` to show T+24 h in the second panel.

**Output:**

![t2m_twice](images/dali/t2m_twice.png)

---

### t2m_twice_altered — Transformed second view

**Input:** [`test/input/t2m_twice_altered.get`](../test/input/t2m_twice_altered.get)

```
GET /dali?customer=test&product=t2m_twice&time=200808050300
    &v2.time_offset=1440
    &v2.attributes.transform=translate%28500,1%29+rotate%2830%29+scale%280.75%29
    &v1.attributes.filter=url%28#shadow%29
    &v2.attributes.filter=url%28#shadow%29 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `v2.attributes.transform` | `translate(500,1) rotate(30) scale(0.75)` | Applies an SVG transform to view 2: shifted, rotated 30°, scaled 75% |
| `v1.attributes.filter` | `url(#shadow)` | Adds a drop-shadow filter to view 1 |
| `v2.attributes.filter` | `url(#shadow)` | Adds a drop-shadow filter to view 2 |

Demonstrates that SVG attributes (including `transform` and `filter`) can be set on any view via URL overrides.  The URL-encoded values `%28` = `(`, `%29` = `)`.

**Output:**

![t2m_twice_altered](images/dali/t2m_twice_altered.png)

---

### t2m_twice_margins — Views with margins (unclipped)

**Input:** [`test/input/t2m_twice_margins.get`](../test/input/t2m_twice_margins.get)

```
GET /dali?customer=test&product=t2m_twice&time=200808050300
    &v2.time_offset=1440
    &v1.clip=0&v2.clip=0
    &v1.margin=5&v2.margin=10
    &projection.place=Helsinki&projection.resolution=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `v1.clip` | `0` | Disables clipping for view 1 (data may extend beyond the view boundary) |
| `v2.clip` | `0` | Disables clipping for view 2 |
| `v1.margin` | `5` | Adds a 5-pixel margin around view 1's data extent |
| `v2.margin` | `10` | Adds a 10-pixel margin around view 2's data extent |
| `projection.place` | `Helsinki` | Centres the projection on Helsinki |
| `projection.resolution` | `1` | Sets projection resolution to 1 km/px |

**Output:**

![t2m_twice_margins](images/dali/t2m_twice_margins.png)

---

### t2m_twice_margins_clipped — Views with margins (clipped)

**Input:** [`test/input/t2m_twice_margins_clipped.get`](../test/input/t2m_twice_margins_clipped.get)

```
GET /dali?customer=test&product=t2m_twice&time=200808050300
    &v2.time_offset=1440
    &v1.clip=1&v2.clip=1
    &v1.margin=5&v2.margin=10
    &projection.place=Helsinki&projection.resolution=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `v1.clip` | `1` | Enables clipping for view 1 |
| `v2.clip` | `1` | Enables clipping for view 2 |

Same as `t2m_twice_margins` but clipping is re-enabled.  Layers are clipped to the view boundary even though a margin is specified.

**Output:**

![t2m_twice_margins_clipped](images/dali/t2m_twice_margins_clipped.png)

---

### resolution — View-level projection resolution

**Input:** [`test/input/resolution.get`](../test/input/resolution.get)

```
GET /dali?customer=test&product=resolution&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `resolution` | Product JSON: [`test/dali/customers/test/products/resolution.json`](../test/dali/customers/test/products/resolution.json) |

Two 500×500 views centred on Finland are placed side by side, each with a different `resolution` setting in their `projection` block (2.5 and 1.5 km/px).  Map layers use `minresolution` and `maxresolution` to select detail levels: coarse data is shown in black for coarse resolutions, fine data in red for fine resolutions.

**Output:**

![resolution](images/dali/resolution.png)

---

### ely_overlay — Multi-customer overlay

**Input:** [`test/input/ely_overlay.get`](../test/input/ely_overlay.get)

```
GET /dali?customer=ely&product=temperatureoverlay&time=200808080000&type=svg HTTP/1.1
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `customer` | `ely` | Selects the `ely` customer directory (separate from `test`) |
| `product` | `temperatureoverlay` | Product JSON: [`test/dali/customers/ely/products/temperatureoverlay.json`](../test/dali/customers/ely/products/temperatureoverlay.json) |
| `type` | `svg` | SVG output |

A second customer (`ely`) demonstrating that multiple customer configurations can coexist.  This overlay product uses a `number` layer with a masked legend background.

**Output:**

![ely_overlay](images/dali/ely_overlay.png)

---

## Time Layer and Timezone

### timelayer — Time annotation

**Input:** [`test/input/timelayer.get`](../test/input/timelayer.get)

```
GET /dali?customer=test&product=timelayer&time=200808051200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `timelayer` | Product JSON: [`test/dali/customers/test/products/timelayer.json`](../test/dali/customers/test/products/timelayer.json) |
| `time` | `200808051200` | Valid time: 2008-08-05 12:00 UTC |

The `time` layer renders the product's valid time as formatted text (e.g. `"%Y-%m-%d %H:%M"`) at a specified position in the SVG.  The product uses `"timestamp": "validtime"` and `"timezone": "UTC"`.

**Output:**

![timelayer](images/dali/timelayer.png)

---

### timezone — Time in local timezone

**Input:** [`test/input/timezone.get`](../test/input/timezone.get)

```
GET /dali?customer=test&product=timelayer&time=200808051500&tz=Europe/Helsinki HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `time` | `200808051500` | Valid time: 2008-08-05 15:00 UTC |
| `tz` | `Europe/Helsinki` | Overrides the display timezone for the time layer; 15:00 UTC → 18:00 EEST |

The `tz` URL parameter overrides the product's timezone for all `time` layer annotations in the response.

**Output:**

![timezone](images/dali/timezone.png)

---

## Observation Numbers

### temperature_fmisid — Station temperatures by FMI ID

**Input:** [`test/input/temperature_fmisid.get`](../test/input/temperature_fmisid.get)

```
GET /dali?customer=test&product=temperature_fmisid&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `temperature_fmisid` | Product JSON: [`test/dali/customers/test/products/temperature_fmisid.json`](../test/dali/customers/test/products/temperature_fmisid.json) |
| `time` | `200808050300` | Valid time: 2008-08-05 03:00 UTC |

Plots temperature observations at a hand-picked list of FMI station IDs (`fmisid`).  Individual stations may override the default label offset (`dx`, `dy`).  Background: white fill + Natural Earth country borders.

**Output:**

![temperature_fmisid](images/dali/temperature_fmisid.png)

---

### opendata_temperature_fmisid — Opendata station temperatures

**Input:** [`test/input/opendata_temperature_fmisid.get`](../test/input/opendata_temperature_fmisid.get)

```
GET /dali?customer=test&product=opendata_temperature_fmisid&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_temperature_fmisid` | Product JSON: [`test/dali/customers/test/products/opendata_temperature_fmisid.json`](../test/dali/customers/test/products/opendata_temperature_fmisid.json) |
| `time` | `20130805T1500` | Valid time: 2013-08-05 15:00 UTC (ISO 8601 compact format) |

Same concept as `temperature_fmisid` but using the `opendata` producer from the Finnish Meteorological Institute.

**Output:**

![opendata_temperature_fmisid](images/dali/opendata_temperature_fmisid.png)

---

### opendata_temperature_fmisid_shift — Station numbers with label offset

**Input:** [`test/input/opendata_temperature_fmisid_shift.get`](../test/input/opendata_temperature_fmisid_shift.get)

```
GET /dali?customer=test&product=opendata_temperature_fmisid_shift&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_temperature_fmisid_shift` | Product JSON: [`test/dali/customers/test/products/opendata_temperature_fmisid_shift.json`](../test/dali/customers/test/products/opendata_temperature_fmisid_shift.json) |

Station labels are systematically shifted (`dx`, `dy`) to reduce overlap with symbols.

**Output:**

![opendata_temperature_fmisid_shift](images/dali/opendata_temperature_fmisid_shift.png)

---

### opendata_temperature_grid — Grid-spaced observation numbers

**Input:** [`test/input/opendata_temperature_grid.get`](../test/input/opendata_temperature_grid.get)

```
GET /dali?customer=test&product=opendata_temperature_grid&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_temperature_grid` | Product JSON: [`test/dali/customers/test/products/opendata_temperature_grid.json`](../test/dali/customers/test/products/opendata_temperature_grid.json) |

Uses `"positions": {"layout": "grid", "dx": 30, "dy": 30}` to place observation values at regular pixel intervals, thinning dense station networks.

**Output:**

![opendata_temperature_grid](images/dali/opendata_temperature_grid.png)

---

### opendata_temperature_keyword — Keyword-selected stations

**Input:** [`test/input/opendata_temperature_keyword.get`](../test/input/opendata_temperature_keyword.get)

```
GET /dali?customer=test&product=opendata_temperature_keyword&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_temperature_keyword` | Product JSON: [`test/dali/customers/test/products/opendata_temperature_keyword.json`](../test/dali/customers/test/products/opendata_temperature_keyword.json) |

Selects stations by keyword (administrative classification such as county or municipality) rather than by explicit FMI station IDs.

**Output:**

![opendata_temperature_keyword](images/dali/opendata_temperature_keyword.png)

---

### ecmwf_data_numbers — ECMWF grid point values

**Input:** [`test/input/ecmwf_data_numbers.get`](../test/input/ecmwf_data_numbers.get)

```
GET /dali?customer=test&product=ecmwf_data_numbers&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `ecmwf_data_numbers` | Product JSON: [`test/dali/customers/test/products/ecmwf_data_numbers.json`](../test/dali/customers/test/products/ecmwf_data_numbers.json) |
| `time` | `201503131200` | Valid time: 2015-03-13 12:00 UTC |

Plots temperature values at regular grid intervals from the ECMWF surface analysis (`ecmwf_maailma_pinta`) over Europe.

**Output:**

![ecmwf_data_numbers](images/dali/ecmwf_data_numbers.png)

---

### ecmwfpoint_data_numbers — ECMWF point values

**Input:** [`test/input/ecmwfpoint_data_numbers.get`](../test/input/ecmwfpoint_data_numbers.get)

```
GET /dali?customer=test&product=ecmwfpoint_data_numbers&time=200808051200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `ecmwfpoint_data_numbers` | Product JSON: [`test/dali/customers/test/products/ecmwfpoint_data_numbers.json`](../test/dali/customers/test/products/ecmwfpoint_data_numbers.json) |
| `time` | `200808051200` | Valid time: 2008-08-05 12:00 UTC |

Plots ECMWF model values at specific geographic point locations rather than a regular grid.

**Output:**

![ecmwfpoint_data_numbers](images/dali/ecmwfpoint_data_numbers.png)

---

### ecmwf_world_numbers — ECMWF global grid values

**Input:** [`test/input/ecmwf_world_numbers.get`](../test/input/ecmwf_world_numbers.get)

```
GET /dali?customer=test&product=ecmwf_world_numbers&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `ecmwf_world_numbers` | Product JSON: [`test/dali/customers/test/products/ecmwf_world_numbers.json`](../test/dali/customers/test/products/ecmwf_world_numbers.json) |

Plots ECMWF global surface temperature values at grid intervals on a world map.

**Output:**

![ecmwf_world_numbers](images/dali/ecmwf_world_numbers.png)

---

### aviation_numbers — Aviation temperature numbers

**Input:** [`test/input/aviation_numbers.get`](../test/input/aviation_numbers.get)

```
GET /dali?customer=test&product=aviation_numbers&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `aviation_numbers` | Product JSON: [`test/dali/customers/test/products/aviation_numbers.json`](../test/dali/customers/test/products/aviation_numbers.json) |
| `time` | `201503131200` | Valid time: 2015-03-13 12:00 UTC |

Renders ECMWF temperature isobands over the North Atlantic aviation area (EPSG:4326, 62.5–75°N, 15°W–37.5°E) and overlays integer temperature values at a 20×20 px grid with alternating 10-pixel row offsets (`ddx`).

**Output:**

![aviation_numbers](images/dali/aviation_numbers.png)

---

### aviation_numbers_multiple5 — Rounded-to-5 numbers

**Input:** [`test/input/aviation_numbers_multiple5.get`](../test/input/aviation_numbers_multiple5.get)

```
GET /dali?customer=test&product=aviation_numbers_multiple5&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `aviation_numbers_multiple5` | Product JSON: [`test/dali/customers/test/products/aviation_numbers_multiple5.json`](../test/dali/customers/test/products/aviation_numbers_multiple5.json) |

Like `aviation_numbers` but values are rounded to the nearest 5 (ICAO convention: temperatures reported in multiples of 5°C on SIGWX charts).

**Output:**

![aviation_numbers_multiple5](images/dali/aviation_numbers_multiple5.png)

---

### synop_numbers — SYNOP observation values

**Input:** [`test/input/synop_numbers.get`](../test/input/synop_numbers.get)

```
GET /dali?customer=test&product=synop_numbers&time=20081201T1200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `synop_numbers` | Product JSON: [`test/dali/customers/test/products/synop_numbers.json`](../test/dali/customers/test/products/synop_numbers.json) |
| `time` | `20081201T1200` | Valid time: 2008-12-01 12:00 UTC |

Plots SYNOP observation values from the `pal_skandinavia` producer at station locations.

**Output:**

![synop_numbers](images/dali/synop_numbers.png)

---

### monitoring — Health monitoring text

**Input:** [`test/input/monitoring.get`](../test/input/monitoring.get)

```
GET /dali?customer=test&product=monitoring HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `monitoring` | Product JSON: [`test/dali/customers/test/products/monitoring.json`](../test/dali/customers/test/products/monitoring.json) |

A minimal product that outputs a plain-text "OK" health-check response without requiring any weather data.  Used for liveness/readiness probes.

**Output:**

![monitoring](images/dali/monitoring.png)

---

## Observation Positions and Keyword Selection

### latlon_positions — Lat/lon coordinate list

**Input:** [`test/input/latlon_positions.get`](../test/input/latlon_positions.get)

```
GET /dali?customer=test&product=latlon_positions&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `latlon_positions` | Product JSON: [`test/dali/customers/test/products/latlon_positions.json`](../test/dali/customers/test/products/latlon_positions.json) |

Plots observation values at positions specified as explicit latitude/longitude coordinate pairs in the product JSON.

**Output:**

![latlon_positions](images/dali/latlon_positions.png)

---

### latlon_positions_shifted — Positions with per-station label offset

**Input:** [`test/input/latlon_positions_shifted.get`](../test/input/latlon_positions_shifted.get)

```
GET /dali?customer=test&product=latlon_positions_shifted&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `latlon_positions_shifted` | Product JSON: [`test/dali/customers/test/products/latlon_positions_shifted.json`](../test/dali/customers/test/products/latlon_positions_shifted.json) |

Each coordinate entry includes an individual `dx`/`dy` offset for its label, allowing crowded stations to be legible without overlapping.

**Output:**

![latlon_positions_shifted](images/dali/latlon_positions_shifted.png)

---

### keyword — Keyword-based station selection

**Input:** [`test/input/keyword.get`](../test/input/keyword.get)

```
GET /dali?customer=test&product=keyword&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `keyword` | Product JSON: [`test/dali/customers/test/products/keyword.json`](../test/dali/customers/test/products/keyword.json) |

Selects stations using a `keyword` field (e.g. a municipality or region name) instead of numeric IDs.  The server resolves the keyword to matching stations.

**Output:**

![keyword](images/dali/keyword.png)

---

### keyword_shifted — Keyword stations with shifted labels

**Input:** [`test/input/keyword_shifted.get`](../test/input/keyword_shifted.get)

```
GET /dali?customer=test&product=keyword_shifted&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `keyword_shifted` | Product JSON: [`test/dali/customers/test/products/keyword_shifted.json`](../test/dali/customers/test/products/keyword_shifted.json) |

Same as `keyword` but with per-station label offsets (`dx`, `dy`) defined for each keyword entry.

**Output:**

![keyword_shifted](images/dali/keyword_shifted.png)

---

## Weather Symbols

### opendata_temperature_symbols — SmartMet symbols

**Input:** [`test/input/opendata_temperature_symbols.get`](../test/input/opendata_temperature_symbols.get)

```
GET /dali?customer=test&product=opendata_temperature_symbols&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_temperature_symbols` | Product JSON: [`test/dali/customers/test/products/opendata_temperature_symbols.json`](../test/dali/customers/test/products/opendata_temperature_symbols.json) |
| `time` | `20130805T1500` | Valid time: 2013-08-05 15:00 UTC |

Plots SmartMet weather symbols at station locations combined with temperature numbers.

**Output:**

![opendata_temperature_symbols](images/dali/opendata_temperature_symbols.png)

---

### opendata_temperature_symbols_oddmitime — Odd-minute valid time

**Input:** [`test/input/opendata_temperature_symbols_oddmitime.get`](../test/input/opendata_temperature_symbols_oddmitime.get)

```
GET /dali?customer=test&product=opendata_temperature_symbols&time=20130805T1502 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `time` | `20130805T1502` | Valid time: 15:02 UTC — not on the hour; tests nearest-time matching |

Tests that the system correctly snaps to the nearest available observation when the requested time is between observation steps.

**Output:**

![opendata_temperature_symbols_oddmitime](images/dali/opendata_temperature_symbols_oddmitime.png)

---

### opendata_temperature_symbols_oddmitime_interval_back — Time interval backward

**Input:** [`test/input/opendata_temperature_symbols_oddmitime_interval_back.get`](../test/input/opendata_temperature_symbols_oddmitime_interval_back.get)

```
GET /dali?customer=test&product=opendata_temperature_symbols&time=20130805T1502&interval_start=2 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `time` | `20130805T1502` | Non-round valid time |
| `interval_start` | `2` | Accept data up to 2 minutes before the requested time |

The `interval_start` parameter widens the time-matching window backward, useful for observations that arrive with a slight delay.

**Output:**

![opendata_temperature_symbols_oddmitime_interval_back](images/dali/opendata_temperature_symbols_oddmitime_interval_back.png)

---

### opendata_temperature_symbols_oddmitime_interval_forward — Time interval forward

**Input:** [`test/input/opendata_temperature_symbols_oddmitime_interval_forward.get`](../test/input/opendata_temperature_symbols_oddmitime_interval_forward.get)

```
GET /dali?customer=test&product=opendata_temperature_symbols&time=20130805T1455&interval_end=5 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `time` | `20130805T1455` | Non-round valid time: 5 minutes before 15:00 |
| `interval_end` | `5` | Accept data up to 5 minutes after the requested time |

The `interval_end` parameter widens the window forward, allowing a request before the hour to still display the on-the-hour data.

**Output:**

![opendata_temperature_symbols_oddmitime_interval_forward](images/dali/opendata_temperature_symbols_oddmitime_interval_forward.png)

---

### smartsymbol — SmartMet numeric symbol codes

**Input:** [`test/input/smartsymbol.get`](../test/input/smartsymbol.get)

```
GET /dali?customer=test&product=smartsymbol&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `smartsymbol` | Product JSON: [`test/dali/customers/test/products/smartsymbol.json`](../test/dali/customers/test/products/smartsymbol.json) |

Renders the `SmartSymbol` parameter (integer code) as SVG symbol graphics at station locations.  Each code maps to an SVG symbol ID loaded from the symbol set.

**Output:**

![smartsymbol](images/dali/smartsymbol.png)

---

### smartsymbolnumber — Symbol codes as numbers

**Input:** [`test/input/smartsymbolnumber.get`](../test/input/smartsymbolnumber.get)

```
GET /dali?customer=test&product=smartsymbolnumber&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `smartsymbolnumber` | Product JSON: [`test/dali/customers/test/products/smartsymbolnumber.json`](../test/dali/customers/test/products/smartsymbolnumber.json) |

Plots the numeric `SmartSymbol` code as a text label (for debugging).

**Output:**

![smartsymbolnumber](images/dali/smartsymbolnumber.png)

---

### weather — Standard weather symbols

**Input:** [`test/input/weather.get`](../test/input/weather.get)

```
GET /dali?customer=test&product=weather&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `weather` | Product JSON: [`test/dali/customers/test/products/weather.json`](../test/dali/customers/test/products/weather.json) |

Renders a set of weather symbols loaded from SVG symbol files.  The product combines a map background with symbol layer.

**Output:**

![weather](images/dali/weather.png)

---

### weather-cssdef — Inline CSS override via URL

**Input:** [`test/input/weather-cssdef.get`](../test/input/weather-cssdef.get)

```
GET /dali?customer=test&product=weather&time=200808050300
    &defs.css.test%2Fsymbols%2Fdot=data:,.Dot%20%7B%20fill%3A%20yellow%3B%20stroke%3A%20none%20%7D%0A HTTP/1.0
```

| Parameter | Value (URL-decoded) | Description |
|-----------|---------------------|-------------|
| `defs.css.test/symbols/dot` | `data:,.Dot { fill: yellow; stroke: none }` | Inlines a CSS rule for the `dot` symbol class via a data-URL; overrides the file-based CSS |

The `defs.css.{name}=data:,...` pattern allows injecting arbitrary CSS rules through the URL without modifying product files.

**Output:**

![weather-cssdef](images/dali/weather-cssdef.png)

---

### weather-symbol-dataurl-get — Inline SVG symbol override

**Input:** [`test/input/weather-symbol-dataurl-get.get`](../test/input/weather-symbol-dataurl-get.get)

```
GET /dali?customer=test&product=weather-base64-symbol&time=200808050300
    &defs.symbols.weather%2F1_II%2F1="data:;base64,PHN5bWJvbC..." HTTP/1.0
```

| Parameter | Value (URL-decoded) | Description |
|-----------|---------------------|-------------|
| `defs.symbols.weather/1_II/1` | `data:;base64,<base64-SVG>` | Replaces the SVG symbol `weather/1_II/1` with an inline base64-encoded SVG symbol (an orange circle) |

Allows runtime replacement of individual symbols through the URL, e.g. for A/B testing or custom client-specific iconography.

**Output:**

![weather-symbol-dataurl-get](images/dali/weather-symbol-dataurl-get.png)

---

### weather-base64json-symbol — Base64 symbol from JSON

**Input:** [`test/input/weather-base64json-symbol.get`](../test/input/weather-base64json-symbol.get)

```
GET /dali?customer=test&product=weather-base64-symbol&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `weather-base64-symbol` | Product JSON: [`test/dali/customers/test/products/weather-base64-symbol.json`](../test/dali/customers/test/products/weather-base64-symbol.json) |

The product JSON itself specifies symbols using `data:;base64,…` data URLs (base64-encoded SVG), bypassing the filesystem symbol store.  Tests that the server can decode and embed inline symbols from the product definition.

**Output:**

![weather-base64json-symbol](images/dali/weather-base64json-symbol.png)

---

### symbols_coloured — Weather symbols in multiple projections

The same `symbols_coloured` product is rendered in four different projections using the `projection.crs` URL override:

**Inputs:**
- [`test/input/symbols_coloured_native.get`](../test/input/symbols_coloured_native.get) — data native CRS
- [`test/input/symbols_coloured_fin.get`](../test/input/symbols_coloured_fin.get) — `projection.crs=EPSG:3067`
- [`test/input/symbols_coloured_webmercator.get`](../test/input/symbols_coloured_webmercator.get) — `projection.crs=EPSG:3857`
- [`test/input/symbols_coloured_wgs84.get`](../test/input/symbols_coloured_wgs84.get) — `projection.crs=WGS84`

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `symbols_coloured` | Product JSON: [`test/dali/customers/test/products/symbols_coloured.json`](../test/dali/customers/test/products/symbols_coloured.json) |
| `projection.crs` | *(varies)* | Overrides the product's CRS on-the-fly |

Demonstrates that `projection.crs` can reproject any product at request time without modifying the JSON.

| Projection | Output |
|------------|--------|
| Native CRS | ![symbols_coloured_native](images/dali/symbols_coloured_native.png) |
| EPSG:3067 (ETRS-TM35FIN) | ![symbols_coloured_fin](images/dali/symbols_coloured_fin.png) |
| EPSG:3857 (WebMercator) | ![symbols_coloured_webmercator](images/dali/symbols_coloured_webmercator.png) |
| WGS84 | ![symbols_coloured_wgs84](images/dali/symbols_coloured_wgs84.png) |

---

## Flash and Lightning Symbols

### flash_symbols — Combined flash symbols

**Input:** [`test/input/flash_symbols.get`](../test/input/flash_symbols.get)

```
GET /dali?customer=test&product=flash_symbols&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `flash_symbols` | Product JSON: [`test/dali/customers/test/products/flash_symbols.json`](../test/dali/customers/test/products/flash_symbols.json) |
| `time` | `20130805T1500` | Valid time: 2013-08-05 15:00 UTC |

Plots all flash (lightning) strike locations (cloud-to-ground and cloud-to-cloud) as symbols from the `flash` producer.

**Output:**

![flash_symbols](images/dali/flash_symbols.png)

---

### flash_cloud_symbols — Cloud flash only

**Input:** [`test/input/flash_cloud_symbols.get`](../test/input/flash_cloud_symbols.get)

```
GET /dali?customer=test&product=flash_cloud_symbols&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `flash_cloud_symbols` | Product JSON: [`test/dali/customers/test/products/flash_cloud_symbols.json`](../test/dali/customers/test/products/flash_cloud_symbols.json) |

Plots only intra-cloud (IC) lightning flashes, filtering out cloud-to-ground (CG) strikes.

**Output:**

![flash_cloud_symbols](images/dali/flash_cloud_symbols.png)

---

### flash_ground_symbols — Ground flash only

**Input:** [`test/input/flash_ground_symbols.get`](../test/input/flash_ground_symbols.get)

```
GET /dali?customer=test&product=flash_ground_symbols&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `flash_ground_symbols` | Product JSON: [`test/dali/customers/test/products/flash_ground_symbols.json`](../test/dali/customers/test/products/flash_ground_symbols.json) |

Plots only cloud-to-ground (CG) lightning strikes.

**Output:**

![flash_ground_symbols](images/dali/flash_ground_symbols.png)

---

### flash_cloud_and_ground_symbols — Separate symbol types

**Input:** [`test/input/flash_cloud_and_ground_symbols.get`](../test/input/flash_cloud_and_ground_symbols.get)

```
GET /dali?customer=test&product=flash_cloud_and_ground_symbols&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `flash_cloud_and_ground_symbols` | Product JSON: [`test/dali/customers/test/products/flash_cloud_and_ground_symbols.json`](../test/dali/customers/test/products/flash_cloud_and_ground_symbols.json) |

Uses two symbol layers (one for IC, one for CG) with distinct symbol shapes and colours in a single product.

**Output:**

![flash_cloud_and_ground_symbols](images/dali/flash_cloud_and_ground_symbols.png)

---

## Wind Arrows

### wind — Basic wind arrows

**Input:** [`test/input/wind.get`](../test/input/wind.get)

```
GET /dali?customer=test&product=wind&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wind` | Product JSON: [`test/dali/customers/test/products/wind.json`](../test/dali/customers/test/products/wind.json) |

Renders wind direction arrows from `pal_skandinavia` using `"direction"` and `"speed"` parameters.  Arrow length scales with wind speed.

**Output:**

![wind](images/dali/wind.png)

---

### wind_margin — Wind arrows with map margin

**Input:** [`test/input/wind_margin.get`](../test/input/wind_margin.get)

```
GET /dali?customer=test&product=wind_margin&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wind_margin` | Product JSON: [`test/dali/customers/test/products/wind_margin.json`](../test/dali/customers/test/products/wind_margin.json) |

Same wind arrows but with a map area `margin` setting that extends the data area outside the nominal bounding box.

**Output:**

![wind_margin](images/dali/wind_margin.png)

---

### wind_minrotationspeed — Arrow rotation threshold

**Input:** [`test/input/wind_minrotationspeed.get`](../test/input/wind_minrotationspeed.get)

```
GET /dali?customer=test&product=wind&time=200808050300&l3.minrotationspeed=10 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `l3.minrotationspeed` | `10` | Arrows for layer `l3` are only rotated (to point in the wind direction) when wind speed exceeds 10 m/s; calmer arrows remain pointing north |

The `minrotationspeed` threshold prevents cluttered arrow directions at very low wind speeds.

**Output:**

![wind_minrotationspeed](images/dali/wind_minrotationspeed.png)

---

### wind_scaled — Scaled wind arrows

**Input:** [`test/input/wind_scaled.get`](../test/input/wind_scaled.get)

```
GET /dali?customer=test&product=wind_scaled&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wind_scaled` | Product JSON: [`test/dali/customers/test/products/wind_scaled.json`](../test/dali/customers/test/products/wind_scaled.json) |

Uses an explicit `"scale"` factor to map wind speed to arrow length.

**Output:**

![wind_scaled](images/dali/wind_scaled.png)

---

### wind_without_speed — Direction-only arrows

**Input:** [`test/input/wind_without_speed.get`](../test/input/wind_without_speed.get)

```
GET /dali?customer=test&product=wind_without_speed&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wind_without_speed` | Product JSON: [`test/dali/customers/test/products/wind_without_speed.json`](../test/dali/customers/test/products/wind_without_speed.json) |

Arrows show direction only; all arrows are rendered at the same fixed length since no speed parameter is provided.

**Output:**

![wind_without_speed](images/dali/wind_without_speed.png)

---

### hirlam_wind — HIRLAM wind (direction + speed)

**Input:** [`test/input/hirlam_wind.get`](../test/input/hirlam_wind.get)

```
GET /dali?customer=test&product=hirlam_wind&time=201804060600 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `hirlam_wind` | Product JSON: [`test/dali/customers/test/products/hirlam_wind.json`](../test/dali/customers/test/products/hirlam_wind.json) |
| `time` | `201804060600` | Valid time: 2018-04-06 06:00 UTC |

Arrow layer using meteorological `"direction": "WindDirection"` and `"speed": "WindSpeedMS"` parameters from the HIRLAM NWP model.

**Output:**

![hirlam_wind](images/dali/hirlam_wind.png)

---

### hirlam_uv — HIRLAM wind (U/V components)

**Input:** [`test/input/hirlam_uv.get`](../test/input/hirlam_uv.get)

```
GET /dali?customer=test&product=hirlam_uv&time=201804060600 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `hirlam_uv` | Product JSON: [`test/dali/customers/test/products/hirlam_uv.json`](../test/dali/customers/test/products/hirlam_uv.json) |

Arrow layer using Cartesian wind components `"u": "WindUMS"` and `"v": "WindVMS"` instead of direction/speed.  The server converts U/V to direction and magnitude internally.

**Output:**

![hirlam_uv](images/dali/hirlam_uv.png)

---

## Wind Barbs and Wind Roses

### windbarb — Classic wind barbs

**Input:** [`test/input/windbarb.get`](../test/input/windbarb.get)

```
GET /dali?customer=test&product=windbarb&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `windbarb` | Product JSON: [`test/dali/customers/test/products/windbarb.json`](../test/dali/customers/test/products/windbarb.json) |

Renders standard meteorological wind barbs (staff + barbs/pennants encoding speed in knots).

**Output:**

![windbarb](images/dali/windbarb.png)

---

### windrose — Wind roses at stations

**Input:** [`test/input/windrose.get`](../test/input/windrose.get)

```
GET /dali?customer=test&product=windrose&time=2013-08-05 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `windrose` | Product JSON: [`test/dali/customers/test/products/windrose.json`](../test/dali/customers/test/products/windrose.json) |
| `time` | `2013-08-05` | Valid date: 2013-08-05 (ISO 8601 date-only format) |

Renders wind roses at named coastal stations showing frequency distribution of wind direction and speed classes.  Each rose uses `starttimeoffset`/`endtimeoffset` to aggregate over a 24-hour window.  Station titles and connectors are customisable via CSS classes.

**Output:**

![windrose](images/dali/windrose.png)

---

## WAFS Aviation Charts

WAFS (World Area Forecast System) products use ICAO aviation weather data at pressure levels.

### wafs_temperature — Upper-level temperature

**Input:** [`test/input/wafs_temperature.get`](../test/input/wafs_temperature.get)

```
GET /dali?customer=test&product=wafs_temperature&time=201601191800 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wafs_temperature` | Product JSON: [`test/dali/customers/test/products/wafs_temperature.json`](../test/dali/customers/test/products/wafs_temperature.json) |
| `time` | `201601191800` | Valid time: 2016-01-19 18:00 UTC |

Temperature isobands at a WAFS pressure level (flight level) over the North Atlantic aviation area.

**Output:**

![wafs_temperature](images/dali/wafs_temperature.png)

---

### wafs_wind — Upper-level wind arrows

**Input:** [`test/input/wafs_wind.get`](../test/input/wafs_wind.get)

```
GET /dali?customer=test&product=wafs_wind&time=201601191800 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wafs_wind` | Product JSON: [`test/dali/customers/test/products/wafs_wind.json`](../test/dali/customers/test/products/wafs_wind.json) |

Wind arrows at WAFS pressure level overlaid on a base map.

**Output:**

![wafs_wind](images/dali/wafs_wind.png)

---

### wafs_leveldata — Level data display

**Input:** [`test/input/wafs_leveldata.get`](../test/input/wafs_leveldata.get)

```
GET /dali?customer=test&product=wafs_leveldata&time=201601191800 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wafs_leveldata` | Product JSON: [`test/dali/customers/test/products/wafs_leveldata.json`](../test/dali/customers/test/products/wafs_leveldata.json) |

Displays numeric values at grid points for WAFS level data.

**Output:**

![wafs_leveldata](images/dali/wafs_leveldata.png)

---

### wafs_windbarb — WAFS wind barbs

**Input:** [`test/input/wafs_windbarb.get`](../test/input/wafs_windbarb.get)

```
GET /dali?customer=test&product=wafs_windbarb&time=201601191800 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wafs_windbarb` | Product JSON: [`test/dali/customers/test/products/wafs_windbarb.json`](../test/dali/customers/test/products/wafs_windbarb.json) |

Standard wind barbs at a WAFS flight level.

**Output:**

![wafs_windbarb](images/dali/wafs_windbarb.png)

---

### wafs_windbarb_shifted — Wind barbs with label offset

**Input:** [`test/input/wafs_windbarb_shifted.get`](../test/input/wafs_windbarb_shifted.get)

```
GET /dali?customer=test&product=wafs_windbarb_shifted&time=201601191800 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wafs_windbarb_shifted` | Product JSON: [`test/dali/customers/test/products/wafs_windbarb_shifted.json`](../test/dali/customers/test/products/wafs_windbarb_shifted.json) |

WAFS wind barbs with a global label offset (`dx`, `dy`) applied to all barb positions.

**Output:**

![wafs_windbarb_shifted](images/dali/wafs_windbarb_shifted.png)

---

### wafs_windbarb_graticulefill — Wind barbs with graticule fill

**Input:** [`test/input/wafs_windbarb_graticulefill.get`](../test/input/wafs_windbarb_graticulefill.get)

```
GET /dali?customer=test&product=wafs_windbarb_graticulefill&time=201601191800 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wafs_windbarb_graticulefill` | Product JSON: [`test/dali/customers/test/products/wafs_windbarb_graticulefill.json`](../test/dali/customers/test/products/wafs_windbarb_graticulefill.json) |

Adds temperature-coloured filled rectangles in the graticule grid cells behind the wind barbs — a compact SIGWX-style layout.

**Output:**

![wafs_windbarb_graticulefill](images/dali/wafs_windbarb_graticulefill.png)

---

## Observations with Wind and Temperature

### opendata_meteorological_wind_keyword — Wind by keyword

**Input:** [`test/input/opendata_meteorological_wind_keyword.get`](../test/input/opendata_meteorological_wind_keyword.get)

```
GET /dali?customer=test&product=opendata_meteorological_wind_keyword&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_meteorological_wind_keyword` | Product JSON: [`test/dali/customers/test/products/opendata_meteorological_wind_keyword.json`](../test/dali/customers/test/products/opendata_meteorological_wind_keyword.json) |

Wind arrows at keyword-selected stations from the `opendata` producer.

**Output:**

![opendata_meteorological_wind_keyword](images/dali/opendata_meteorological_wind_keyword.png)

---

### opendata_meteorological_wind_keyword_plustemperatures — Wind + temperature

**Input:** [`test/input/opendata_meteorological_wind_keyword_plustemperatures.get`](../test/input/opendata_meteorological_wind_keyword_plustemperatures.get)

```
GET /dali?customer=test&product=opendata_meteorological_wind_keyword_plustemperatures&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_meteorological_wind_keyword_plustemperatures` | Product JSON: [`test/dali/customers/test/products/opendata_meteorological_wind_keyword_plustemperatures.json`](../test/dali/customers/test/products/opendata_meteorological_wind_keyword_plustemperatures.json) |

Combines wind arrows and temperature numbers at keyword-selected stations.

**Output:**

![opendata_meteorological_wind_keyword_plustemperatures](images/dali/opendata_meteorological_wind_keyword_plustemperatures.png)

---

### opendata_meteorological_wind_grid — Wind on grid

**Input:** [`test/input/opendata_meteorological_wind_grid.get`](../test/input/opendata_meteorological_wind_grid.get)

```
GET /dali?customer=test&product=opendata_meteorological_wind_grid&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_meteorological_wind_grid` | Product JSON: [`test/dali/customers/test/products/opendata_meteorological_wind_grid.json`](../test/dali/customers/test/products/opendata_meteorological_wind_grid.json) |

Wind arrows placed at regular pixel-grid intervals (`layout: "grid"`), thinning the station network for legibility.

**Output:**

![opendata_meteorological_wind_grid](images/dali/opendata_meteorological_wind_grid.png)

---

### opendata_meteorological_wind_data_plustemperatures — Wind + temperature data

**Input:** [`test/input/opendata_meteorological_wind_data_plustemperatures.get`](../test/input/opendata_meteorological_wind_data_plustemperatures.get)

```
GET /dali?customer=test&product=opendata_meteorological_wind_data_plustemperatures&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_meteorological_wind_data_plustemperatures` | Product JSON: [`test/dali/customers/test/products/opendata_meteorological_wind_data_plustemperatures.json`](../test/dali/customers/test/products/opendata_meteorological_wind_data_plustemperatures.json) |

Combines wind arrows and temperature numbers sourced from the same OpenData observation query.

**Output:**

![opendata_meteorological_wind_data_plustemperatures](images/dali/opendata_meteorological_wind_data_plustemperatures.png)

---

### opendata_meteorological_wind_data_plustemperatures_aggregate_max — Time-aggregated max wind

**Input:** [`test/input/opendata_meteorological_wind_data_plustemperatures_aggregate_max.get`](../test/input/opendata_meteorological_wind_data_plustemperatures_aggregate_max.get)

```
GET /dali?customer=test&product=opendata_meteorological_wind_data_plustemperatures_aggregate_max&time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `opendata_meteorological_wind_data_plustemperatures_aggregate_max` | Product JSON: [`test/dali/customers/test/products/opendata_meteorological_wind_data_plustemperatures_aggregate_max.json`](../test/dali/customers/test/products/opendata_meteorological_wind_data_plustemperatures_aggregate_max.json) |

Uses SmartMet time-aggregation function syntax to compute maximum wind speed over a trailing 12-hour window: `"speed": "nanmax_t(WindSpeedMS:12h:0h)"`.  The `nanmax_t` function aggregates over the time range ignoring NaN values.

**Output:**

![opendata_meteorological_wind_data_plustemperatures_aggregate_max](images/dali/opendata_meteorological_wind_data_plustemperatures_aggregate_max.png)

---

## Heatmap

### heatmap — Kernel-density heatmap

**Input:** [`test/input/heatmap.get`](../test/input/heatmap.get)

```
GET /dali?customer=test&product=heatmap&time=20130805T1500&l.heatmap.kernel=exp&l.heatmap.radius=40&l.heatmap.deviation=4 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `heatmap` | Product JSON: [`test/dali/customers/test/products/heatmap.json`](../test/dali/customers/test/products/heatmap.json) |
| `time` | `20130805T1500` | Valid time: 2013-08-05 15:00 UTC |
| `l.heatmap.kernel` | `exp` | Kernel function for density estimation (`exp` = exponential decay) |
| `l.heatmap.radius` | `40` | Override: kernel radius in pixels (product default is 20) |
| `l.heatmap.deviation` | `4` | Override: kernel deviation parameter (product default is 10.0) |

The heatmap is generated by an `isoband` layer with a `"heatmap"` sub-block specifying `resolution`, `radius`, `kernel`, and `deviation`.  A kernel-density surface is computed from the observation point locations and then rendered as isobands.

**Output:**

![heatmap](images/dali/heatmap.png)

---

## Hovmoeller Diagrams

Hovmoeller diagrams show a meteorological parameter plotted over two axes, one of which is time.

### hovmoeller_geoph500 — Geopotential: time × longitude

**Input:** [`test/input/hovmoeller_geoph500.get`](../test/input/hovmoeller_geoph500.get)

```
GET /dali?customer=test&product=hovmoeller_geoph500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `hovmoeller_geoph500` | Product JSON: [`test/dali/customers/test/products/hovmoeller_geoph500.json`](../test/dali/customers/test/products/hovmoeller_geoph500.json) |

`"direction": "time_lon"` — time on Y-axis, longitude on X-axis at fixed `"latitude": 60.0`.  Plots 500 hPa geopotential height isobands over `lon_min`/`lon_max` = 5°–35°E.

**Output:**

![hovmoeller_geoph500](images/dali/hovmoeller_geoph500.png)

---

### hovmoeller_geoph500_time_lat — Geopotential: time × latitude

**Input:** [`test/input/hovmoeller_geoph500_time_lat.get`](../test/input/hovmoeller_geoph500_time_lat.get)

```
GET /dali?customer=test&product=hovmoeller_geoph500_time_lat HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `hovmoeller_geoph500_time_lat` | Product JSON: [`test/dali/customers/test/products/hovmoeller_geoph500_time_lat.json`](../test/dali/customers/test/products/hovmoeller_geoph500_time_lat.json) |

`"direction": "time_lat"` — time on Y-axis, latitude on X-axis at fixed `"longitude": 20.0`.

**Output:**

![hovmoeller_geoph500_time_lat](images/dali/hovmoeller_geoph500_time_lat.png)

---

### hovmoeller_temperature_time_level — Temperature: time × pressure level

**Input:** [`test/input/hovmoeller_temperature_time_level.get`](../test/input/hovmoeller_temperature_time_level.get)

```
GET /dali?customer=test&product=hovmoeller_temperature_time_level HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `hovmoeller_temperature_time_level` | Product JSON: [`test/dali/customers/test/products/hovmoeller_temperature_time_level.json`](../test/dali/customers/test/products/hovmoeller_temperature_time_level.json) |

`"direction": "time_level"` — time on X-axis, pressure level on Y-axis at a fixed geographic point.  Shows the vertical temperature structure over time (a time-height cross-section).

**Output:**

![hovmoeller_temperature_time_level](images/dali/hovmoeller_temperature_time_level.png)

---

## Graticule

### graticule — Basic lat/lon grid

**Input:** [`test/input/graticule.get`](../test/input/graticule.get)

```
GET /dali?customer=test&product=graticule HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `graticule` | Product JSON: [`test/dali/customers/test/products/graticule.json`](../test/dali/customers/test/products/graticule.json) |

Two overlapping grids: a fine 1° grid (light, 0.2 px) skipping 10° multiples, and a coarse 10° grid (dark, 0.5 px).  Projection is inferred from the `kap` producer's native CRS.

**Output:**

![graticule](images/dali/graticule.png)

---

### graticule_goode — Graticule in Goode projection

**Input:** [`test/input/graticule_goode.get`](../test/input/graticule_goode.get)

```
GET /dali?customer=test&product=graticule_goode HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `graticule_goode` | Product JSON: [`test/dali/customers/test/products/graticule_goode.json`](../test/dali/customers/test/products/graticule_goode.json) |

A 10° graticule in the [Goode Interrupted Homolosine](https://proj.org/en/stable/operations/projections/igh.html) projection (`+proj=igh`).  Tests correct handling across the projection's interior discontinuities.

**Output:**

![graticule_goode](images/dali/graticule_goode.png)

---

### graticule_num_center — Centred labels

**Input:** [`test/input/graticule_num_center.get`](../test/input/graticule_num_center.get)

```
GET /dali?customer=test&product=graticule_num_center HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `graticule_num_center` | Product JSON: [`test/dali/customers/test/products/graticule_num_center.json`](../test/dali/customers/test/products/graticule_num_center.json) |

Graticule with `"labels": {"layout": "center"}` — degree values placed at the centres of grid lines with a rectangle background.

**Output:**

![graticule_num_center](images/dali/graticule_num_center.png)

---

### graticule_num_center_edge — Edge-centred labels

**Input:** [`test/input/graticule_num_center_edge.get`](../test/input/graticule_num_center_edge.get)

```
GET /dali?customer=test&product=graticule_num_edge_center HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `graticule_num_edge_center` | Product JSON: [`test/dali/customers/test/products/graticule_num_edge_center.json`](../test/dali/customers/test/products/graticule_num_edge_center.json) |

Graticule with `"layout": "edge_center"` — labels placed at the mid-point of each grid line where it intersects the map edge (frame), with a bordered rectangle background.

**Output:**

![graticule_num_center_edge](images/dali/graticule_num_center_edge.png)

---

### graticule_num_cross — Crossing-point labels

**Input:** [`test/input/graticule_num_cross.get`](../test/input/graticule_num_cross.get)

```
GET /dali?customer=test&product=graticule_num_cross HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `graticule_num_cross` | Product JSON: [`test/dali/customers/test/products/graticule_num_cross.json`](../test/dali/customers/test/products/graticule_num_cross.json) |

Graticule with `"layout": "cross"` — labels placed at every intersection of latitude and longitude grid lines.

**Output:**

![graticule_num_cross](images/dali/graticule_num_cross.png)

---

### graticule_ticks — Tick marks at frame

**Input:** [`test/input/graticule_ticks.get`](../test/input/graticule_ticks.get)

```
GET /dali?customer=test&product=graticule_ticks HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `graticule_ticks` | Product JSON: [`test/dali/customers/test/products/graticule_ticks.json`](../test/dali/customers/test/products/graticule_ticks.json) |

Uses `"layout": "ticks"` to draw only short tick marks at the map border where grid lines intersect, without drawing the full grid lines across the map.

**Output:**

![graticule_ticks](images/dali/graticule_ticks.png)

---

## Circles Layer

The `circle` layer type draws SVG circles at named geographic locations.

### circles — Concentric circles at cities

**Input:** [`test/input/circles.get`](../test/input/circles.get)

```
GET /dali?customer=test&product=circles&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `circles` | Product JSON: [`test/dali/customers/test/products/circles.json`](../test/dali/customers/test/products/circles.json) |
| `time` | `201503131200` | Valid time: 2015-03-13 12:00 UTC |

Draws concentric rings around named cities (Helsinki, Tampere, Turku) over a Natural Earth map in EPSG:3035 (ETRS-LAEA).  Each `circle` layer entry specifies a `name` (resolved to coordinates via a geocoder) and a radius.

**Output:**

![circles](images/dali/circles.png)

---

### circles_labels_sw — Circle labels (SW anchor)

**Input:** [`test/input/circles_labels_sw.get`](../test/input/circles_labels_sw.get)

```
GET /dali?customer=test&product=circles_labels_sw&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `circles_labels_sw` | Product JSON: [`test/dali/customers/test/products/circles_labels_sw.json`](../test/dali/customers/test/products/circles_labels_sw.json) |

Adds city name labels to the circles, anchored at the south-west corner of each station.

**Output:**

![circles_labels_sw](images/dali/circles_labels_sw.png)

---

### circles_labels_tr — Circle labels (TR anchor)

**Input:** [`test/input/circles_labels_tr.get`](../test/input/circles_labels_tr.get)

```
GET /dali?customer=test&product=circles_labels_tr&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `circles_labels_tr` | Product JSON: [`test/dali/customers/test/products/circles_labels_tr.json`](../test/dali/customers/test/products/circles_labels_tr.json) |

Same as `circles_labels_sw` but labels are anchored at the top-right corner.

**Output:**

![circles_labels_tr](images/dali/circles_labels_tr.png)

---

## Fronts

### fronts — Weather fronts

**Input:** [`test/input/fronts.get`](../test/input/fronts.get)

```
GET /dali?customer=test&product=fronts&type=png&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `fronts` | Product JSON: [`test/dali/customers/test/products/fronts.json`](../test/dali/customers/test/products/fronts.json) |
| `type` | `png` | Rasterised PNG output (700×500 px, 8-bit palette) |

Renders weather fronts (cold, warm, occluded, stationary) in EPSG:3857 over a background map.  Uses the `fronts` layer type with CSS styling.

**Output:**

![fronts](images/dali/fronts.png)

---

## PostGIS Layer

### ice — Ice chart from PostGIS

**Input:** [`test/input/ice.get`](../test/input/ice.get)

```
GET /dali?customer=test&product=ice HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `ice` | Product JSON: [`test/dali/customers/test/products/ice.json`](../test/dali/customers/test/products/ice.json) |

Renders Baltic Sea ice chart polygons from the `icemap` PostGIS database using `"layer_type": "postgis"`.  The PostGIS layer can read any geometry type (Polygon, MultiPolygon, LineString) and apply CSS fill/stroke styling.

**Output:**

![ice](images/dali/ice.png)

---

## WKT Layer

### wkt — Well-Known Text geometry

**Input:** [`test/input/wkt.get`](../test/input/wkt.get)

```
GET /dali?customer=test&product=wkt&time=201503131200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `wkt` | Product JSON: [`test/dali/customers/test/products/wkt.json`](../test/dali/customers/test/products/wkt.json) |
| `time` | `201503131200` | Valid time: 2015-03-13 12:00 UTC |

The `wkt` layer type renders arbitrary WKT (Well-Known Text) geometry strings (e.g. polygons, lines) stored in the product definition.  Geometries are projected to the map CRS and styled with CSS.

**Output:**

![wkt](images/dali/wkt.png)

---

## METAR Layer

### metar_basic — Aviation METAR observations

**Input:** [`test/input/metar_basic.get`](../test/input/metar_basic.get)

```
GET /dali?customer=test&product=metar_basic&time=201511170020&format=svg HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `metar_basic` | Product JSON: [`test/dali/customers/test/products/metar_basic.json`](../test/dali/customers/test/products/metar_basic.json) |
| `time` | `201511170020` | Valid time: 2015-11-17 00:20 UTC |
| `format` | `svg` | SVG output format |

Uses `"layer_type": "metar"` with `"message_type": "METAR"` and `"message_format": "TAC"` to decode raw METAR reports stored as TAC (Traditional Alphanumeric Code) weather tokens.  Controls which elements to display: `show_station`, `show_wind`, `show_visibility`, `show_present_weather`, `show_sky_condition`, `show_temperature`.

*No reference output image available (no test output file generated in CI).*

---

## Fire Weather

### forestfire — Forest Fire Index isobands

**Input:** [`test/input/forestfire.get`](../test/input/forestfire.get)

```
GET /dali?product=forestfire&time=20190207T0600 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `forestfire` | No `customer` specified — uses the default customer directory |
| `time` | `20190207T0600` | Valid time: 2019-02-07 06:00 UTC |

Renders `ForestFireIndex` isobands using `"extrapolation": 2` to fill in missing data near the domain boundaries.  The product uses the `forestfire` producer (separate from general synoptic producers).

**Output:**

![forestfire](images/dali/forestfire.png)

---

### grassfire — Grassfire risk by named region

**Input:** [`test/input/grassfire.get`](../test/input/grassfire.get)

```
GET /dali?product=grassfire&time=20181212T0600 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `grassfire` | Uses default customer |
| `time` | `20181212T0600` | Valid time: 2018-12-12 06:00 UTC |

Renders grassfire risk levels using the `grassfire` layer type.  Regions are matched by named feature attributes (e.g. `"value": "Uusimaa"` matches the Uusimaa administrative area).

**Output:**

![grassfire](images/dali/grassfire.png)

---

### grassfire_automatic — Grassfire risk by attribute range

**Input:** [`test/input/grassfire_automatic.get`](../test/input/grassfire_automatic.get)

```
GET /dali?product=grassfire_automatic&time=20181212T0600 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `grassfire_automatic` | Uses default customer |

Like `grassfire` but regions are matched by numeric attribute ranges instead of named values — more flexible for data with numeric risk-class codes.

*No reference output image available.*

---

## Text Layer

### text — SVG text layer

**Input:** [`test/input/text.get`](../test/input/text.get)

```
GET /dali?customer=test&product=text&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `product` | `text` | Product JSON: [`test/dali/customers/test/products/text.json`](../test/dali/customers/test/products/text.json) |

The `text` layer type renders static or parameterised text strings as SVG `<text>` elements.  The product also demonstrates how `tag: "clipPath"` and `tag: "g"` layers are used to create inline clip regions.

**Output:**

![text](images/dali/text.png)

---

## World Map Products

These products render global-scale model data in various projections.  Each family (ECMWF, GFS) covers a standard set of projections; Pacific-centred views are available for equidistant cylindrical and WGS84 projections.

### ECMWF global temperature

All EC world products use the `ecmwf_maailma_pinta` producer and a 1440×720 px canvas:

| Test | Projection | CRS | Input |
|------|-----------|-----|-------|
| `ec_world_eqc` | Equidistant Cylindrical (Atlantic) | `EPSG:4087` | [`ec_world_eqc.get`](../test/input/ec_world_eqc.get) |
| `ec_world_eqc_pacific` | Equidistant Cylindrical (Pacific, lon₀=180°) | `+init=epsg:4087 +lon_0=180` | [`ec_world_eqc_pacific.get`](../test/input/ec_world_eqc_pacific.get) |
| `ec_world_webmercator` | Web Mercator | `EPSG:3857` | [`ec_world_webmercator.get`](../test/input/ec_world_webmercator.get) |
| `ec_world_wgs84_atlantic` | Geographic WGS84 (Atlantic) | `+init=epsg:4326` | [`ec_world_wgs84_atlantic.get`](../test/input/ec_world_wgs84_atlantic.get) |
| `ec_world_wgs84_pacific` | Geographic WGS84 (Pacific, lon_wrap=180) | `+init=epsg:4326 +lon_wrap=180` | [`ec_world_wgs84_pacific.get`](../test/input/ec_world_wgs84_pacific.get) |

The `+lon_wrap=180` modifier shifts the longitude seam to the antimeridian so that the Pacific Ocean is unbroken.

| Projection | Output |
|-----------|--------|
| EQC Atlantic | ![ec_world_eqc](images/dali/ec_world_eqc.png) |
| EQC Pacific | ![ec_world_eqc_pacific](images/dali/ec_world_eqc_pacific.png) |
| Web Mercator | ![ec_world_webmercator](images/dali/ec_world_webmercator.png) |
| WGS84 Atlantic | ![ec_world_wgs84_atlantic](images/dali/ec_world_wgs84_atlantic.png) |
| WGS84 Pacific | ![ec_world_wgs84_pacific](images/dali/ec_world_wgs84_pacific.png) |

---

### GFS global temperature

All GFS world products use the `gfs` producer.  In addition to the five standard projections, GFS also includes polar, Goode, Natural Earth, and Near-side Perspective views:

| Test | Projection | CRS |
|------|-----------|-----|
| `gfs_world_eqc` | EQC Atlantic | `+init=epsg:4087` |
| `gfs_world_eqc_pacific` | EQC Pacific (lon₀=180°) | `+init=epsg:4087 +lon_0=180` |
| `gfs_world_webmercator` | Web Mercator (Atlantic) | `EPSG:3857` |
| `gfs_world_webmercator_pacific` | Web Mercator (Pacific bbox) | `EPSG:3857` |
| `gfs_world_wgs84_atlantic` | Geographic WGS84 Atlantic | `+init=epsg:4326` |
| `gfs_world_wgs84_pacific` | Geographic WGS84 Pacific | `+init=epsg:4326 +lon_wrap=180` |
| `gfs_world_arctic` | Polar Stereographic (N) | `+proj=stere +lat_0=90` |
| `gfs_world_antarctic` | Polar Stereographic (S) | `+proj=stere +lat_0=-90` |
| `gfs_world_goode` | Goode Homolosine | `+proj=igh +lon_0=0` |
| `gfs_world_natural_earth` | Natural Earth | `+proj=natearth` |
| `gfs_world_perspective` | Near-side Perspective (h=3000 km) | `+proj=nsper +h=3000000 +lat_0=60 +lon_0=25` |
| `gfs_world_stereographic` | Polar Stereographic (Europe) | `+proj=stere +lat_0=90 +lat_ts=60 +lon_0=20` |

| Projection | Output |
|-----------|--------|
| EQC Atlantic | ![gfs_world_eqc](images/dali/gfs_world_eqc.png) |
| EQC Pacific | ![gfs_world_eqc_pacific](images/dali/gfs_world_eqc_pacific.png) |
| Web Mercator | ![gfs_world_webmercator](images/dali/gfs_world_webmercator.png) |
| Web Mercator Pacific | ![gfs_world_webmercator_pacific](images/dali/gfs_world_webmercator_pacific.png) |
| WGS84 Atlantic | ![gfs_world_wgs84_atlantic](images/dali/gfs_world_wgs84_atlantic.png) |
| WGS84 Pacific | ![gfs_world_wgs84_pacific](images/dali/gfs_world_wgs84_pacific.png) |
| Arctic | ![gfs_world_arctic](images/dali/gfs_world_arctic.png) |
| Antarctic | ![gfs_world_antarctic](images/dali/gfs_world_antarctic.png) |
| Goode Homolosine | ![gfs_world_goode](images/dali/gfs_world_goode.png) |
| Natural Earth | ![gfs_world_natural_earth](images/dali/gfs_world_natural_earth.png) |
| Perspective (h=3000 km) | ![gfs_world_perspective](images/dali/gfs_world_perspective.png) |
| Stereographic (Europe) | ![gfs_world_stereographic](images/dali/gfs_world_stereographic.png) |

---

# WMS Test Examples

The WMS tests exercise the `/wms` endpoint.  Test inputs are in [`test/input/wms_*.get`](../test/input/) and expected outputs in [`test/output/wms_*.get`](../test/output/).  Product configurations are under [`test/wms/customers/`](../test/wms/customers/).

## GetCapabilities

### wms_getcapabilities

**Input:** [`test/input/wms_getcapabilities.get`](../test/input/wms_getcapabilities.get)

```
GET /wms?service=wms&request=GetCapabilities&debug=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `service` | `wms` | OGC WMS service identifier (case-insensitive) |
| `request` | `GetCapabilities` | Returns the service metadata XML document |
| `debug` | `1` | Adds layer-level debug information to the response |

Returns a WMS 1.3.0 XML capabilities document listing all advertised layers, styles, CRS support, output formats, and service metadata.

**Output:** [`test/output/wms_getcapabilities.get`](../test/output/wms_getcapabilities.get) — XML

### wms_getcapabilities_fi

**Input:** [`test/input/wms_getcapabilities_fi.get`](../test/input/wms_getcapabilities_fi.get)

```
GET /wms?service=wms&request=GetCapabilities&language=fi&debug=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `language` | `fi` | Returns layer titles and abstracts in Finnish |
| `debug` | `1` | Adds layer-level debug information |

Demonstrates multi-language support in GetCapabilities: layer titles and abstracts are returned in the requested language when translations are defined in the product JSON.

**Output:** [`test/output/wms_getcapabilities_fi.get`](../test/output/wms_getcapabilities_fi.get) — XML

### wms_getcapabilities_json

**Input:** [`test/input/wms_getcapabilities_json.get`](../test/input/wms_getcapabilities_json.get)

```
GET /wms?service=wms&request=GetCapabilities&format=application/json&debug=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `format` | `application/json` | Returns capabilities as a JSON document instead of XML |
| `debug` | `1` | Adds layer-level debug information |

Returns the same layer metadata as the XML capabilities but in JSON format, suitable for programmatic consumption.

**Output:** [`test/output/wms_getcapabilities_json.get`](../test/output/wms_getcapabilities_json.get) — JSON

### wms_getcapabilities_namespace

**Input:** [`test/input/wms_getcapabilities_namespace.get`](../test/input/wms_getcapabilities_namespace.get)

```
GET /wms?service=wms&request=GetCapabilities&namespace=ely HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `namespace` | `ely` | Filters the capabilities response to only include layers in the `ely` customer namespace |

Returns a capabilities document scoped to a single customer namespace.

**Output:** [`test/output/wms_getcapabilities_namespace.get`](../test/output/wms_getcapabilities_namespace.get) — XML

### wms_getcapabilities_recursive

**Input:** [`test/input/wms_getcapabilities_recursive.get`](../test/input/wms_getcapabilities_recursive.get)

```
GET /wms?service=wms&request=GetCapabilities&debug=1&layout=recursive HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layout` | `recursive` | Groups layers recursively by customer namespace in the capabilities XML |

**Output:** [`test/output/wms_getcapabilities_recursive.get`](../test/output/wms_getcapabilities_recursive.get) — XML

### wms_getcapabilities_timeinterval

**Input:** [`test/input/wms_getcapabilities_timeinterval.get`](../test/input/wms_getcapabilities_timeinterval.get)

```
GET /wms?service=wms&request=GetCapabilities&starttime=2008-08-09T00:00:00Z&endtime=2008-08-09T06:00:00Z&debug=1 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `starttime` | `2008-08-09T00:00:00Z` | Restricts time dimension listings to this start |
| `endtime` | `2008-08-09T06:00:00Z` | Restricts time dimension listings to this end |

Returns capabilities with time dimension values limited to the specified interval.

**Output:** [`test/output/wms_getcapabilities_timeinterval.get`](../test/output/wms_getcapabilities_timeinterval.get) — XML

---

## GetMap — Isoband styles

All three tests render temperature isobands for the `test:t2m` layer over Scandinavia (EPSG:4326, bbox 59–71°N, 17–34°E, 300×500 px, PNG).  They differ only in the `styles` parameter, which selects the isoband interval width.

### wms_getmap_isoband_style1

**Input:** [`test/input/wms_getmap_isoband_style1.get`](../test/input/wms_getmap_isoband_style1.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:t2m
    &styles=temperature_one_degrees
    &crs=EPSG:4326
    &bbox=59,17,71,34
    &width=300&height=500
    &format=image/png
    &time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `service` | `wms` | OGC WMS service |
| `request` | `GetMap` | Render a map image |
| `version` | `1.3.0` | WMS version |
| `layers` | `test:t2m` | Layer `t2m` in customer `test` |
| `styles` | `temperature_one_degrees` | 1°C isoband width style |
| `crs` | `EPSG:4326` | WGS 84 geographic CRS |
| `bbox` | `59,17,71,34` | Bounding box: minLat, minLon, maxLat, maxLon (WMS 1.3.0 axis order for EPSG:4326) |
| `width` / `height` | `300` / `500` | Output image dimensions in pixels |
| `format` | `image/png` | PNG output |
| `time` | `200808050300` | Valid time: 2008-08-05 03:00 UTC |

Product config: [`test/wms/customers/test/products/t2m.json`](../test/wms/customers/test/products/t2m.json)

![wms_getmap_isoband_style1](images/wms/wms_getmap_isoband_style1.png)

### wms_getmap_isoband_style2

**Input:** [`test/input/wms_getmap_isoband_style2.get`](../test/input/wms_getmap_isoband_style2.get)

Same as above with `styles=temperature_two_degrees` (2°C isoband width).

![wms_getmap_isoband_style2](images/wms/wms_getmap_isoband_style2.png)

### wms_getmap_isoband_style3

**Input:** [`test/input/wms_getmap_isoband_style3.get`](../test/input/wms_getmap_isoband_style3.get)

Same as above with `styles=temperature_three_degrees` (3°C isoband width).

![wms_getmap_isoband_style3](images/wms/wms_getmap_isoband_style3.png)

### wms_getmap_isoband_tile

**Input:** [`test/input/wms_getmap_isoband_tile.get`](../test/input/wms_getmap_isoband_tile.get)

Renders the same `test:t2m` layer cropped to a WMS tile-compatible bounding box, verifying that isobands are properly clipped to tile boundaries.

![wms_getmap_isoband_tile](images/wms/wms_getmap_isoband_tile.png)

---

## GetMap — Isoline styles

### wms_getmap_isoline_style1

**Input:** [`test/input/wms_getmap_isoline_style1.get`](../test/input/wms_getmap_isoline_style1.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:precipitation
    &styles=precipitation_thin_style
    &crs=EPSG:4326
    &bbox=59,17,71,34
    &width=300&height=500
    &format=image/png
    &time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:precipitation` | Precipitation isobands + isoline layer |
| `styles` | `precipitation_thin_style` | Thin (1 px) isoline style |

Product config: [`test/wms/customers/test/products/precipitation_areas.json`](../test/wms/customers/test/products/precipitation_areas.json)

![wms_getmap_isoline_style1](images/wms/wms_getmap_isoline_style1.png)

### wms_getmap_isoline_style2

**Input:** [`test/input/wms_getmap_isoline_style2.get`](../test/input/wms_getmap_isoline_style2.get)

Same request with `styles=precipitation_thick_style` (2 px isoline width).

![wms_getmap_isoline_style2](images/wms/wms_getmap_isoline_style2.png)

### wms_getmap_isoline_groups

**Input:** [`test/input/wms_getmap_isoline_groups.get`](../test/input/wms_getmap_isoline_groups.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:pressure_isoline_groups
    &styles=
    &crs=EPSG:4326
    &bbox=59,17,71,34
    &width=300&height=500
    &format=image/png
    &time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:pressure_isoline_groups` | Pressure isolines with grouped rendering |
| `styles` | _(empty)_ | Default style |

Demonstrates the `isoline_groups` feature: isolines are organised into named groups (e.g. every 5 hPa vs every 1 hPa), which can be styled and filtered independently.

Product config: [`test/wms/customers/test/products/pressure_isoline_groups.json`](../test/wms/customers/test/products/pressure_isoline_groups.json)

![wms_getmap_isoline_groups](images/wms/wms_getmap_isoline_groups.png)

The `_aggregate_min` and `_aggregate_max` variants (`wms_getmap_isoline_groups_aggregate_min`, `wms_getmap_isoline_groups_aggregate_max`) test time-aggregation modes.

---

## GetMap — Isolabel styles

### wms_getmap_isolabel_style1

**Input:** [`test/input/wms_getmap_isolabel_style1.get`](../test/input/wms_getmap_isolabel_style1.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:isolabel
    &styles=isolabel_small_blue_text_style
    &crs=EPSG:4326
    &bbox=59,17,71,34
    &width=300&height=500
    &format=image/png
    &time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:isolabel` | Layer with isoline labels |
| `styles` | `isolabel_small_blue_text_style` | Small (24 px), blue Roboto font labels |

The `isolabel` layer type places text annotations directly on isolines at computed placement positions.

Product config: [`test/wms/customers/test/products/isolabel.json`](../test/wms/customers/test/products/isolabel.json)

![wms_getmap_isolabel_style1](images/wms/wms_getmap_isolabel_style1.png)

### wms_getmap_isolabel_style2

**Input:** [`test/input/wms_getmap_isolabel_style2.get`](../test/input/wms_getmap_isolabel_style2.get)

Same request with `styles=isolabel_big_red_text_style` (36 px red Roboto font).

![wms_getmap_isolabel_style2](images/wms/wms_getmap_isolabel_style2.png)

---

## GetMap — Temperature numbers

### wms_getmap_temperature_numbers

**Input:** [`test/input/wms_getmap_temperature_numbers.get`](../test/input/wms_getmap_temperature_numbers.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:opendata_temperature_numbers
    &styles=
    &crs=EPSG:4326
    &bbox=59,17,71,34
    &width=600&height=1000
    &format=image/png
    &time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:opendata_temperature_numbers` | Temperature observations as numeric labels |
| `width` / `height` | `600` / `1000` | Higher resolution for readable numbers |
| `time` | `20130805T1500` | ISO 8601 time format |

Renders temperature observations from the `opendata` producer at FMI station positions over a background map.

Product config: [`test/wms/customers/test/products/opendata_temperature_numbers.json`](../test/wms/customers/test/products/opendata_temperature_numbers.json)

![wms_getmap_temperature_numbers](images/wms/wms_getmap_temperature_numbers.png)

### Mindistance and priority variants

The following tests verify label collision avoidance and priority selection for dense station networks:

| Test | `mindistance` | `priority` | Description |
|------|---------------|------------|-------------|
| `wms_getmap_temperature_numbers_mindistance` | set | — | Filter stations closer than `mindistance` pixels |
| `wms_getmap_temperature_numbers_mindistance_priority_none` | set | `none` | No priority: arbitrary station is kept |
| `wms_getmap_temperature_numbers_mindistance_priority_min` | set | `min` | Keep station with minimum value |
| `wms_getmap_temperature_numbers_mindistance_priority_max` | set | `max` | Keep station with maximum value |
| `wms_getmap_temperature_numbers_mindistance_priority_extrema` | set | `extrema` | Keep both min and max within the area |
| `wms_getmap_temperature_numbers_mindistance_priority_array` | set | array | User-defined priority list |
| `wms_getmap_temperature_numbers_aggregate_min` | — | — | Time-aggregate: minimum over interval |
| `wms_getmap_temperature_numbers_aggregate_max` | — | — | Time-aggregate: maximum over interval |

---

## GetMap — Meteorological wind arrows

### wms_getmap_meteorological_windarrows

**Input:** [`test/input/wms_getmap_meteorological_windarrows.get`](../test/input/wms_getmap_meteorological_windarrows.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:opendata_meteorological_windarrows
    &styles=
    &crs=EPSG:4326
    &bbox=59,17,71,34
    &width=600&height=1000
    &format=image/png
    &time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:opendata_meteorological_windarrows` | Wind observations as meteorological barb symbols |

Renders wind barbs at FMI observation stations, scaled to knots (`multiplier=1.94384449244`).  Wind barbs face the wind direction with the `southflop` option ensuring correct hemisphere rendering.

Product config: [`test/wms/customers/test/products/opendata_meteorological_windarrows.json`](../test/wms/customers/test/products/opendata_meteorological_windarrows.json)

![wms_getmap_meteorological_windarrows](images/wms/wms_getmap_meteorological_windarrows.png)

The mindistance and aggregate variants follow the same pattern as temperature numbers (see table above).

---

## GetMap — Wind arrows (model data)

### wms_getmap_windarrows

**Input:** [`test/input/wms_getmap_windarrows.get`](../test/input/wms_getmap_windarrows.get)

```
GET /wms?servicE=wms&requesT=GetMap&versioN=1.3.0
    &layerS=test:windarrows
    &styleS=
    &crS=EPSG:4326
    &bboX=59,17,71,34
    &widtH=300&heighT=500
    &formaT=image/svg%2Bxml
    &timE=200808050300 HTTP/1.0
```

Note: parameter names use mixed case (e.g. `layerS`, `crS`) — this tests that the WMS handler is case-insensitive for all WMS standard parameters.

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:windarrows` | Model wind arrows at a regular grid layout |
| `format` | `image/svg+xml` | SVG output |

Product config: [`test/wms/customers/test/products/windarrows.json`](../test/wms/customers/test/products/windarrows.json)

![wms_getmap_windarrows](images/wms/wms_getmap_windarrows.png)

### wms_getmap_windarrows_style

**Input:** [`test/input/wms_getmap_windarrows_style.get`](../test/input/wms_getmap_windarrows_style.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:windarrows
    &styles=windarrows_sparse_style
    &crs=EPSG:4326&bbox=59,17,71,34
    &width=300&height=500
    &format=image/png&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `styles` | `windarrows_sparse_style` | Sparser grid layout (50×50 px spacing) |

![wms_getmap_windarrows_style](images/wms/wms_getmap_windarrows_style.png)

The `_aggregate_min` and `_aggregate_max` variants test temporal aggregation.

---

## GetMap — Wind numbers

### wms_getmap_windnumbers_style1

**Input:** [`test/input/wms_getmap_windnumbers_style1.get`](../test/input/wms_getmap_windnumbers_style1.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:windnumbers
    &styles=
    &crs=EPSG:4326&bbox=59,17,71,34
    &width=300&height=500
    &format=image/png&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:windnumbers` | Wind speed as numeric annotations at a regular grid |
| `styles` | _(empty)_ | Default style (fine grid: 20×20 px) |

Product config: [`test/wms/customers/test/products/windnumbers.json`](../test/wms/customers/test/products/windnumbers.json)

![wms_getmap_windnumbers_style1](images/wms/wms_getmap_windnumbers_style1.png)

### wms_getmap_windnumbers_style2

Same with `styles=windnumbers_sparse_style` (60×60 px grid).

![wms_getmap_windnumbers_style2](images/wms/wms_getmap_windnumbers_style2.png)

The `_aggregate_min` and `_aggregate_max` variants test temporal aggregation.

---

## GetMap — Temperature symbols

### wms_getmap_temperature_symbols

**Input:** [`test/input/wms_getmap_temperature_symbols.get`](../test/input/wms_getmap_temperature_symbols.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:opendata_temperature_symbols
    &styles=
    &crs=EPSG:4326&bbox=59,17,71,34
    &width=600&height=1000
    &format=image/svg%2Bxml
    &time=20130805T1500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:opendata_temperature_symbols` | Temperature observations as colour-coded SVG symbols |

Renders temperature observations as coloured circle symbols using the `symbol` layer type.

![wms_getmap_temperature_symbols](images/wms/wms_getmap_temperature_symbols.png)

Mindistance and priority variants follow the same pattern as temperature numbers (see above).

---

## GetMap — Multiple layers

### wms_getmap_multiple_layers

**Input:** [`test/input/wms_getmap_multiple_layers.get`](../test/input/wms_getmap_multiple_layers.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:backgroundmap,test:precipitation_areas,test:cities
    &styles=
    &crs=EPSG:4326&bbox=59,17,71,34
    &width=300&height=500
    &format=image/svg%2Bxml
    &time=20080805120000 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:backgroundmap,test:precipitation_areas,test:cities` | Comma-separated layer list rendered bottom-to-top |
| `styles` | _(empty)_ | Default styles for all layers |

Demonstrates composite multi-layer rendering: a background map, precipitation isobands/isolines, and city symbol overlay are merged into a single SVG response.

![wms_getmap_multiple_layers](images/wms/wms_getmap_multiple_layers.png)

### wms_getmap_multiple_layers_with_style

**Input:** [`test/input/wms_getmap_multiple_layers_with_style.get`](../test/input/wms_getmap_multiple_layers_with_style.get)

```
GET /wms?...&layers=test:backgroundmap,test:precipitation_areas,test:cities
    &styles=,precipitation_thick_style, HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `styles` | `,precipitation_thick_style,` | Per-layer styles: empty for 1st and 3rd layers, explicit style for 2nd |

The `styles` list must have the same number of comma-separated entries as `layers`; empty entries use the default style.

![wms_getmap_multiple_layers_with_style](images/wms/wms_getmap_multiple_layers_with_style.png)

---

## GetMap — CRS variants

### wms_getmap_png (EPSG:4326, PNG)

**Input:** [`test/input/wms_getmap_png.get`](../test/input/wms_getmap_png.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=ely:wmsmap&styles=
    &crs=EPSG:4326&bbox=59,17,71,34
    &width=300&height=500
    &format=image/png&time=200808050300 HTTP/1.0
```

Standard WMS GetMap request using EPSG:4326 with PNG output.

Product config: [`test/wms/customers/ely/products/wmsmap.json`](../test/wms/customers/ely/products/wmsmap.json)

![wms_getmap_png](images/wms/wms_getmap_png.png)

### wms_getmap_svgxml (case-insensitive parameters)

**Input:** [`test/input/wms_getmap_svgxml.get`](../test/input/wms_getmap_svgxml.get)

Same request with mixed-case parameter names (`servicE`, `requesT`, etc.) and `format=image/svg+xml`.  Verifies that all WMS standard parameters are parsed case-insensitively.

![wms_getmap_svgxml](images/wms/wms_getmap_svgxml.png)

### wms_getmap_crs_auto2_42001 (AUTO2 UTM zone)

**Input:** [`test/input/wms_getmap_crs_auto2_42001.get`](../test/input/wms_getmap_crs_auto2_42001.get)

```
GET /wms?service=wms&request=getmap&version=1.3.0
    &layers=ely:wmsmap&styles=
    &crs=AUTO2:42001,1,25,60
    &debug=1
    &bbox=221000,6660000,690000,7770000
    &width=500&height=500
    &format=image/svg%2bxml
    &time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `crs` | `AUTO2:42001,1,25,60` | OGC AUTO2 Universal Transverse Mercator; zone determined by `lon=25°E, lat=60°N` |
| `bbox` | `221000,6660000,690000,7770000` | Bounding box in metres (AUTO2 projected coordinates) |

OGC AUTO2 CRS codes automatically derive a projection from supplied reference coordinates.  `42001` = UTM.

![wms_getmap_crs_auto2_42001](images/wms/wms_getmap_crs_auto2_42001.png)

### wms_getmap_crs_auto2_42004

Same but with `AUTO2:42004` (Equidistant Cylindrical).

![wms_getmap_crs_auto2_42004](images/wms/wms_getmap_crs_auto2_42004.png)

### wms_getmap_crs_scandinavia (named CRS)

**Input:** [`test/input/wms_getmap_crs_scandinavia.get`](../test/input/wms_getmap_crs_scandinavia.get)

```
GET /wms?...&crs=CRS:SmartMetScandinavia
    &bbox=-1010040,-4051052,1005949,-1814781
    &width=500&height=500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `crs` | `CRS:SmartMetScandinavia` | Named CRS defined in the server's GIS configuration |
| `bbox` | (metres) | Bounding box in the named CRS's projected coordinate system |

Demonstrates the use of server-defined named CRS strings in addition to standard EPSG/AUTO2 codes.

![wms_getmap_crs_scandinavia](images/wms/wms_getmap_crs_scandinavia.png)

---

## GetMap — Time interpolation

### wms_getmap_svgxml_interpolated_time

**Input:** [`test/input/wms_getmap_svgxml_interpolated_time.get`](../test/input/wms_getmap_svgxml_interpolated_time.get)

```
GET /wms?...&layers=ely:wmsmap&time=200808050330 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `time` | `200808050330` | 03:30 UTC — halfway between data timesteps at 03:00 and 04:00 |

Tests that the server correctly interpolates or snaps to the nearest available data timestep when the requested time does not exactly match a data timestep.

![wms_getmap_svgxml_interpolated_time](images/wms/wms_getmap_svgxml_interpolated_time.png)

---

## GetMap — Time intervals

The `flash15min` layer stores data in 15-minute slots.  The `time` parameter can specify either a point in time or an interval using ISO 8601 duration syntax.

| Test | `time` value | Description |
|------|-------------|-------------|
| `wms_getmap_interval_default` | `201308051200` | Point in time; server uses the default aggregation interval |
| `wms_getmap_interval_start_30m` | `201308051200/PT30M` | 30-minute window starting at 12:00 |
| `wms_getmap_interval_end_30m` | `PT30M/201308051200` | 30-minute window ending at 12:00 |
| `wms_getmap_interval_30m30m` | `PT30M/201308051200/PT30M` | 30-minute window centred at 12:00 |

![wms_getmap_interval_default](images/wms/wms_getmap_interval_default.png)

---

## GetMap — GeoJSON output

### wms_getmap_geojson

**Input:** [`test/input/wms_getmap_geojson.get`](../test/input/wms_getmap_geojson.get)

```
GET /wms?service=wms&request=GetMap&version=1.3.0
    &layers=test:precipitation_areas&styles=
    &crs=EPSG:4326&bbox=59,17,71,34
    &width=300&height=500
    &format=application/geo%2Bjson
    &time=20080805120000 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `format` | `application/geo+json` | Returns isoband geometry as a GeoJSON FeatureCollection |

Isobands are returned as GeoJSON MultiPolygon features with the band's lower/upper limits as properties.  Used for programmatic access to contour geometry.

**Output:** [`test/output/wms_getmap_geojson.get`](../test/output/wms_getmap_geojson.get) — GeoJSON

### wms_getmap_geojson_precision

Same but tests that coordinate precision is controlled by the `precision` configuration setting.

**Output:** [`test/output/wms_getmap_geojson_precision.get`](../test/output/wms_getmap_geojson_precision.get) — GeoJSON

---

## GetMap — PDF output

### wms_getmap_pdf

**Input:** [`test/input/wms_getmap_pdf.get`](../test/input/wms_getmap_pdf.get)

```
GET /wms?...&layers=ely:wmsmap&format=application/pdf&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `format` | `application/pdf` | Returns the map as a PDF vector document |

**Output:** [`test/output/wms_getmap_pdf.get`](../test/output/wms_getmap_pdf.get) — PDF

---

## GetMap — Observation data

### wms_getmap_netatmo_observations

**Input:** [`test/input/wms_getmap_netatmo_observations.get`](../test/input/wms_getmap_netatmo_observations.get)

```
GET /wms?...&layers=test:netatmo5min&crs=EPSG:4326
    &bbox=53.5,3.5,72,33&width=600&height=800
    &format=image/png&time=20190325090500 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:netatmo5min` | Netatmo citizen weather station observations |
| `time` | `20190325090500` | Observation time (5-minute data) |

Renders crowd-sourced Netatmo temperature observations as coloured dots.

![wms_getmap_netatmo_observations](images/wms/wms_getmap_netatmo_observations.png)

### wms_getmap_roadcloud_observations

Same pattern for RoadCloud road-condition sensor data.

![wms_getmap_roadcloud_observations](images/wms/wms_getmap_roadcloud_observations.png)

---

## GetMap — Variants

The `variant` layer prefix selects an alternative data producer using the variant layer syntax `variant:{variant_name}:{layer_name}`.

| Test | `layers` | Description |
|------|----------|-------------|
| `wms_getmap_variant_pal` | `variant:pal:temperature` | Temperature layer using the `pal` data producer variant |
| `wms_getmap_variant_ground` | `variant:ground:groundtemperature` | Ground temperature from the `ground` variant |
| `wms_getmap_nonvariant_pal` | `test:nonvariant_pal` | Same data but as a regular (non-variant) layer |
| `wms_getmap_nonvariant_ground` | `test:nonvariant_ground` | Same data but as a regular (non-variant) layer |

---

## GetMap — Frame layer

### wms_getmap_frame

**Input:** [`test/input/wms_getmap_frame.get`](../test/input/wms_getmap_frame.get)

```
GET /wms?...&layers=test:frame_layer&crs=EPSG:4326
    &bbox=51,8,68,32&width=1800&height=2500
    &format=image/svg%2Bxml&time=current HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:frame_layer` | Ice-map style frame with inner/outer borders and tick-mark scale |
| `bbox` | `51,8,68,32` | Geographic bounding box (lat/lon) |
| `time` | `current` | Use the most recent available data timestep |

The `frame` layer type draws a decorative border with inner and outer rectangular frames and a graduated scale with tick marks and labels.

Product config: [`test/wms/customers/test/products/frame_layer.json`](../test/wms/customers/test/products/frame_layer.json)

![wms_getmap_frame](images/wms/wms_getmap_frame.png)

---

## GetMap — Ice chart

### wms_icechart_fmi_color

**Input:** [`test/input/wms_icechart_fmi_color.get`](../test/input/wms_icechart_fmi_color.get)

```
GET /wms?...&layers=test:icechart_fmi_color&crs=EPSG:4326
    &bbox=51,6,69,33&width=2000&height=2500
    &format=image/svg%2Bxml&time=20120227142200 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:icechart_fmi_color` | Baltic Sea ice chart with FMI colour scheme |
| `time` | `20120227142200` | Winter 2012 ice analysis time |

Renders Baltic Sea ice coverage using the FMI ice-chart color conventions.

![wms_icechart_fmi_color](images/wms/wms_icechart_fmi_color.png)

### wms_icechart_areas

**Input:** [`test/input/wms_icechart_areas.get`](../test/input/wms_icechart_areas.get)

Same ice chart data rendered as named area polygons rather than coloured fills.

![wms_icechart_areas](images/wms/wms_icechart_areas.png)

---

## GetMap — Symbol style

### wms_getmap_symbol_style

**Input:** [`test/input/wms_getmap_symbol_style.get`](../test/input/wms_getmap_symbol_style.get)

```
GET /wms?...&layers=test:precipitation&styles=precipitation_red_rain_style
    &crs=EPSG:4326&bbox=59,17,71,34&width=300&height=500
    &format=image/png&time=200808050300 HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `layers` | `test:precipitation` | Precipitation layer |
| `styles` | `precipitation_red_rain_style` | Custom style that uses a rain SVG symbol instead of filled isobands |

Demonstrates that an isoband layer can be rendered as repeated SVG symbols rather than filled polygons by specifying a symbol-based style.

![wms_getmap_symbol_style](images/wms/wms_getmap_symbol_style.png)

---

## GetMap — Error handling

When a request contains invalid parameters the server returns either an XML service exception (default) or an in-image error (when `EXCEPTIONS=INIMAGE` or format-specific variants are used).

| Test | Error | Response format |
|------|-------|-----------------|
| `wms_getmap_invalid_crs` | Unknown CRS | XML exception |
| `wms_getmap_invalid_crs_inimage_blank` | Unknown CRS | Blank PNG image |
| `wms_getmap_invalid_crs_inimage_png` | Unknown CRS | Error message PNG |
| `wms_getmap_invalid_crs_json` | Unknown CRS | JSON exception |
| `wms_getmap_invalid_layer` | Unknown layer | XML exception |
| `wms_getmap_invalid_layer_inimage_pdf` | Unknown layer | Error PDF |
| `wms_getmap_invalid_layer_json` | Unknown layer | JSON exception |
| `wms_getmap_invalid_style` | Unknown style | XML exception |
| `wms_getmap_invalid_style_inimage_png` | Unknown style | Error PNG |
| `wms_getmap_invalid_style_json` | Unknown style | JSON exception |
| `wms_getmap_invalid_time` | Invalid time | XML exception |
| `wms_getmap_invalid_time_inimage_pdf` | Invalid time | Error PDF |
| `wms_getmap_invalid_time_json` | Invalid time | JSON exception |
| `wms_getmap_invalid_version` | Unsupported version | XML exception |
| `wms_getmap_invalid_version_inimage_svg` | Unsupported version | Error SVG |
| `wms_getmap_invalid_version_json` | Unsupported version | JSON exception |
| `wms_getmap_invalid_format` | Unsupported format | XML exception |
| `wms_getmap_invalid_format_inimage_svg` | Unsupported format | Error SVG |

---

## GetFeatureInfo

GetFeatureInfo returns data values at a specified pixel location within a previously rendered map.

### wms_getfeatureinfo_json

**Input:** [`test/input/wms_getfeatureinfo_json.get`](../test/input/wms_getfeatureinfo_json.get)

```
GET /wms?service=wms&request=GetFeatureInfo&version=1.3.0
    &layers=test:frame_layer
    &query_layers=test:frame_layer
    &styles=
    &crs=EPSG:4326&bbox=51,8,68,32
    &width=1800&height=2500
    &format=image/svg%2Bxml
    &i=900&j=1250
    &info_format=application/json
    &time=current HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `request` | `GetFeatureInfo` | Returns data at a pixel position |
| `query_layers` | `test:frame_layer` | Layers to query (must be a subset of `layers`) |
| `i` | `900` | Pixel column in the image (0-based from left) |
| `j` | `1250` | Pixel row in the image (0-based from top) |
| `info_format` | `application/json` | Return data as JSON |

**Output:** [`test/output/wms_getfeatureinfo_json.get`](../test/output/wms_getfeatureinfo_json.get) — JSON

### wms_getfeatureinfo_html

Same request with `info_format=text/html`.

**Output:** [`test/output/wms_getfeatureinfo_html.get`](../test/output/wms_getfeatureinfo_html.get) — HTML

### wms_getfeatureinfo_precipitation_json

**Input:** [`test/input/wms_getfeatureinfo_precipitation_json.get`](../test/input/wms_getfeatureinfo_precipitation_json.get)

```
GET /wms?...&layers=test:precipitation_areas&query_layers=test:precipitation_areas
    &bbox=59,17,71,34&width=300&height=500
    &i=150&j=250&info_format=application/json
    &time=20080805120000 HTTP/1.0
```

Queries the precipitation isoband value at pixel (150, 250) — the centre of the map.

**Output:** [`test/output/wms_getfeatureinfo_precipitation_json.get`](../test/output/wms_getfeatureinfo_precipitation_json.get) — JSON

### wms_getfeatureinfo_numbers_json

**Input:** [`test/input/wms_getfeatureinfo_numbers_json.get`](../test/input/wms_getfeatureinfo_numbers_json.get)

Queries the temperature value at a station position from the `opendata_temperature_numbers_max` layer (uses the time-maximum aggregate).

**Output:** [`test/output/wms_getfeatureinfo_numbers_json.get`](../test/output/wms_getfeatureinfo_numbers_json.get) — JSON

Additional GetFeatureInfo tests:
- `wms_getfeatureinfo_missing_query_layers` — error when `query_layers` is omitted
- `wms_getfeatureinfo_precipitationform_symbols_json` — queries precipitation form symbols
- `wms_getfeatureinfo_precipitation_isoband_labels_json` — queries isoband label values
- `wms_getfeatureinfo_roadcondition_isoband_json` — queries road condition isobands
- `wms_getfeatureinfo_temperature_symbols_json` — queries temperature symbol data

---

## GetLegendGraphic

GetLegendGraphic returns a legend image for a given layer and style.

### wms_getlegendgraphic_automatic

**Input:** [`test/input/wms_getlegendgraphic_automatic.get`](../test/input/wms_getlegendgraphic_automatic.get)

```
GET /wms?service=WMS&request=GetLegendGraphic&version=1.3.0&sld_version=1.1.0
    &layer=test:precipitation&style=
    &format=image/svg%2Bxml HTTP/1.0
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `request` | `GetLegendGraphic` | Returns a legend image for the named layer/style |
| `layer` | `test:precipitation` | Layer to generate legend for |
| `style` | _(empty)_ | Default style |
| `sld_version` | `1.1.0` | SLD version (required) |
| `format` | `image/svg+xml` | SVG legend output |

Generates an automatic legend by reading isoband colour definitions from the product CSS.

![wms_getlegendgraphic_automatic](images/wms/wms_getlegendgraphic_automatic.png)

### wms_getlegendgraphic_automatic_isoband_style1

**Input:** [`test/input/wms_getlegendgraphic_automatic_isoband_style1.get`](../test/input/wms_getlegendgraphic_automatic_isoband_style1.get)

```
GET /wms?...&layer=test:t2m&style=temperature_one_degrees&format=image/svg%2Bxml HTTP/1.0
```

Legend for the `temperature_one_degrees` style (1°C isoband intervals).

![wms_getlegendgraphic_automatic_isoband_style1](images/wms/wms_getlegendgraphic_automatic_isoband_style1.png)

### wms_getlegendgraphic_internal_legend

**Input:** [`test/input/wms_getlegendgraphic_internal_legend.get`](../test/input/wms_getlegendgraphic_internal_legend.get)

```
GET /wms?...&layer=test:t2m_p&style=&format=image/svg%2Bxml HTTP/1.0
```

Uses an internally embedded legend definition from the product JSON instead of the automatic CSS-derived legend.

![wms_getlegendgraphic_internal_legend](images/wms/wms_getlegendgraphic_internal_legend.png)

### wms_getlegendgraphic_external_legend

**Input:** [`test/input/wms_getlegendgraphic_external_legend.get`](../test/input/wms_getlegendgraphic_external_legend.get)

```
GET /wms?...&layer=test:wind_legend&style=&format=image/svg%2Bxml HTTP/1.0
```

Uses an external SVG file as the legend image.

![wms_getlegendgraphic_external_legend](images/wms/wms_getlegendgraphic_external_legend.png)

Additional GetLegendGraphic tests cover: Finnish (`_fi`) and other language translations, modified legends, product-specific legend templates, isoband label legends, and isoline style legends.  See [`test/input/wms_getlegendgraphic_*.get`](../test/input/).

---

# WMTS Test Examples

The WMTS tests exercise the `/wmts` endpoint, which implements OGC Web Map Tile Service 1.0.0.  The REST-KVP hybrid URL path has the form:

```
/wmts/1.0.0/{layer}/{style}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.{format}
```

Test inputs are in [`test/input/wmts_*.get`](../test/input/) and expected outputs in [`test/output/wmts_*.get`](../test/output/).

## wmts_getcapabilities

**Input:** [`test/input/wmts_getcapabilities.get`](../test/input/wmts_getcapabilities.get)

```
GET /wmts/1.0.0/WMTSCapabilities.xml HTTP/1.0
```

Returns the WMTS 1.0.0 capabilities document listing all available tile layers, tile matrix sets, styles, and supported output formats.  Each layer advertises tile matrix sets for EPSG:3035, EPSG:3067, EPSG:3857, and EPSG:4326.

**Output:** [`test/output/wmts_getcapabilities.get`](../test/output/wmts_getcapabilities.get) — XML

## wmts_gettile_isoband

**Input:** [`test/input/wmts_gettile_isoband.get`](../test/input/wmts_gettile_isoband.get)

```
GET /wmts/1.0.0/test:t2m/temperature_one_degrees/EPSG:4326/5/4/36.png?TIME=20080805T030000 HTTP/1.0
```

| Path segment | Value | Description |
|--------------|-------|-------------|
| `test:t2m` | Layer | Temperature isoband layer in customer `test` |
| `temperature_one_degrees` | Style | 1°C isoband interval style |
| `EPSG:4326` | TileMatrixSet | WGS 84 tile grid |
| `5` | TileMatrix | Zoom level 5 |
| `4` | TileRow | Row 4 within the tile grid |
| `36` | TileCol | Column 36 within the tile grid |
| `.png` | Format | PNG output |
| `TIME` | `20080805T030000` | Valid time query parameter |

Returns a 1024×1024 PNG tile of temperature isobands.

**Output:** [`test/output/wmts_gettile_isoband.get`](../test/output/wmts_gettile_isoband.get) — PNG (1024×1024)

![wmts_gettile_isoband](images/wmts/wmts_gettile_isoband.png)

## wmts_gettile_temperature_numbers

**Input:** [`test/input/wmts_gettile_temperature_numbers.get`](../test/input/wmts_gettile_temperature_numbers.get)

```
GET /wmts/1.0.0/test:opendata_temperature_numbers/default/EPSG:4326/5/4/36.svg?TIME=20130805T1500 HTTP/1.0
```

| Path segment | Value | Description |
|--------------|-------|-------------|
| `test:opendata_temperature_numbers` | Layer | Temperature observation numbers |
| `default` | Style | Default style |
| `.svg` | Format | SVG output (vector tiles) |
| `TIME` | `20130805T1500` | Valid time |

Returns a 1024×1024 SVG tile containing temperature number annotations as vector graphics.

**Output:** [`test/output/wmts_gettile_temperature_numbers.get`](../test/output/wmts_gettile_temperature_numbers.get) — SVG

![wmts_gettile_temperature_numbers](images/wmts/wmts_gettile_temperature_numbers.png)

## wmts_gettile_geotiff

**Input:** [`test/input/wmts_gettile_geotiff.get`](../test/input/wmts_gettile_geotiff.get)

```
GET /wmts/1.0.0/grid:raster_1/default/EPSG:4326/5/4/36.tiff?TIME=20080805T080000 HTTP/1.0
```

| Path segment | Value | Description |
|--------------|-------|-------------|
| `grid:raster_1` | Layer | Raw gridded raster data layer |
| `.tiff` | Format | GeoTIFF raster tile |
| `TIME` | `20080805T080000` | Valid time |

Returns a GeoTIFF tile containing raw numerical grid data (not rendered colour).  Used for programmatic access to model data in raster form.

**Output:** [`test/output/wmts_gettile_geotiff.get`](../test/output/wmts_gettile_geotiff.get) — GeoTIFF (metadata/header only in the test)

---

# OGC Tiles Test Examples

The OGC Tiles tests exercise the `/tiles` endpoint, which implements the OGC API — Tiles standard.  This is the RESTful successor to WMTS using the collections model.  The URL path structure is:

```
/tiles/collections/{collection}/tiles/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}?f={format}&TIME={time}
```

Test inputs are in [`test/input/tiles_*.get`](../test/input/) and expected outputs in [`test/output/tiles_*.get`](../test/output/).

## tiles_getcollections

**Input:** [`test/input/tiles_getcollections.get`](../test/input/tiles_getcollections.get)

```
GET /tiles/collections HTTP/1.0
```

Returns a catalogue of all available tile collections (layers), with their identifiers, titles, extent, and supported tile matrix sets and formats.

**Output:** [`test/output/tiles_getcollections.get`](../test/output/tiles_getcollections.get) — XML/JSON

## tiles_gettile_isoband

**Input:** [`test/input/tiles_gettile_isoband.get`](../test/input/tiles_gettile_isoband.get)

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

**Output:** [`test/output/tiles_gettile_isoband.get`](../test/output/tiles_gettile_isoband.get) — PNG (1024×1024)

![tiles_gettile_isoband](images/tiles/tiles_gettile_isoband.png)

## tiles_gettile_temperature_numbers

**Input:** [`test/input/tiles_gettile_temperature_numbers.get`](../test/input/tiles_gettile_temperature_numbers.get)

```
GET /tiles/collections/test:opendata_temperature_numbers/tiles/EPSG:4326/5/4/36?f=svg&TIME=20130805T1500 HTTP/1.0
```

Returns a 1024×1024 SVG tile of temperature number annotations.

**Output:** [`test/output/tiles_gettile_temperature_numbers.get`](../test/output/tiles_gettile_temperature_numbers.get) — SVG

![tiles_gettile_temperature_numbers](images/tiles/tiles_gettile_temperature_numbers.png)

## tiles_gettile_geotiff

**Input:** [`test/input/tiles_gettile_geotiff.get`](../test/input/tiles_gettile_geotiff.get)

```
GET /tiles/collections/grid:raster_1/tiles/EPSG:4326/5/4/36?f=tiff&TIME=20080805T080000 HTTP/1.0
```

Returns a GeoTIFF tile of raw numerical grid data.

**Output:** [`test/output/tiles_gettile_geotiff.get`](../test/output/tiles_gettile_geotiff.get) — GeoTIFF

## tiles_gettile_geotiff_wind_speed_and_direction_1 and _2

These tests retrieve GeoTIFF tiles for a multi-band wind product (`grid:wind_speed_and_direction`).  Two variants test different band configurations:

```
GET /tiles/collections/grid:wind_speed_and_direction_1/tiles/EPSG:4326/5/4/36?f=tiff&TIME=20080805T080000 HTTP/1.0
GET /tiles/collections/grid:wind_speed_and_direction_2/tiles/EPSG:4326/5/4/36?f=tiff&TIME=20080805T080000 HTTP/1.0
```

Wind speed and direction are encoded in separate GeoTIFF bands, allowing clients to reconstruct vector wind fields.

**Output:** [`test/output/tiles_gettile_geotiff_wind_speed_and_direction_1.get`](../test/output/tiles_gettile_geotiff_wind_speed_and_direction_1.get) / [`_2`](../test/output/tiles_gettile_geotiff_wind_speed_and_direction_2.get) — GeoTIFF

## Mapbox Vector Tile (MVT) outputs

MVT is a compact binary format for encoding vector geometry data in tiles, widely used by Mapbox, OpenLayers, and other mapping clients.

### tiles_gettile_mvt_isoband

**Input:** [`test/input/tiles_gettile_mvt_isoband.get`](../test/input/tiles_gettile_mvt_isoband.get)

```
GET /tiles/collections/test:t2m/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

| Query segment | Value | Description |
|---------------|-------|-------------|
| `f` | `mvt` | Mapbox Vector Tile binary format |

Returns temperature isoband polygons encoded as an MVT tile.  The isoband geometry is quantised and delta-encoded per the MVT specification.

**Output:** [`test/output/tiles_gettile_mvt_isoband.get`](../test/output/tiles_gettile_mvt_isoband.get) — MVT (binary)

### tiles_gettile_mvt_isoline

**Input:** [`test/input/tiles_gettile_mvt_isoline.get`](../test/input/tiles_gettile_mvt_isoline.get)

```
GET /tiles/collections/test:t2m_p/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

Returns temperature isolines as MVT `LineString` features.

**Output:** [`test/output/tiles_gettile_mvt_isoline.get`](../test/output/tiles_gettile_mvt_isoline.get) — MVT (binary)

### tiles_gettile_mvt_numbers

**Input:** [`test/input/tiles_gettile_mvt_numbers.get`](../test/input/tiles_gettile_mvt_numbers.get)

```
GET /tiles/collections/test:t2m_numbers/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

Returns temperature number positions and values as MVT `Point` features with the numeric value as an attribute.

**Output:** [`test/output/tiles_gettile_mvt_numbers.get`](../test/output/tiles_gettile_mvt_numbers.get) — MVT (binary)

### tiles_gettile_mvt_circles

**Input:** [`test/input/tiles_gettile_mvt_circles.get`](../test/input/tiles_gettile_mvt_circles.get)

```
GET /tiles/collections/test:t2m_circles/tiles/EPSG:4326/5/4/36?f=mvt&TIME=20080805T030000 HTTP/1.0
```

Returns circle-layer data (station point positions with radius attributes) as MVT `Point` features.

**Output:** [`test/output/tiles_gettile_mvt_circles.get`](../test/output/tiles_gettile_mvt_circles.get) — MVT (binary)
