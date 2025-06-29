%bcond_without authentication
%bcond_without observation
%define DIRNAME wms
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet WMS/Dali plugin
Name: %{SPECNAME}
Version: 25.6.18
Release: 3%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-wms
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
%if 0%{?rhel} && 0%{rhel} < 9
%define smartmet_boost boost169
%else
%define smartmet_boost boost
%endif

%if 0%{?rhel} && 0%{rhel} <= 9
%define smartmet_fmt_min 11.0.1
%define smartmet_fmt_max 12.0.0
%define smartmet_fmt fmt-libs >= %{smartmet_fmt_min}, fmt-libs < %{smartmet_fmt_max}
%define smartmet_fmt_devel fmt-devel >= %{smartmet_fmt_min}, fmt-devel < %{smartmet_fmt_max}
%else
%define smartmet_fmt fmt
%define smartmet_fmt_devel fmt-devel
%endif

BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: %{smartmet_boost}-devel
BuildRequires: rpm-build
BuildRequires: smartmet-library-giza-devel >= 25.2.18
BuildRequires: smartmet-library-grid-content-devel >= 25.5.22
BuildRequires: smartmet-library-grid-files-devel >= 25.5.30
BuildRequires: smartmet-library-macgyver-devel >= 25.5.30
BuildRequires: smartmet-library-spine-devel >= 25.5.13
BuildRequires: smartmet-library-timeseries-devel >= 25.6.9
%if %{with authentication}
BuildRequires: smartmet-engine-authentication-devel >= 25.2.18
%endif
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 25.6.16
%endif
BuildRequires: smartmet-engine-gis-devel >= 25.2.18
BuildRequires: smartmet-engine-grid-devel >= 25.6.3
BuildRequires: smartmet-engine-geonames-devel >= 25.2.18
BuildRequires: smartmet-engine-querydata-devel >= 25.6.17
BuildRequires: smartmet-engine-contour-devel >= 25.2.18
BuildRequires: smartmet-library-gis-devel >= 25.2.18
BuildRequires: smartmet-library-trax-devel >= 25.4.11
BuildRequires: %{smartmet_fmt_devel}
BuildRequires: ctpp2 >= 2.8.8
BuildRequires: jsoncpp-devel
# BuildRequires: flex-devel
BuildRequires: cairo-devel
BuildRequires: bzip2-devel
BuildRequires: libconfig17-devel
BuildRequires: heatmap-devel
%if %{defined el7}
BuildRequires: librsvg2-devel = 2.40.6
#TestRequires: librsvg2-devel = 2.40.6
#TestRequires: librsvg2-tools = 2.40.6
%else
BuildRequires: librsvg2-devel
#TestRequires: librsvg2-devel
#TestRequires: librsvg2-tools
%endif
Requires: cairo
Requires: %{smartmet_fmt}
Requires: jsoncpp
Requires: ctpp2 >= 2.8.8
Requires: libconfig17
# Default font for some layers:
Requires: google-roboto-fonts
Requires: smartmet-library-grid-content >= 25.5.22
Requires: smartmet-library-grid-files >= 25.5.30
Requires: smartmet-library-gis >= 25.2.18
Requires: smartmet-library-trax >= 25.4.11
Requires: smartmet-library-macgyver >= 25.5.30
Requires: smartmet-library-spine >= 25.5.13
Requires: smartmet-library-timeseries >= 25.6.9
Requires: smartmet-library-giza >= 25.2.18
%if %{with authentication}
Requires: smartmet-engine-authentication >= 25.2.18
%endif
Requires: smartmet-engine-querydata >= 25.6.17
Requires: smartmet-engine-contour >= 25.2.18
Requires: smartmet-engine-gis >= 25.2.18
Requires: smartmet-engine-grid >= 25.6.3
Requires: smartmet-engine-geonames >= 25.2.18
Requires: smartmet-server >= 25.5.13
Requires: smartmet-library-spine >= 25.5.13
Requires: smartmet-fonts
Requires: %{smartmet_boost}-filesystem
Requires: %{smartmet_boost}-iostreams
Requires: %{smartmet_boost}-regex
Requires: %{smartmet_boost}-system
Requires: %{smartmet_boost}-thread
Provides: %{SPECNAME}
Obsoletes: smartmet-brainstorm-dali < 16.11.1
Obsoletes: smartmet-brainstorm-dali-debuginfo < 16.11.1
#TestRequires: %{smartmet_boost}-devel
#TestRequires: bzip2-devel
#TestRequires: fmt-devel
#TestRequires: gcc-c++
#TestRequires: jsoncpp-devel
#TestRequires: libconfig17-devel
#TestRequires: ImageMagick
#TestRequires: bc
#TestRequires: smartmet-engine-contour-devel >= 25.2.18
#TestRequires: smartmet-engine-geonames-devel >= 25.2.18
#TestRequires: smartmet-engine-gis-devel >= 25.2.18
#TestRequires: smartmet-engine-querydata-devel >= 25.6.17
#TestRequires: smartmet-library-giza-devel >= 25.2.18
#TestRequires: smartmet-library-trax-devel >= 25.4.11
#TestRequires: smartmet-library-newbase-devel >= 25.3.20
#TestRequires: smartmet-library-macgyver-devel >= 25.5.30
#TestRequires: smartmet-library-spine-devel >= 25.5.13
#TestRequires: smartmet-library-timeseries-devel >= 25.6.9
#TestRequires: smartmet-engine-grid-devel >= 25.6.3
#TestRequires: smartmet-engine-grid-test
#TestRequires: smartmet-test-data
#TestRequires: smartmet-test-db
#TestRequires: smartmet-fonts
#TestRequires: libconfig17-devel
#TestRequires: google-roboto-fonts
#TestRequires: zlib-devel
#TestRequires: libcurl-devel
#TestRequires: cairo-devel
#TestRequires: redis
%if %{with observation}
#TestRequires: smartmet-engine-observation-devel >= 25.6.16
%endif

%description
SmartMet WMS/Dali plugin

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{SPECNAME}

%build -q -n %{SPECNAME}
make %{_smp_mflags} \
     %{?!with_authentication:CFLAGS=-DWITHOUT_AUTHENTICATION} \
     %{?!with_observation:CFLAGS=-DWITHOUT_OBSERVATION}

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(0775,root,root,0775)
%{_datadir}/smartmet/plugins/wms.so
%defattr(0664,root,root,0775)
%{_datadir}/smartmet/wms/*.c2t

%changelog
* Wed Jun 18 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.6.18-3.fmi
- Added a safety check for the number of requested layers

* Wed Jun 18 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.6.18-2.fmi
- Fixed typo

* Wed Jun 18 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.6.18-1.fmi
- Added more sanity checks for requested image size

* Thu Jun 12 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.6.12-1.fmi
- Ignore GRID generations that do not contain timesteps for the requested parameter in GetCapabilities responses

* Wed Jun 11 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.6.11-1.fmi
- Automatically detect constant step in GetCapabilities elevation dimension output

* Sat May 31 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.5.31-1.fmi
- Fixed WMSElevationDimension to initialize the default elevation in both constructors

* Fri May 30 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.5.30-1.fmi
- Define WMS default elevation from the JSON configuration if set

* Tue May 13 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.5.13-2.fmi
- Improved hash counting for parameters that contain producer definition

* Tue May 13 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.5.13-1.fmi
- Fixed CircleLayer keyword setting
- Added CircleLayer geoids setting

* Wed May  7 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.5.7-1.fmi
- Do not stop producing variant WMS layers if one of them for example lacks required data

* Fri Apr 25 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.4.25-1.fmi
- Added optional labels for CircleLayer
- Fixed icemap fontsize_column handling, it was assigned incorrectly to fontname_column
- Fixed areaunits to work for minarea settings in GRID queries
- Silenced some compiler warnings

* Thu Apr 24 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.4.24-2.fmi
- Removed 'upright' setting and added 'orientation' setting for GraticuleLayer

* Thu Apr 24 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.4.24-1.fmi
- Added 'textattributes' setting for IsolabelLayer

* Thu Apr 10 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.4.10-1.fmi
- Added GraticuleLayer

* Wed Apr  9 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.4.9-1.fmi
- Improved GRID geometry selection algorithm

* Tue Apr  8 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.4.8-1.fmi
- Using geometry groups for generating time steps for producers that have multiple geometries

* Fri Mar 28 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.3.28-1.fmi
- Fixed CircleLayer styles to work

* Fri Mar 21 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.3.21-1.fmi
- Better support for symbol/isoline layer label conversions

* Wed Mar 19 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.3.19-1.fmi
- Improved producer's name search inside a function parameter

* Thu Feb 27 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.2.27-1.fmi
- Added "features" and "lines" settings to CircleLayer
- Remove duplicate locations from CircleLayer name searches

* Tue Feb 18 2025 Andris Pavēnis <andris.pavenis@fmi.fi> 25.2.18-1.fmi
- Update to gdal-3.10, geos-3.13 and proj-9.5

* Wed Jan 29 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.1.29-1.fmi
- Repackaged due to ABI changes

* Thu Jan  9 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.1.9-1.fmi
- Repackaged due to GRID library changes

* Thu Jan  2 2025 Mika Heiskanen <mika.heiskanen@fmi.fi> - 25.1.2-1.fmi
- Added CircleLayer for drawing circles around airports etc

* Tue Dec 17 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.12.17-1.fmi
- Fixed variant title translations

* Thu Dec  5 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.12.5-1.fmi
- Enable debug prints for SQL generated by observation requests

* Wed Dec  4 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.12.4-1.fmi
- Make it easier to overlay layers by automatically moving producer settings to the view level

* Thu Nov 28 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.11.28-1.fmi
- Separate GRID-engine area and contour interpolation methods

* Fri Nov 22 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.11.22-1.fmi
- Update TimeSeries aggregation according smartmet-library-timeseries ABI changes

* Thu Nov 21 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.11.21-1.fmi
- Fixed GetCapabilities to use default language when needed

* Tue Nov 12 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.11.12-1.fmi
- Use new SpatialReference::WKT() accessor for better speed

* Fri Nov  8 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.11.8-1.fmi
- Repackage due to smartmet-library-spine ABI changes

* Thu Oct 31 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.31-1.fmi
- Improved contouring speed for GRIB files

* Wed Oct 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.23-2.fmi
- Fixed GetCapabilities not to print a reference_time for multifiles
- Enable printhash option for GetCapabilities requests

* Wed Oct 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.23-1.fmi
- Repackaged due to ABI changes

* Wed Oct 16 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.16-1.fmi
- Repackaged due to grid library ABI changes

* Tue Oct 15 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.15-1.fmi
- Removed landscaping as obsolete

* Fri Oct  4 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.10.4-1.fmi
- Fixed GetCapabilities to handle levelId, forecastType and geometryId settings properly
- Fixed GetCapabilities to handle arrow layer parameters

* Fri Sep 20 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.9.20-2.fmi
- Added a "margin" setting for Positions

* Fri Sep 20 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.9.20-1.fmi
- Fix to levelId handling
- Fixes to location generation for symbol/number/arrow layers

* Tue Sep  3 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.9.3-1.fmi
- Repackage due smartmlibrary-grid-data and smartmet-engine-querydata changes

* Thu Aug 29 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.8.29-1.fmi
- Warn only once on duplicate qids per URL to remove repeated error messages

* Wed Aug  7 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.8.7-1.fmi
- Update to gdal-3.8, geos-3.12, proj-94 and fmt-11

* Thu Aug  1 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.8.1-1.fmi
- Changed to use new QEngine API for resampling data

* Tue Jul 30 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.30-2.fmi
- IsoBandLayer: update according Engine::Querydata::Model ABI change

* Tue Jul 30 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.7.30-1.fmi
- Warn if some qid is used several times to aid JSON developers

* Mon Jul 29 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.7.29-2.fmi
- Fixed GRIB contours to apply the fiter settings
- Fixed location of minarea code

* Mon Jul 29 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.7.29-1.fmi
- Fixed margin to define a closed range instead of a half open one

* Mon Jul 22 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.22-1.fmi
- Rebuild dues to smartmet-library-macgyver changes

* Wed Jul 17 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.17-1.fmi
- Do not link with libboost_filesystem

* Fri Jul 12 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.7.12-1.fmi
- Replace many boost library types with C++ standard library ones

* Thu Jun  6 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.6.6-1.fmi
- Fixes to elevation dimension handling

* Tue Jun  4 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.6.4-1.fmi
- Enable caching of SymbolLayers if the requested time is more than 5 minutes old
- Restored level querystring parameter to work

* Mon Jun  3 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.6.3-1.fmi
- Repackaged due to ABI changes

* Fri May 31 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.31-1.fmi
- NullLayer will no longer generate warnings on unused settings

* Thu May 30 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.30-1.fmi
- Fixed parameter name comparison when picking query results
- Added elevation dimension information to GetCapabilities

* Tue May 28 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.28-1.fmi
- Remove uses of LocalTimePool

* Thu May 16 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.16-1.fmi
- Clean up boost date-time uses

* Mon May 13 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.13-1.fmi
- Fixed conflict in using height parameter in two different ways (not released)

* Wed May  8 2024 Andris Pavēnis <andris.pavenis@fmi.fi> 24.5.8-1.fmi
- Use Date library (https://github.com/HowardHinnant/date) instead of boost date_time

* Fri May  3 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.5.3-1.fmi
- Repackaged due to GRID library changes

* Wed Apr 24 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.24-1.fmi
- Refactored code: use GeometrySmoother from GIS-library

* Mon Apr 22 2024 Mika Heiskanen <mheiskan@rhel8.dev.fmi.fi> - 24.4.22-1.fmi
- Fixed boost::optional comparisons to use std::string instead of char *

* Wed Apr 17 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.17-2.fmi
- Do not show apikey in GetCapabilities if omit-fmi-apikey header is provided

* Wed Apr 17 2024 Mika Heiskanen <mheiskan@rhel8.dev.fmi.fi> - 24.4.17-1.fmi
- Work around bug in PROJ projection cloning

* Wed Apr 10 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.10-1.fmi
- Avoid reprojecting sampled data

* Mon Apr  8 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.4.8-1.fmi
- Repackaged since Spine::Station ABI changed

* Tue Mar  5 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.3.5-1.fmi
- Added max_image_size setting with default value of 20M

* Thu Feb 29 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.2.29-1.fmi
- Selecting a style for a null layer no longer deletes the old settings since they are intentional defaults

* Tue Feb 27 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.2.27-1.fmi
- Improved error checking and error reports

* Fri Feb 23 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.23-1.fmi
- Full repackaging

* Tue Feb 20 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.20-2.fmi
- Repackaged due to grid-files ABI changes

* Tue Feb 20 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.20-1.fmi
- Repackaged due to grid-files ABI changes

* Mon Feb  5 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.2.5-1.fmi
- Repackaged due to grid-files ABI changes

* Tue Jan 30 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> 24.1.30-1.fmi
- Repackaged due to newbase ABI changes

* Thu Jan  4 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.1.4-1.fmi
- Repackaged due to TimeSeriesGeneratorOptions ABI changes

* Wed Jan  3 2024 Mika Heiskanen <mika.heiskanen@fmi.fi> - 24.1.3-1.fmi
- Added 'areaunit' setting for isoband/isoline 'minarea' setting with possible values km^2 and px^2

* Fri Dec 22 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.22-1.fmi
- Repackaged due to ThreadLock ABI changes

* Fri Dec  8 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.8-1.fmi
- Optimized observation reading for speed

* Tue Dec  5 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.5-1.fmi
- Repackaged due to an ABI change in SmartMetPlugin

* Mon Dec  4 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.12.4-1.fmi
- Repackaged due to QEngine ABI changes

* Thu Nov 23 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.23-1.fmi
- Fixed GetCapabilities isValidTime test to handle data with broken intervals

* Tue Nov 21 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.21-1.fmi
- Added a cache for calculated latlon bounding boxes corresponding to WMS input BBOX parameter (BRAINSTORM-2790)

* Fri Nov 17 2023 Pertti Kinnia <pertti.kinnia@fmi.fi> - 23.11.17-1.fmi
- Repackaged due to API changes in grid-files and grid-content

* Thu Nov 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.16-1.fmi
- Improved isolabel rectangle distance calculations by taking the center point into account

* Fri Nov 10 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.10-1.fmi
- Repackaged due to API changes in grid-content

* Thu Nov  9 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.11.9-1.fmi
- Improved isolabel placement for appearance and optimized for speed

* Mon Oct 30 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.30-1.fmi
- Added show_hidden option to GetCapabilities request, hidden layers can also be requested with GetMap request (BRAINSTORM-2768)
- Improved add_sublayers algorithm in WMSLayerHierarchy.cpp file (BRAINSTORM-2766)
- Fixed GetCapabilities endtime of observation layers (BRAINSTORM-2452)

* Sat Oct 21 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.21-1.fmi
- Fixed default templatedir value to /usr/share/smartmet/wms

* Fri Oct 20 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.20-1.fmi
- Added min_isoline_length setting for IsolabelLayer with default value 150 (~ radius of 24 pixels)

* Thu Oct 19 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.19-2.fmi
- Move templates from /etc to /usr/share

* Thu Oct 19 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.19-1.fmi
- Optimized IsolabelLayer for speed

* Wed Oct 18 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.10.18-1.fmi
- Fixed parsing of observation parameters (BRAINSTORM-2763)

* Thu Oct 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.10.12-1.fmi
- Repackage due to smartmet-library-grid-files and smartmet-library-grid-files changes

* Wed Oct 11 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.11-1.fmi
- Aggregation support added (BRAINSTORM-2457)
- Fixed GetCapabilities to ignore incomplete GRIB generations

* Thu Oct 5 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.10.5-1.fmi
- Fixed layout=recursive bug when namespace-option and layer name are identical (BRAINSTORM-2742)

* Fri Sep 29 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.29-1.fmi
- Repackaged due to changes in grid libraries

* Fri Sep 15 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.15-1.fmi
- Fixed producer, source and geometryId extraction (BRAINSTORM-2703)

* Mon Sep 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.11-1.fmi
- Repackaged due to ABI changes in grid-files

* Wed Sep  6 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.9.4-2.fmi
- Allow gaps between time-intervals in GetCapabilities response document (BRAINSTORM-2713)

* Wed Sep  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.9.6-1.fmi
- Fixed typo in config setting: primarytForecastSource --> primaryForecastSource

* Mon Sep 4 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.9.4-1.fmi
- Allow gaps between time-intervals in GetCapabilities response document (BRAINSTORM-2713)

* Thu Aug 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.8.31-2.fmi
- Cache sizes in bytes can now be defined with strings of form "10G"

* Thu Aug 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.8.31-1.fmi
- A WMS style no longer needs to set layer_type
- Symbol priority list now specifies the rendering order of the symbols, unlisted symbols have lowest priority
- Better support for symbols in legend layer (BRAINSTORM-2585)
- Read producer from views,layers if not found on root level in layer file (BRAINSTORM-2703)
- Added "rendering_order = normal|reverse" for symbol, number and arrow  layers

* Mon Aug 28 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.8.28-1.fmi
- Added BBOX to Contour::Options to handle Pacific WebMercator views

* Thu Aug 24 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.8.24-1.fmi
- Enable WMS styles to disable settings with null values (BRAINSTORM-2697)

* Thu Aug 3 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.8.3-1.fmi
- Fixed handling of styles of external legends in product file's alternative styles section (BRAINSTORM-2585, BRAINSTORM-2496)

* Wed Aug 2 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.8.2-1.fmi
- Enabling alternative styles for external legends (BRAINSTORM-2585)

* Fri Jul 28 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.28-1.fmi
- Repackage due to bulk ABI changes in macgyver/newbase/spine

* Tue Jul 25 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.25-1.fmi
- Added desliver option for sliver removal

* Fri Jul 14 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.14-1.fmi
- Enabled modifying closed_range/validate/strict settings for contouring

* Wed Jul 12 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.7.12-1.fmi
- Use postgresql 15, gdal 3.5, geos 3.11 and proj-9.0

* Tue Jul 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.7.11-1.fmi
- Improved error handling
- Fixed JSON substitutions not to require JSON toString()
- Enable generating symbols for missing values
- Fixed handling of TIME=current requests

* Thu Jun 15 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.15-1.fmi
- Optimized Locations class for speed

* Tue Jun 13 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.13-1.fmi
- Support internal and environment variables in configuration files

* Wed Jun  7 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.7-1.fmi
- Fixed bounding box calculations to sample the full grid since the poles may be inside the image area

* Tue Jun  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.6-3.fmi
- Fixed querydata geographic bounding box to be based on all edge points instead of just the corner points (BRAINSTORM-2639)

* Tue Jun  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.6-2.fmi
- Added xmlescape calls where necessary to handle CDATA segments properly

* Tue Jun  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.6-1.fmi
- Repackaged due to GRID ABI changes

* Thu Jun  1 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.1-2.fmi
- Recompiled to use to ObsEngine API with stationgroups options

* Thu Jun  1 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.6.1-1.fmi
- More accurate error reports on WMS layers (full paths)

* Tue May 30 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.30-1.fmi
- Fixed potential segfault

* Mon May 29 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.29-1.fmi
- Added support for layer variants

* Mon May 22 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.22-1.fmi
- Handle legend_url_layer removal properly
- Added timestamps to JSON configuration error messages

* Wed May 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.17-3.fmi
- Added "autoqid" and "autoclass" settings for isobands and isolines to avoid repetitive typing

* Wed May 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.17-2.fmi
- Fixed PresentWeatherObservationLayer to use parameter WAWA_PT1M_RANK in order to get foreign WMS layers to work

* Wed May 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.17-1.fmi
- Improved ObservationLayer code to handle position/label/maxdistance etc settings
- Rewrote observation reading for speed

* Thu May 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.11-1.fmi
- Added ObservationReader to simplify number, symbol and arrow layer code

* Wed May 10 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.10-1.fmi
- Use the latest obs engine API for looking for the desired time given a time interval

* Tue May  9 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.5.9-1.fmi
- Improved language support for GetCapabilities and GetLegendGraphic requests

* Thu Apr 27 2023 Andris Pavēnis <andris.pavenis@fmi.fi> 23.4.27-1.fmi
- Repackage due to macgyver ABI changes (AsyncTask, AsyncTaskGroup)

* Thu Apr 20 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.4.20-1.fmi
- Fixed observation layer keyword handling

* Mon Apr 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.4.17-2.fmi
- Fixed Properties to handle "level" setting even when elevation is given in the query string

* Mon Apr 17 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.4.17-1.fmi
- Repackaged due to GRID ABI changes

* Tue Apr 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.4.11-2.fmi
- Both isoband limits can now be set simultaneously with value instead of using lolimit and hilimit

* Tue Apr 11 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.4.11-1.fmi
- Improved JSON validation
- Added NullLayer with layer_type=null to enable styles to add new layers in a clean manner
- If the WMS STYLES option changes the type of a layer, the old settings will be completely erased instead of just overwritten
- Minor fixes

* Wed Mar 29 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.29-1.fmi
- Fixed ymargin parsing

* Thu Mar 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.16-1.fmi
- Disable full GetMap stack traces on user input errors
- Disable logging GetMap errors on user input errors in quiet mode (release servers)

* Wed Mar 8 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.3.8-1.fmi
- Fixed variable name from CLHB5_PT1M_INSTANT to CLH5_PT1M_INSTANT

* Mon Mar  6 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.3.6-1.fmi
- Avoid using boost::lexical_cast to avoid GNU global locale locks
- Silenced several CodeChecker warnings

* Thu Mar 2 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.3.2-1.fmi
- Improved CloudCeilingLayer: If no keyword or fmisid configured bbox+producer is used to get stations

* Fri Feb 24 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.24-1.fmi
- Fixed WMS queries to report the host name

* Thu Feb 23 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.2.23-3.fmi
- Added CloudCeilingLayer (BRAINSTORM-2428)

* Thu Feb 23 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.23-2.fmi
- Enable default settings for replaceable variables in SVG elements

* Thu Feb 23 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.23-1.fmi
- Added support for U/V components in the streamline layer

* Mon Feb 20 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.20-2.fmi
- Unified streamline layer parameter naming style

* Mon Feb 20 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.20-1.fmi
- Added streamline layer

* Mon Feb 13 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.13-1.fmi
- Prefer a .svg suffix for SVG markers etc

* Wed Feb  8 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.2.8-1.fmi
- Add host name to stack traces

* Tue Jan 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.31-2.fmi
- Fixed IceMapLayer to omit empty geometries

* Tue Jan 31 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.31-1.fmi
- Added error checking to GeoJSON conversion

* Thu Jan 26 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.26-3.fmi
- Repackaged due to engine API changes

* Thu Jan 26 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.26-2.fmi
- Fixed CSS parser
- Fixed geojson generation for ice maps

* Thu Jan 26 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.26-1.fmi
- Repackaged due to contour-engine API changes

* Wed Jan 25 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.25-1.fmi
- Fixed styles option to work when the styles are an array of JSON includes

* Tue Jan 24 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.24-1.fmi
- Interim beta release, refactored code and silenced many CodeChecker warnings

* Thu Jan 19 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.19-1.fmi
- Repackaged due to ABI changes in grid libraries

* Mon Jan 16 2023 Mika Heiskanen <mika.heiskanen@fmi.fi> - 23.1.16-1.fmi
- Enabled possibility to fix speed and direction for ArrowLayer for querydata input

* Thu Jan 12 2023 Anssi Reponen <anssi.reponen@fmi.fi> - 23.1.12-1.fmi
- Enable alternative styles for all relevant layer types (BRAINSTORM-2507)

* Thu Dec 22 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.22-1.fmi
- Fixed several issues found by CodeChecker

* Wed Dec 21 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.21-1.fmi
- Repackaged since GIS library ABI changed

* Tue Dec 13 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.12.13-1.fmi
- Fixed GetCapabilities Interval Dimension bug (BRAINSTORM-2499)
- Enable configuring the missing symbol for observation layers, zero disables the symbol

* Mon Dec 12 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.12-1.fmi
- Repackaged due to ABI changes

* Fri Dec  2 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.12.2-3.fmi
- Check HTTP request method checking and support OPTIONS method

* Fri Dec  2 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.2-2.fmi
- Fixed all code to use the time_offset setting properly

* Thu Dec  1 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.12.1-1.fmi
- Added support for styling isolabels

* Tue Nov 29 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.29-1.fmi
- Repackaged since hash_value method for Q objects changed

* Tue Nov 22 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.22-1.fmi
- Added support for logarithmic interpolation

* Fri Nov 18 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.18-1.fmi
- Do not use exceptions for normal control flow in the WMS GetCapabilities update loop

* Tue Nov  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.8-3.fmi
- Repackaged due to base library ABI changes

* Tue Nov  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.8-2.fmi
- Disable logging stack traces if the requested WMS version is not supported

* Tue Nov  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.8-1.fmi
- Disable logging stack traces for common user input errors

* Wed Nov  2 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.11.2-1.fmi
- Fixed FrameLayer spatial reference transformation bug (BRAINSTORM-2439)

* Tue Nov  1 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.11.1-1.fmi
- Allow using WMS 1.1 SRS option if CRS option is not given

* Mon Oct 31 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.31-1.fmi
- Add apikey to error messages

* Fri Oct 21 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.21-1.fmi
- Fixes to icemap layer rendering

* Thu Oct 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.20-2.fmi
- Improved error message for JSON configuration errors

* Thu Oct 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.20-1.fmi
- Repackaged due to base library ABI changes

* Tue Oct 11 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.11-1.fmi
- Fixed observation layers to fetch station registry coordinates instead of geonames coordinates

* Mon Oct 10 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.10-2.fmi
- Improved error handling

* Mon Oct 10 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.10-1.fmi
- Repackaged due to base library ABI changes

* Wed Oct  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.5-1.fmi
- Do not use boost::noncopyable

* Mon Oct  3 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.10.3-1.fmi
- Added rounding modes for labels

* Thu Sep 29 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.29-1.fmi
- Fixes to missing value contouring

* Wed Sep 21 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.21-1.fmi
- Added TopoJSON support with temporary mime type application/topo+json

* Mon Sep 12 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.12-1.fmi
- Silenced several compiler warnings

* Fri Sep  9 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.9-1.fmi
- Report cache statistic units as bytes when necessary

* Mon Sep  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.5-1.fmi
- Fixed an incorrect test on shape emptyness detected by RHEL9 compiler

* Fri Sep  2 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.2-1.fmi
- Silenced several compiler warnings

* Thu Sep  1 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.9.1-1.fmi
- Added webp support

* Thu Aug 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.25-2.fmi
- Use a generic configuration exception handler

* Thu Aug 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.25-1.fmi
- GeoJSON precision is now configurable
- Added precision querystring option

* Wed Aug 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.24-1.fmi
- Support geojson and kml in WMS queries

* Fri Aug 19 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.19-1.fmi
- Repackaged to make sure smartmet-fonts will be installed

* Wed Aug 17 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.17-1.fmi
- Added support for AUTO2 spatial references

* Mon Aug  8 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.8.8-1.fmi
- Fixed WaWa to WW conversion for PresentWeatherObservationLayer

* Thu Jul 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.28-1.fmi
- Fixed ETag generation for layers using multifiles

* Wed Jul 27 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.27-2.fmi
- Added CSS cache statistics to admin report

* Wed Jul 27 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.7.27-1.fmi
- Repackaged since macgyver CacheStats API changed

* Wed Jun 29 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.29-1.fmi
- Repackage after merging hotfix

* Wed Jun 22 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.22-1.fmi
- Check that the WMS layers option is non empty

* Tue Jun 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.6.21-1.fmi
- Add support for RHEL9, upgrade libpqxx to 7.7.0 (rhel8+) and fmt to 8.1.1

* Tue Jun  7 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.6-1.fmi
- Fixed heatmap calculations to use std::abs to enforce positive sizes

* Mon Jun  6 2022 Andris Pavenis <andris.pavenis@fmi.fi> 22.6.6-1.fmi
- Repackage due to smartmet-engine-observation ABI changes

* Wed Jun  1 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.6.1-1.fmi
- Improved ETag calculation support

* Tue May 31 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.5.31-1.fmi
- Repackage due to smartmet-engine-querydata and smartmet-engine-observation ABI changes

* Tue May 24 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.5.24-2.fmi
- Fixed bug in default interval dimension (BRAINSTORM-2299)

* Tue May 24 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.24-1.fmi
- Repackaged due to NFmiArea ABI changes

* Fri May 20 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.20-1.fmi
- Repackaged due to ABi changes to newbase LatLon methods

* Thu May 12 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.5.12-1.fmi
- Added StyleSheet cache (BRAINSTORM-2317)

* Mon May 9 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.5.9-1.fmi
- Interval dimension added (BRAINSTORM-2299)
- Fixed bug in custom legends with alternative styles (BRAINSTORM-2314)
- Added missing error message when parsing alternative styles (BRAINSTORM-2316)
- GetCapabilities namespace must allow layer name in namespace-string (BRAINSTORM-2313)

* Thu May  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.5-1.fmi
- Repackaged since Trax ABI switch from using doubles to floats

* Wed May  4 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.5.4-1.fmi
- Use Trax for contouring

* Thu Apr 28 2022 Andris Pavenis <andris.pavenis@fmi.fi> 22.4.28-1.fmi
- Repackage due to SmartMet::Spine::Reactor ABI changes

* Thu Apr 14 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.4.14-1.fmi
- Fixed tag-layer (mask) bug in alternative styles (BRAINSTORM-2301)

* Tue Apr 5 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.4.5-2.fmi
- Fixed capabilitiesUpdateLoop-function bug (BRAINSTORM-2296)

* Tue Apr  5 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.4.5-1.fmi
- Repackaged

* Thu Mar 31 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.3.31-1.fmi
- Fix uninitialized variable

* Mon Mar 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.28-1.fmi
- Repackaged due to ABI changes in grid-content library

* Thu Mar 24 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.3.24-1.fmi
- Fix broken WMSConfig::shutdown()

* Mon Mar 21 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.21-3.fmi
- Disable stack traces for trivial user errors such as missing obligatory query parameters

* Mon Mar 21 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.3.21-2.fmi
- Possible to have custom legends for alternative styles (BRAINSTORM-2275)
- Fix default language bug of legends, config file->product file->url parameter (BRAINSTORM-2266)

* Mon Mar 21 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.3.21-1.fmi
- Update due to changes in smartmet-library-spine and smartnet-library-timeseries

* Thu Mar 10 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.10-1.fmi
- Repackaged due to base library ABI changes

* Tue Mar 8 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.3.8-1.fmi
- Started using timeseries-library (BRAINSTORM-2259)

* Mon Mar  7 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.3.7-1.fmi
- Repackaged due to base library API changes

* Tue Mar 1 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.3.1-1.fmi
- Fixed handling of symbol groups in legends (BRAINSTORM-2266)

* Mon Feb 28 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.28-1.fmi
- Repackaged due to base library/engine ABI changes

* Wed Feb 16 2022 Anssi Reponen <anssi.reponen@fmi.fi> - 22.2.16-1.fmi
- Support for present weather observations (BRAINSTORM-2231)
- Check for empty vector when processing result set  (BRAINSTORM-2264)

* Wed Feb  9 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.9-1.fmi
- Repackaged due to ABI changes in grid libraries

* Mon Feb  7 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.2.7-1.fmi
- Added "multiple" setting for labels

* Tue Jan 25 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.1.25-1.fmi
- Minor fix to unfinished generation handling

* Mon Jan 24 2022 Andris Pavēnis <andris.pavenis@fmi.fi> 22.1.24-1.fmi
- Rebuild due to package upgrade from PGDG (gdal 3.4 etc)

* Tue Jan 18 2022 Mika Heiskanen <mika.heiskanen@fmi.fi> - 22.1.18-1.fmi
- Added support for type=cnf with stage=N for different phases of configuration file preprocessing

* Thu Dec 16 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.12.16-1.fmi
- Fixed style selection so that any elment, except qid, can be redefined in styles-section

* Wed Dec 15 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.12.15-1.fmi
- Fixed NumberLayer style selection bug (BRAINSTORM-2220)

* Wed Dec  8 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.12.8-1.fmi
- Added unit conversion support for SymbolLayer

* Tue Dec  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.12.7-1.fmi
- Update to postgresql 13 and gdal 3.3

* Mon Nov 15 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.11.15-2.fmi
- Repackaged due to ABI changes in base grid libraries

* Mon Nov 15 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.15-1.fmi
- Map-layer must not show time-dimension in GetCapabilities response (BRAINSTORM-2197)

* Tue Nov 2 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.11.3-1.fmi
- Set valid value in GetCapabilities Last-Modified field (BRAINSTORM-2174)
- Fixed search order of file-path starting with '/' (BRAINSTORM-2193)

* Fri Oct 29 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.10.29-1.fmi
- Repackaged due to ABI changes in base grid libraries

* Wed Oct 27 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.27-1.fmi
- GetCapabilities expiration time may be defined in configuration file, default value 60s (BRAINSTORM-2172)

* Mon Oct 25 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.25-1.fmi
- Minimum distance and priority for Symbol-,Arrow-,NumberLayer (BRAINSTORM-2182)

* Tue Oct 19 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.19-1.fmi
- Use resources directory for common resources (BRAINSTORM-2164)

* Wed Oct 13 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.13-1.fmi
- Fixed missing keyword in GetCapabilities response when layout is not flat (BRAINSTORM-2165)

* Mon Oct 11 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.10.11-1.fmi
- Simplified grid storage structures
- Support for finnish road observations (BRAINSTORM-2155)

* Mon Oct  4 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.10.4-1.fmi
- Repackaged due to grid-files ABI changes

* Thu Sep 23 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.9.23-1.fmi
- Repackage to prepare for moving libconfig to different directory

* Wed Sep 22 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.9.22-1.fmi
- Must be possible for IsolineLayer to define isolines by defining startvalue, endvalue, interval (BRAINSTORM-2157)

* Wed Sep 15 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.9.15-1.fmi
- Repackaged due to NetCDF related ABI changes in base libraries

* Tue Sep  7 2021 Andris Pavēnis <andris.pavenis@fmi.fi> - 21.9.7-1.fmi
- Repackaged due to dependency changes (libconfig -> libconfig17)

* Wed Sep  1 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.9.1-1.fmi
- Fixed image caching to work again

* Mon Aug 30 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.8.30-1.fmi
- Cache counters added (BRAINSTORM-1005)

* Sat Aug 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.21-1.fmi
- Repackaged due to LocalTimePool ABI changes

* Thu Aug 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.19-1.fmi
- Start using local time pool to avoid unnecessary allocations of local_date_time objects (BRAINSTORM-2122)

* Tue Aug 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.17-1.fmi
- Use the new shutdown API

* Tue Aug  3 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.3-1.fmi
- Use boost::atomic_shared_ptr instead of atomic_store/load

* Mon Aug  2 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.8.2-1.fmi
- Repackaged since GeoEngine ABI changed by switching to boost::atomic_shared_ptr

* Tue Jul 27 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.7.27-1.fmi
- Repackaged due to GIS-library changes

* Mon Jul  5 2021 Andris Pavēnis <andris.pavenis@fmi.fi> 21.7.5-1.fmi
- Rebuild after moving DataFilter from obsengine to spine

* Tue Jun 29 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.29-2.fmi
- Repackaged since Observation::Engine::Settings ABI changed

* Thu Jun 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.17-2.fmi
- Fixed GetCapabilities not to return a layer which contains no valid times in the given starttime/endtime interval

* Thu Jun 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.17-1.fmi
- Fixed endtime handling in GetCapabilities for data with fixed timesteps

* Mon Jun 14 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.14-2.fmi
- Fixed GetCapabilities LegendURL to have width and height attributes

* Mon Jun 14 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.6.14-1.fmi
- Fixed LegendGraphic related bugs (BRAINSTORM-2083)
- Added width,height parameters to GetLegendGraphic URL

* Tue Jun  8 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.8-2.fmi
- Repackaged due to memory saving ABI changes in base libraries

* Tue Jun 8 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.6.8-1.fmi
- List of intervals in GetCapabilities reponse is made optional (BRAINSTORM-2090)

* Thu Jun  3 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.3-1.fmi
- Improved mindistance handling in graticulefill layouts

* Wed Jun 2 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.6.2-1.fmi
- Added support for list of multiple intervals (BRAINSTORM-2079)

* Tue Jun  1 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.6.1-1.fmi
- Repackaged due to grid library ABI changes

* Mon May 31 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.5.31-1.fmi
- Added layout configuration parameter (BRAINSTORM-2076)

* Fri May 28 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.28-2.fmi
- Fixed WMS parameter name to be dim_reference_time instead of plain reference_time in query strings

* Tue May 25 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.25-1.fmi
- Repackaged due to grid-files ABI changes

* Mon May 24 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.24-1.fmi
- Use Roboto as default font for some layers

* Fri May 21 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.21-1.fmi
- Repackaged due to a QEngine API change

* Wed May 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.19-1.fmi
- Use FMI hash functions, boost::hash_combine produces too many collisions

* Mon May 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.17-2.fmi
- Fixed ArrowLayer exception handling not to dereference uninitialized optional values

* Mon May 17 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.17-1.fmi
- Fixed SymbolLayer to handle lightning coordinates correctly for non geographic CRS queries

* Wed May 12 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.5.12-1.fmi
- Implemented styles-option for symbol,arrow,number,isolabel layers (BRAINSTORM-2057)

* Thu May  6 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.5.6-1.fmi
- Repackaged due to ABI changes in NFmiAzimuthalArea

* Wed May 5 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.5.5-1.fmi
- Fixed manual setting of legend width (BRAINSTORM-1522)
- Now it is also possible to configure fixed width per language

* Tue Apr 27 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.4.27-1.fmi
- Set unique name for reference time layers (BRAINSTORM-1836)

* Thu Apr 15 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.4.15-1.fmi
- Fixed bounding box generation for latlon projections

* Tue Apr 13 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.4.13-1.fmi
- Fixed inheritance of "level" setting in the Properties class

* Thu Apr  1 2021 Pertti Kinnia <pertti.kinnia@fmi.fi> - 21.4.1-1.fmi
- Repackaged due to grid-files API changes

* Mon Mar 22 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.22-1.fmi
- Handle invalid coordinates for ice map components, times, tags etc

* Fri Mar 12 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.12-1.fmi
- Print apikey on for errorneous requests

* Fri Mar  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.5-1.fmi
- Small fix to layer handling

* Thu Mar  4 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.4-1.fmi
- Merged grid and querydata code and tests

* Wed Mar  3 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.3-2.fmi
- Grid-engine may now be disabled

* Wed Mar  3 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.3.3-1.fmi
- Clip IceMapLayer polygons to the image size to prevent problems in high zoom levels

* Fri Feb 26 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.26-1.fmi
- Fmi::CoordinateTransformation API changed

* Thu Feb 25 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.25-1.fmi
- Added lon_wrap support for data using 0...360 longitudes

* Wed Feb 24 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.24-1.fmi
- Fixed to handle missing size settings

* Sat Feb 20 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.20-1.fmi
- Fixed undefined behaviour in FrameLayer

* Fri Feb 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.19-1.fmi
- Enabled GridLayer

* Thu Feb 18 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.18-1.fmi
- Repackaged due to newbase ABI changes

* Tue Feb 16 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.2.16-1.fmi
- Fixed GRIB mode GetCapabilities response to handle all analysis times

* Thu Feb 11 2021 Anssi Reponen <anssi.reponen@fmi.fi> - 21.2.11-1.fmi
- Support for layer hierarchy, reference_time, elevation in 
GetCapabilities response (BRAINSTORM-1836,BRAINSTORM-1413,BRAINSTORM-1414),
to see this feature 'newfeature=1' or 'newfearure=2' option must be used in GetCapabilities request
- Fixed attribute name nearestValues to nearestValue in GetCapbilities response

* Wed Jan 27 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.27-1.fmi
- Repackaged due to base library ABI changes

* Tue Jan 19 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.19-1.fmi
- Repackaged due to base library ABI changes

* Thu Jan 14 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.14-1.fmi
- Repackaged smartmet to resolve debuginfo issues

* Tue Jan 12 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.12-1.fmi
- Upgraded jsoncpp

* Mon Jan 11 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.11-1.fmi
- Repackaged due to grid-files API changes

* Tue Jan  5 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.5-1.fmi
- GDAL32 and FMT 7.1.3 upgrades

* Mon Jan  4 2021 Mika Heiskanen <mika.heiskanen@fmi.fi> - 21.1.4-1.fmi
- Upgrade to GDAL 3.2

* Tue Dec 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.15-1.fmi
- Upgrade to pgdg12

* Thu Dec  3 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.3-1.fmi
- Repackaged due to library ABI changes

* Tue Dec  1 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.12.1-1.fmi
- Fixed observation layers not to abort if there are no nearby layers

* Mon Nov 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.11.30-1.fmi
- Repackaged due to grid-content library API changes

* Tue Nov 24 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.11.24-1.fmi
- Repackaged due to library ABI changes

* Fri Oct 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.30-1.fmi
- Upgrade to FMT 7.1

* Wed Oct 28 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.10.28-1.fmi
- Rebuild due to fmt upgrade

* Fri Oct 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.23-1.fmi
- Use new TemplateFormatter API

* Thu Oct 22 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.22-1.fmi
- Updated libconfig requirement

* Thu Oct 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.15-1.fmi
- Fixed LocationLayer coordinate clipping to work for metric spatial references

* Fri Oct  9 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.9-1.fmi
- Use a cache for reading all JSON files

* Tue Oct  6 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.6-1.fmi
- Enable sensible relative libconfig include paths

* Thu Oct  1 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.10.1-1.fmi
- Repackaged due to library ABI changes

* Wed Sep 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.23-1.fmi
- Use Fmi::Exception instead of Spine::Exception

* Fri Sep 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.18-1.fmi
- Repackaged due to library ABI changes

* Tue Sep 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.15-1.fmi
- Repackaged due to library ABI changes

* Mon Sep 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.14-1.fmi
- Repackaged due to library ABI changes

* Mon Sep  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.7-1.fmi
- Repackaged due to library ABI changes

* Wed Sep  2 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.9.1-1.fmi
- Repackaged since Observation::Settings size changed

* Mon Aug 31 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.31-1.fmi
- Repackaged due to library ABI changes

* Fri Aug 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.21-1.fmi
- Upgrade to fmt 6.2

* Tue Aug 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.18-1.fmi
- Repackaged due to grid library ABI changes

* Fri Aug 14 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.14-1.fmi
- Repackaged due to grid library ABI changes

* Tue Aug 11 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.8.11-1.fmi
- Speed improvements

* Fri Jul 24 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.24-1.fmi
- System wide known spatial references can now be disabled by default
- Layers can now disable or enable spatial references listed in the main configuration file

* Thu Jul 23 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.23-1.fmi
- Precalculate WMS layer projected bboxes for GetCapabilities speedup

* Wed Jul 22 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.22-2.fmi
- Avoid unnecessary OGRCoordinateTransformation creation for speed

* Wed Jul 22 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.22-1.fmi
- Use GIS-engine for creating coordinate transformations faster

* Tue Jul 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.21-2.fmi
- Use GIS-engine for creating spatial references faster

* Tue Jul 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.21-1.fmi
- GetCapabilities speed improvements
- Repackaged due to ObsEngine ABI changes for station searches

* Thu Jul  2 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.2-2.fmi
- Fixed PNG settings quality and errorfactor to be of type double instead of int

* Thu Jul  2 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.7.2-1.fmi
- Fixed GetCapabilities to list times with "," separator without a space after the comma

* Mon Jun 29 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.29-1.fmi
- Do not draw SymbolLayer symbols if the data value is missing

* Mon Jun 22 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.22-1.fmi
- Removed debugging code

* Mon Jun 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.15-1.fmi
- Renamed .so to enable simultaneous installation of wms and gribwms

* Wed Jun 10 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.6.10-1.fmi
- Rebuilt due to obsengine API change

* Mon Jun  8 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.6.8-1.fmi
- Fixed ArrowLayer not to fetch clipping bbox observations when wanted stations are listed
- Repackaged due to base library changes

* Fri May 15 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.5.15-1.fmi
- Repackaged due to base library changes

* Tue May 12 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.5.12-1.fmi
- Observation-engine API changed (BRAINSTORM-1678)

* Thu Apr 30 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.30-1.fmi
- Repackaged due to base library API changes

* Sat Apr 18 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.18-1.fmi
- Upgraded to Boost 1.69

* Fri Apr  3 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.4.3-1.fmi
- Repackaged due to library API changes
- Avoid recursion in WMS exception handling

* Fri Mar 20 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.3.20-1.fmi
- Do not return 200 OK except for error responses in image format

* Thu Mar  5 2020 Andris Pavenis <andris.pavenis@fmi.fi> - 20.3.5-1.fmi
- Use SmartMet::Spine::makeParameter

* Wed Feb 26 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.26-1.fmi
- Fixed IsolabelLayer type to be a querydata-layer for WMS requests

* Tue Feb 25 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.25-1.fmi
- Repackaged due to base library API changes
- Fixed level-setting to work for isoline/isoband layers

* Fri Feb 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.21-1.fmi
- Upgrade to GDAL 3.0

* Thu Feb 20 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.20-1.fmi
- GRIB GetCapabilities response now depends on the used parameters
- Added print_params query option

* Wed Feb 19 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.19-1.fmi
- Improved GetCapabilities responses in GRIB mode

* Sun Feb  9 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.9-1.fmi
- Repackaged due to delfoi/obsengine changes

* Fri Feb  7 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.7-1.fmi
- Repackaged since Spine::Station API changed due to POD member initializations

* Tue Feb  4 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.2.4-1.fmi
- Added IsolabelLayer

* Wed Jan 29 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.29-1.fmi
- Repackaged due to base library API changes

* Tue Jan 21 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.21-1.fmi
- Repackaged due to grid-content and grid-engine API changes

* Thu Jan 16 2020 Mika Heiskanen <mika.heiskanen@fmi.fi> - 20.1.16-1.fmi
- Repackaged due to base library API changes

* Tue Jan 14 2020 Anssi Reponen <anssi.reponen@fmi.fi> - 20.01.14-1.fmi
- Support added for starttime/endtime parameters in GetCapabilities-request 
in order to restrict valid time period in output document (BRAINSTORM-1744)

* Fri Dec 27 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.27-1.fmi
- Prevent crashes if sampling is requested for observations in isoband/isoline layers

* Fri Dec 13 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.13-1.fmi
- Upgrade to GDAL 3.0

* Wed Dec 11 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.11-1.fmi
- Fixed handling of "{name}.raw" parameters

* Wed Dec  4 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.12.4-1.fmi
- Added more error checks

* Fri Nov 22 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.22-1.fmi
- Repackaged due to API changes in grid-content library

* Wed Nov 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.20-1.fmi
- Repackaged due to newbase/spine ABI changes

* Thu Nov  7 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.11.7-1.fmi
- Minor fixes

* Thu Oct 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.31-1.fmi
- Rebuilt due to newbase API/ABI changes

* Wed Oct 30 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.30-1.fmi
- Full recompile of GRIB server components

* Wed Oct 23 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.10.23-1.fmi
- If multiple layers is given in WMS LAYERS-option they are combined together (BRAINSTORM-1059)

* Tue Oct  8 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.8-1.fmi
- Fixed arrow layer not to correct U/V components anymore, qengine handles it

* Tue Oct  1 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.10.1-1.fmi
- Repackaged due to SmartMet library ABI changes

* Thu Sep 26 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.26-1.fmi
- Repackaged due to ABI changes

* Tue Sep 17 2019  Anssi Reponen <anssi.reponen@fmi.fi> - 19.9.17-1.fmi
- NetAtmo and RoadCloud test cases updated because mid-parameter is no more supported,
but parameter name must be used instead (Related to BRAINSTORM-1673)

* Thu Sep 12 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.9.12-1.fmi
- Fixed WMSLayer destructor to be virtual to avoid memory problems

* Wed Aug 28 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.8.28-1.fmi
- Repackaged since Spine::Location ABI changed

* Fri Jun 14 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.14-1.fmi
- Fixed intersection code to check for empty output

* Wed Jun  5 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.5-1.fmi
- Do not print a stack trace for invalid time requests to reduce log sizes

* Tue Jun  4 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.6.4-1.fmi
- Added minvalues setting to symbol, number and arrow layers to require a sufficient number of valid values

* Wed May 29 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.5.29-1.fmi
- Changed to handle altered MetaData class from Gis-engine

* Mon May 6 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.5.6-1.fmi
- Added a proper error message for missing legend template files (BRAINSTORM-1526)

* Tue Apr 30 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.30-2.fmi
- Remove display=none views and layers from SVG output if optimizesize=1 is set

* Tue Apr 30 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.30-1.fmi
- Option quiet=1|true disables stack trace printing
- Modified illegal input tests to use quiet=1 to keep test output simple

* Tue Apr 23 2019 Anssi Reponen <anssi.reponen@fmi.fi> - 19.4.23-1.fmi
- Language support for automatically generated legends (BRAINSTORM-1523)
- Support for mobile and external observations (BRAINSTORM-1420)

* Fri Apr 12 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.4.12-1.fmi
- Fixed MapLayer hash calculation to include querydata hash value if styles are used

* Fri Mar 29 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.3.29-1.fmi
- Fixed WMS exceptions not to return 304/200 responses which might be cached

* Thu Feb 21 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.22-1.fmi
- Fixed ArrowLayer southflop setting to work
- Added ArrowLayer flip and northflop settings
- Added unit_conversion settings

* Thu Feb 21 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.21-1.fmi
- Added minarea setting for IsobandLayer and IsolineLayer

* Wed Feb 20 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.20-1.fmi
- Added direct translation support for isoband labels

* Tue Feb 19 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.19-1.fmi
- Always return ETag unless it is invalid

* Mon Feb 18 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.18-1.fmi
- Print URI and client IP on WMS exceptions to the console

* Fri Feb 15 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.15-1.fmi
- Print stack trace to console for WMS exceptions too

* Thu Feb 14 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.14-1.fmi
- Added client IP to exception reports

* Tue Feb 12 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.12-1.fmi
- Added extrapolation-setting for IsoLineLayer and IsoBandLayer

* Thu Feb  7 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.2.7-1.fmi
- Fixed etag-based caching

* Thu Jan 31 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.1.31-1.fmi
- Added possibility to style map layers based on forecast values

* Mon Jan 14 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.1.14-1.fmi
- Fixed ArrowLayer to handle missing values properly

* Thu Jan 10 2019 Mika Heiskanen <mika.heiskanen@fmi.fi> - 19.1.10-1.fmi
- Fixed GetLegendGraphic not to crash if the style option is not given

* Thu Dec 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.12.13-1.fmi
- Continue GetCapabilities generation even if bbox info is not available for some configured EPSG

* Fri Nov 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.23-1.fmi
- Fixed handling of observations

* Wed Nov 21 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.21-1.fmi
- Minor const correctness fix

* Fri Nov 16 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.16-1.fmi
- Fixed intersections to inherit the producer name instead of the data so that landscaped parameters can be handled too

* Mon Nov 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.12-2.fmi
- Added tz setting for all layers, which affects how "time" (not origintime) is parsed

* Mon Nov 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.12-1.fmi
- Refactored TemplateFactory into macgyver library

* Fri Nov  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.11.9-1.fmi
- GetCapabilities update loop will now warn only once per file to avoid flooding the logs

* Sat Sep 29 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.29-1.fmi
- Upgrade to latest fmt

* Thu Sep 27 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.9.27-1.fmi
- WIDTH- and HEIGHT-parameters, when present, determine the size of 
Exception-picture (when EXCEPTIONS=INIMAGE)

* Wed Sep 26 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.9.26-1.fmi
- Handling of WMS's optional EXCEPTIONS-parameter added (BRAINSTORM-1344)
- In addition to xml-, and json-formats, it is now possible to get all exceptions also in picture formats (svg,png,pdf)

* Thu Sep 20 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.9.20-1.fmi
- Added handling of new optional product attribute 'disable_wms_time_dimension'

* Tue Sep 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.11-1.fmi
- Fixed IceMapLayer hash_value calculations
- Silenced some CodeChecker warnings

* Mon Sep 10 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.10-1.fmi
- Enabled JSON output for GetCapabilities and exceptions

* Sun Sep  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.9.9-1.fmi
- Fixed derived class destructors to be virtual

* Thu Aug 30 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.30-1.fmi
- Silenced CodeChecker warnings

* Tue Aug 28 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.8.28-1.fmi
- Oracle parameter names in test/cnf/observation.conf file made uppercase (BRAINSTORM-1156)

* Wed Aug 22 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.8.22-2.fmi
- Fixed handling of symbols and symbol groups in GetLegendGraphic response (BRAINSTORM-1279)
- New symbols taken in use for precipitation form

* Thu Aug 16 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.8.16-1.fmi
- Improved iceegg creation algorithm (BRAINSTORM-1266)
- Fixed (potential) memory leak in FrameLayer

* Wed Aug 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.15-2.fmi
- Use only one TemplateFactory object, WMSGetCapabilities is now plain namespace instead of an object

* Wed Aug 15 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.15-1.fmi
- Refactored code

* Wed Aug  8 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.8-1.fmi
- Silenced several CodeChecker warnings
- Fixed CTTP2 template cache, it did not cache anything

* Fri Aug  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.3-1.fmi
- The SVG precision of isoband, map etc layers is now configurable

* Thu Aug  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.2-2.fmi
- Changed default resolution to 0.3 decimals (was 1)

* Thu Aug  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.2-1.fmi
- Fixed a potential crash if authentication engine is not available

* Wed Aug  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.8.1-1.fmi
- Use C++11 for-loops instead of BOOST_FOREACH

* Thu Jul 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.26-1.fmi
- Use Spine::JsonCache to avoid parsing WMS layer files repeatedly

* Wed Jul 18 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.18-1.fmi
- Fixed observation layers not to be cached

* Wed Jul  4 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.7.4-1.fmi
- Fixed a memory leak in WMS legend generation

* Wed Jun 13 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.6.13-1.fmi
- Support for WMS STYLES added

* Mon Jun 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.11-1.fmi
- Added a safety check against requests with sub minute intervals, which are not supported yet

* Wed Jun  6 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.6-2.fmi
- Added a safety check to layer namespace generation

* Wed Jun  6 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.6-1.fmi
- Reduced size of GetCapabilities by placing xlink schema in the document namespace

* Sun Jun  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.6.3-1.fmi
- Fixed GetCapabilities not to abort if POST online_resource is undefined and a protocol change is required

* Thu May 31 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.31-3.fmi
- Added origintime option

* Thu May 31 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.31-2.fmi
- Absolute CSS paths are now relative to WMS root directory

* Thu May 31 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.31-1.fmi
- Added setting wms.disable_updates to disable capability updates
- Added setting wms.update_interval with default value 5 seconds

* Mon May 28 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.28-1.fmi
- XMLESCAPE cannot handle undefined strings, added if's to the GetCapabilities template

* Mon May 14 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.14-2.fmi
- Added a default margin setting for WMS layers

* Mon May 14 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.14-1.fmi
- Added Layer margins to Positions to fix clipping of arrows, symbols and layers

* Fri May 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.11-1.fmi
- Isolines and isobands will now be assigned qid:s automatically if not set

* Wed May  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.9-1.fmi
- Fixed heatmaps to generate, the point querydata test added earlier broke them

* Thu May  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.3-2.fmi
- Error if isobands or isolines are attempted from point querydata
- Added X-WMS-Exception and X-WMS-Error headers to WMS error responses

* Thu May  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.3-1.fmi
- Fixed WMS reponse codes for errors not to be 200 OK to prevent caching

* Thu May  3 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.5.3-1.fmi
- Moved common legend template-directory to wms-root directory (from <customer>-directory)
- Fixed LegendGraphics bugs: BRAINSTORM-1129, BRAINSTORM-1134

* Wed May  2 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.5.2-1.fmi
- Added png settings for controlling color reduction

* Wed Apr 25 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.25-1.fmi
- Added possibility to pass CSS via query string

* Wed Apr 18 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.18-1.fmi
- Added XMLESCAPE calls for GetCapabilities titles, abstracts etc

* Fri Apr 13 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.13-1.fmi
- New query string overrides for filters, patterns, markers and gradients

* Wed Apr 11 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.11-1.fmi
- Added handling of WindUMS and WindVMS as meta parameters

* Tue Apr 10 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.10-1.fmi
- Check whether LAYERS is set before authenticating to prevent crashes

* Mon Apr  9 2018 Pertti Kinnia <pertti.kinnia@fmi.fi> - 18.4.9-1.fmi
- Fixed crash with heatmap when no flashes were found

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-2.fmi
- Upgrade to boost 1.66

* Sat Apr  7 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.7-1.fmi
- Added possibility to define symbols to be used via the query string

* Fri Apr  6 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.6-1.fmi
- Fixed wind direction handling in ArrowLayer to use output CRS instead of data CRS
- ArrowLayer now supports relative_uv setting if drawn from U/V components

* Tue Apr  3 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.4.3-1.fmi
- Added ArrowLayer setting minrotationspeed to disable rotation of variable wind symbols

* Wed Mar 21 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.21-1.fmi
- SmartMetCache ABI changed

* Tue Mar 20 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.20-1.fmi
- Full recompile of all server plugins

* Mon Mar 19 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.19-1.fmi
- Removed obsolete call to Observation::Engine::setGeonames

* Sat Mar 17 2018 Anssi Reponen <anssi.reponen@fmi.fi> - 18.3.17-1.fmi
- Fixed text layout inside iceegg
- Fixed LegendGraphics parsing when there is no reference to external json/css files in product file
- Added heatmap support for lightning data

* Sat Mar 10 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.10-1.fmi
- Added support for formatting times in TimeLayer by libfmt

* Fri Mar  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.9-1.fmi
- Use macgyver time to string conversions to avoid locale locks

* Thu Mar  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.1-3.fmi
- TimeLayer formatter can now be either boost or strftime

* Thu Mar  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.1-2.fmi
- Fixed station layout to work for forecasts too by setting feature_code to SYNOP

* Thu Mar  1 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.3.1-1.fmi
- GetLegend expiration time is now configurable

* Wed Feb 28 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.28-1.fmi
- Added station layout
- Modified Positions to return dx,dy adjustments to objects attached to individual coordinates
- Support station layout for numbers, symbols and arrows

* Tue Feb 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.27-2.fmi
- Individual latlon locations can now be shifted with dx/dy attributes

* Tue Feb 27 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.27-1.fmi
- Added support for meta parameters in isoband, isoline, symbol and number layers

* Mon Feb 26 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.26-1.fmi
- Fixed fill attribute IRIs to work

* Mon Feb 19 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.19-1.fmi
- shutdown is now much faster

* Mon Feb 12 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.12-1.fmi
- Restored validtime as the default value for TimeLayer timestamp

* Fri Feb  9 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.2.9-1.fmi
- Repackaged due to TimeZones API change

* Wed Jan 31 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.31-1.fmi
- Fixed wms.quiet option to work

* Tue Jan 23 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.23-1.fmi
- Added new time layer formatting options

* Thu Jan 18 2018 Mika Heiskanen <mika.heiskanen@fmi.fi> - 18.1.18-1.fmi
- Allow plain map layers in WMS requests

* Wed Dec 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.13-1.fmi
- Removed forgotten debugging code from NumberLayer

* Tue Dec 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.12-1.fmi
- Fixed code to allow both double and string fmisids

* Fri Dec  1 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.12.1-1.fmi
- Return updated expiration times for etag responses (BRAINSTORM-1000)

* Wed Nov 29 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.29-1.fmi
- Improved expiration time estimates for layers using querydata

* Mon Nov 27 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.11.27-1.fmi
- Ice egg layer functionality added
- Symbol layer for 'strips and patches' added into product files
- Typos corrected, legend layout fine-tuned

* Wed Nov 22 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.22-1.fmi
- Fixed GeoJSON winding rule to be CCW for shells and CW for holes

* Fri Nov 17 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.11.17-1.fmi
- New 'wms.get_legend_graphic.symbols_to_ignore' parameter added to configuration file: you can define 
list of symbols that are not shown in GetLegendGraphic-response (e.g. fmi_logo)
- New and modified product files, legend layers, patterns, symbols. Some product and layer files renamed

* Tue Nov 14 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.11.14-1.fmi
- BRAINSTORM-959: GetLegendGraphic doesnt use ctpp2-templates any more
- BRAINSTORM-980: WMS-plugin reports to standard output when it notices that product-file has been modified
- Product files, symbols, filters, patterns updated
- PostGIS metadata query interval can now be schema-specific: 'postgis-layer' entry in configuration file can be a list
- Handling of text-field added to IceMapLayer
- Added map number to icemap

* Thu Nov 9 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.11.9-1.fmi
- Fixed metadata update interval bug

* Wed Nov  1 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.11.1-1.fmi
- Rebuilt due to dependency updates

* Tue Oct 31 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.10.31-1.fmi
- icemap product files updated
- traffic restrictions table implemented
- BRAINSTORM-976: metadata query interval made configurable. Additionally 
if PostGISLayer is of type icemap, the existence of new icemaps can be checked 
from database before metadata is updated

* Mon Oct 23 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.23-1.fmi
- Allow placing more layer data into the defs-section to enable more sharing

* Thu Oct 19 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.19-1.fmi
- Added checks against duplicate qid-values.

* Tue Oct 17 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.10.16-2.fmi
- Allow application/pdf requests

* Mon Oct 16 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.10.16-1.fmi
- product files updated
- handling of sublayers of PostGISLayer corrected
- GetCapabilities geographic bounding box longitudes are now forced to range -180...180

* Thu Oct 12 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.10.12-1.fmi
- Icemap-related changes:
- changes are based on two Agile Jira-issues: VANADIS-376,COICESERV-36 
- new layer, FrameLayer, added for a frame (and scale) around the map
- WMS now supports application/pdf-documents
- longitude, latitude attribute added to TagLayer, TimeLayer to express location on map
- new icemap-products, patterns, symbols, filters added

* Wed Sep 27 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.27-2.fmi
- Fixed GetLegendGraphic to use unique IDs in case there are two or more isoband layers

* Wed Sep 27 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.27-1.fmi
- Changed variable name obsengine_disable to observation_disabled as in Timeseries-plugin

* Mon Sep 25 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.25-2.fmi
- Allow any time in WMS requests inside the range of times listed in GetCapabilities

* Mon Sep 25 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.9.25-1.fmi
- Encode lolimit and hilimit with null in GeoJSON, infinities are not valid in JSON
- Added configurability for GetLegendGraphic-response: Parameter name, unit and layout of output document can be configured (BRAINSTORM-922)

* Wed Sep 20 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.20-1.fmi
- Output lolimit and hilimit to geojson even if +-inf

* Thu Sep 14 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.9.14-1.fmi
- Added configurability for GetLegendGraphic-response:
Parameter name, unit and layout of output document can be configured (see. BRAINSTORM-922)

* Tue Sep 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.12-1.fmi
- Using new contouring API which does not care if crs=data is used.

* Fri Sep  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.8-1.fmi
- Allow projection settings to be missing in WMS product configuration files
- Improved query string handling via fixes to Spine

* Thu Sep  7 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.7-1.fmi
- Fixed polygon intersections to use the requested spatial references

* Mon Sep  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.9.4-1.fmi
- Using request's host and apikey when generating online resource urls for GetCapabilities response

* Thu Aug 31 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.31-1.fmi
- Fixed handling of invalid CRS requests to return the correct WMS exception report

* Tue Aug 29 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.8.29-1.fmi
- Size of returned map for GetLegendGraphic request is calculated automatically (fixed size was used before).
  When WIDTH, HEIGHT request parameters are present they override automatically calculated values. 

* Mon Aug 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.28-1.fmi
- Upgrade to boost 1.65

* Mon Aug 21 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.21-1.fmi
- Fixed observation arrows and symbols to work for non geographic references

* Fri Aug 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.18-1.fmi
- Plain ETag response code to frontend is now 204 no content
- Supported spatial references are now listed in the configuration file
- Added CRS:84, CRS:27, CRS:83 support
- Added local references CRS::SmartMetScandinavia and CRS:SmartMetEurope

* Mon Aug  7 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.7-1.fmi
- WMS exceptions are now more detailed if the debug option is set
- Rewrote WMS error messages to be more detailed

* Thu Aug  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.3-2.fmi
- urlencode layer names. In particular ':' should be encoded.

* Thu Aug  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.3-1.fmi
- Do not cache CTPP::CDT objects at all, it is not thread safe

* Wed Aug  2 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.1-2.fmi
- Fixed hostname generation to include the http protocol

* Tue Aug  1 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.8.1-1.fmi
- CTPP2 downgrade forced recompile
- Changed to using a shared pointer instead of optional for CTPP::CDT objects

* Mon Jul 31 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.7.31-1.fmi
- Rewrote GetCapabilities template and respective code to cover the full XML schema with matching names
- Added namespace attribute to WMS GetCapabilities queries

* Mon Jul 17 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.7.17-2.fmi
- Fixed error messages for partial projection descriptions

* Mon Jul 17 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.7.17-1.fmi
- Added electronic_mail_address GetCapabilities configuration variable
- Fixed GetCapabilities Style section to validate - name and title are obligatory

* Tue Jun 20 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.6.20-1.fmi
- Use layer customer instead of default customer in wms-query

* Mon Jun 19 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.6.19-2.fmi
- Fixed GetCapabilities-response

* Mon Jun 19 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.6.19-1.fmi
- Handling of GetLegendGraphic message added (BRAINSTORM-895)
- testcases updated

* Wed May 31 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.31-1.fmi
- JSON references and includes can now be replaced from query strings

* Mon May  29 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.5.29-1.fmi
- LegendURL support added to GetCapabilities response (BRAINSTORM-895)
- GetCapabilities response message made more configurable (BRAINSTORM-870)

* Fri May  5 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.5.5-1.fmi
- http/https scheme selection based on X-Forwarded-Proto header; STU-5084
- GetMap and GetCapabilities keywords are now case insensitive

* Fri Apr 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.28-1.fmi
- Avoid reprojections once sampling has been done
- Prevent crash if TimeLayer origintime is desired when no querydata has been specified

* Mon Apr 24 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.24-1.fmi
- Fixed GeoJSON templates to omit quotes around the coordinates

* Thu Apr 20 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.4.20-1.fmi
- WMS acceps now observation-queries (BRAINSTORM-865)
- Some fields in GetCapabilities-response is made optional (BRAINSTORM-869)

* Wed Apr 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.12-1.fmi
- Bug fix: if crs=data, use geographic bounding box for latlon projections

* Tue Apr 11 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.4.11-1.fmi
- Variable name in template file corrected

* Mon Apr 10 2017 Anssi Reponen <anssi.reponen@fmi.fi> - 17.4.10-3.fmi
- Hardcoded values in GetCapabilities-response template file removed and moved into configuration file

* Mon Apr 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.10-2.fmi
- Added rawkml and rawgeojson templates which omit presentation attributes

* Mon Apr 10 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.10-1.fmi
- Added detection of global data that needs wraparound when contouring

* Sat Apr  8 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.8-1.fmi
- Simplified error handling

* Fri Apr  7 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.7-1.fmi
- Bug fix to symbol layer, required obs. parameter names was counted incorrectly

* Mon Apr  3 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.4.3-1.fmi
- Added CORS headers to all responses
- Modifications due to observation engine API changes:
  redundant parameter in values-function removed,
  redundant Engine's Interface base class removed

* Wed Mar 29 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.29-3.fmi
- Added a lines-setting to PostGISLayer to enable polyline-type clipping

* Wed Mar 29 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.29-2.fmi
- scale attribute is now appended into transform attributes in number, symbol and arrow layers

* Wed Mar 29 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.29-1.fmi
- KML and GeoJSON output now includes presentation attributes

* Thu Mar 16 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.16-1.fmi
- Added WKTLayer

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-3.fmi
- Fixed ArrowLayer not to require a speed parameter

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-2.fmi
- Recompiled since Spine::Exception changed

* Wed Mar 15 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.15-1.fmi
- Crash fix: added anonymous namespaces to protect similarly named structs

* Tue Mar 14 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.14-1.fmi
- Switched to using macgyver StringConversion tools

* Mon Mar 13 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.13-1.fmi
- Added GeoJSON support
- Added KML support

* Thu Mar  9 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.3.9-1.fmi
- Added support for observations
- Added support for margins
- Added support for adding a clipping path to layers
- Fixed isoline clipping by shapes
- Added support for filtering observations by other observations

* Sun Feb 12 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.12-1.fmi
- Added obsengine_disabled option with default = false
- Added possibility to package the plugin without the observation engine

* Sat Feb 11 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.11-1.fmi
- Repackaged due to newbase API change

* Sat Feb  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.4-1.fmi
- root and wms.root are now required settings

* Wed Feb  1 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.2.1-1.fmi
- Updated apikey handling

* Sat Jan 28 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.28-1.fmi
- Switched to using the new type of NFmiQueryData lonlon-cache

* Wed Jan 18 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.18-1.fmi
- Upgrade from cppformat-library to fmt

* Thu Jan  5 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.5-1.fmi
- Fixed WMS plugin to report that STYLES parameter is missing, not STYLE

* Wed Jan  4 2017 Mika Heiskanen <mika.heiskanen@fmi.fi> - 17.1.4-1.fmi
- Changed to use renamed SmartMet base libraries

* Wed Nov 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.30-1.fmi
- Using test databases in test configuration
- No installation for configuration

* Tue Nov 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.29-1.fmi
- Recompiled due to spine API changes

* Tue Nov  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.11.1-1.fmi
- Namespace changed
- Authentication can now be disabled by setting authenticate=false

* Wed Sep 28 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.28-1.fmi
- Disabled the Inspire extension until the XML validates
- Initialize WMSLayer coordinates properly in the constructor

* Tue Sep 27 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.27-1.fmi
- Fixed handling of supported WMS versions

* Fri Sep 23 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.23-1.fmi
- Setting a zero or negative resolution now disables sampling, this enables disabling sampling via a query string

* Wed Sep 21 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.21-1.fmi
- IceMapLayer refactored: IceMapLayerHandler class removed and functionality moved to IceMapLayer class. Layer properties modified/unified and configuration files updated accordingly (JSON Reference document updated as well).

* Tue Sep 13 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.13-1.fmi
- Code modified bacause of Contour-engine API changes: vector of isolines/isobands queried at once instead of only one isoline/isoband

* Tue Sep 13 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.13-1.fmi
- Added parameter value checkings for "type","width" and "height".

* Tue Sep  6 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.9.6-1.fmi
- New exception handler
- dali configuration file in test directory corrected dali/test/dali.conf
- 'true' replaced with 1 when assigning into CTPP::CDT map
- Support for Inspire extended capabilities added (BRAINSTORM-700)
- Refactoring: wms-related stuff in Config.cpp moved to WMSConfig.cpp, naming and structure of wms-parameters in configuration file changed

* Tue Aug 30 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.8.30-1.fmi
- Base class API change
- Use response code 400 instead of 503

* Mon Aug 15 2016 Markku Koskela <markku.koskela@fmi.fi> - 16.8.15-1.fmi
- The init(),shutdown() and requestHandler() methods are now protected methods
- The requestHandler() method is called from the callRequestHandler() method

* Wed Jun 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.29-1.fmi
- QEngine API changed

* Tue Jun 14 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.14-1.fmi
- Full recompile

* Thu Jun  2 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.2-1.fmi
- Full recompile

* Wed Jun  1 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.6.1-1.fmi
- Added graceful shutdown

* Tue May 17 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.5.17-1.fmi
- Fixed unhelpful accessors
- No longer doing double GetCapabilities - update

* Mon May 16 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.16-1.fmi
- Replaced TimeZoneFactory with TimeZones

* Wed May 11 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.5.11-1.fmi
- Avoid string streams for speed
- TagLayer cdata now supports translations

* Wed Apr 20 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.4.20-1.fmi
- Added "quiet" konfiguration option to be able to silence warnings during tests. Default = false.
- Built against new Contour-Engine
- AuthEngine support

* Tue Apr 19 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.4.19-1.fmi
- Allow a missing time setting if time is not needed in the result

* Mon Apr 18 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.4.18-3.fmi
- Added catch-guard to GIS engine metadata retrieval

* Mon Apr 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.4.18-2.fmi
- Upgraded to the latest jsoncpp with UInt64 changes

* Mon Apr 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.4.18-1.fmi
- Upgraded to cppformat 2.0

* Thu Mar 31 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.31-1.fmi
- Fixed segfault issue in shape handling

* Mon Mar  7 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.3.7-2.fmi
- Permit negative dx,dy offsets for most layout algorithms

* Mon Mar  7 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.3.7-1.fmi
- Added dx,dy offsetting support for all label/symbol layout algorithms

* Fri Mar  4 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.3.4-1.fmi
- Added cppformat dependencies
- Added layout=keyword option to position generation
- Added layout=latlon option to position generation

* Thu Feb 11 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.2.11-1.fmi
- Fixed WindRoseLayer not to crash if a station has no observations, the station is silently skipped

* Tue Feb  9 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.9-1.fmi
- Rebuilt against the new TimeSeries::Value definition

* Mon Feb  8 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.8-2.fmi
- Better fix for segfault issue

* Mon Feb  8 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.8-1.fmi
- Fixed segfault issue in WindRoseLayer

* Thu Feb  4 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.2.4-1.fmi
- Fixed parameter setting in NumberLayer to be done after the positions are generated

* Tue Feb  2 2016 Tuomo Lauri <tuomo.lauri@fmi.fi> - 16.2.2-1.fmi
- Missingvalue is now deprecated in Q-Engine calls

* Fri Jan 29 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.29-1.fmi
- Added possibility to move numbers and symbols based on a direction parameter
- Added graticulefill layout for positions

* Tue Jan 26 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.26-1.fmi
- Fixed pixel to latlon conversion in Positions calculations

* Sat Jan 23 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.23-1.fmi
- Fmi::TimeZoneFactory API changed

* Wed Jan 20 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.20-1.fmi
- Added graticule support for position generation, mostly indended for symbols

* Tue Jan 19 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.19-1.fmi
- Added support for U- and V-components to ArrowLayer
- Explicit infinity checking no longer required, it is done by macgyver parsers

* Mon Jan 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.18-2.fmi
- Throw if bbox contains infinities

* Mon Jan 18 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.18-1.fmi
- newbase API changed, full recompile

* Thu Jan 14 2016 Mika Heiskanen <mika.heiskanen@fmi.fi> - 16.1.14-1.fmi
- Draw coordinate grid fully without clipping it, clip it using SVG for better speed

* Mon Dec 21 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.12.21-1.fmi
- WMS GetCapablities update loop was missing exception handling

* Mon Dec 14 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.12.14-1.fmi
- Added multiplier and offset to ArrowLayer to enable knots in speed selection
- Added southflop option to enable flopping wind barbs in the southern hemisphere
- Enabled aviation chart numbers by adding plusprefix and minusprefix attributes for labels
- Symbol, filter, marker etc modification time is now included in the ETag.
- Symbols, numbers and arrows can now be placed at querydata points
- WMS refactoring: time_column no longer required in PostGIS - layers
- Icemap is now its own dedicated layer

* Thu Dec  3 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.12.3-1.fmi
- New Icemap functionality in PostGisLayer

* Wed Nov 18 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.11.18-1.fmi
- SmartMetPlugin now receives a const HTTP Request

* Tue Nov 10 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.10-2.fmi
- Avoid timezone locks by avoiding unnecessary conversions from ptime to NFmiMetTime in loops

* Tue Nov 10 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.10-1.fmi
- Avoid string streams to avoid global std::locale locks

* Fri Nov  6 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.6-1.fmi
- Removed IceLayer, same functionality can now be achieved using PostGISLayer
- Added optional time truncation to PostGISLayer

* Wed Nov  4 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.11.4-1.fmi
- Using Fmi::to_string instead of WMS::to_string to avoid std::locale locks

* Wed Oct 28 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.10.28-1.fmi
- Added safety checks against deleted files/dirs when scanning WMS directories

* Mon Oct 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.10.26-1.fmi
- Added proper debuginfo packaging
- Removed printouts when WMS layer is not available in certain projections

* Mon Oct 12 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.10.12-1.fmi
- Now setting CORS headers to succesful responses
- Fix segfault by checking symbol is defined before dereferencing it

* Tue Sep 29 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.9.29-1.fmi
- Samping improvements
- Fixed WMS GetCapabilities times

* Mon Sep 28 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.9.28-1.fmi
- Resampling now uses the projection bounding box instead of corner coordinates

* Thu Sep 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.9.24-1.fmi
- Improved WMS bounding box calculations

* Fri Aug 28 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.28-1.fmi
- Allow parameter not to be set in intersections, and just do nothing

* Mon Aug 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.24-1.fmi
- Recompiled due to Convenience.h API changes

* Fri Aug 21 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.8.21-1.fmi
- Fixed segfault issue in WMSGetMap

* Thu Aug 20 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.8.20-1.fmi
- Implemented PostGIS WMS layer

* Tue Aug 18 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.18-1.fmi
- Use time formatters from macgyver to avoid global locks from sstreams

* Mon Aug 17 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.17-1.fmi
- Use -fno-omit-frame-pointer to improve perf use

* Fri Aug 14 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.8.14-1.fmi
- Recompiled due to string formatting changes

* Thu Jul 23 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.7.23-1.fmi
- Improved handling of arrows, symbols and numbers for geographic references

* Wed Jul 22 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.7.22-1.fmi
- Added intersections with isobands for arrows, symbols and numbers

* Tue Jul 21 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.7.21-1.fmi
- Fixed ArrowLayer, SymbolLayer and NumberLayer to work for geographic projections

* Fri Jun 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.26-1.fmi
- Recompiled to get case insensitive JSON expansion from query strings
- SymbolLayer now supports symbol scaling
- SymbolLayer now supports metaparameters such as weathersymbol

* Tue Jun 23 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.6.23-1.fmi
- Location API changed and forced a recompile

* Tue May 12 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.5.12-1.fmi
- Updated to use shared dem & land cover data from geoengine

* Wed Apr 29 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.29-1.fmi
- Added server thread pool deduction
- Fixed WindRoseLayer data validity checks
- Added lake corrections to landscape interpolation

* Fri Apr 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.24-1.fmi
- Pass the set timezone from WindRoseLayer to obsengine

* Tue Apr 14 2015 Santeri Oksman <santeri.oksman@fmi.fi> - 15.4.14-1.fmi
- Rebuild

* Mon Apr 13 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.4.13-1.fmi
- No supporting frontend cache also in dali queries

* Thu Apr  9 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.9-1.fmi
- newbase API changed

* Wed Apr  8 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.4.8-1.fmi
- Implemented frontend cache support for WMS queries

* Fri Mar  6 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.6-2.fmi
- Reverted getCapabilities axis ordering

* Fri Mar  6 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.6-1.fmi
- Fixed getCapabilities Content-Type and simplified MIME handling

* Tue Mar  3 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.3-2.fmi
- Fixed bug in GetCapabilities coordinates generation

* Tue Mar  3 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.3.3-1.fmi
- Now automatically updating GetCapabilities info from WMS directory

* Thu Feb 26 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.26-1.fmi
- Fixed GetCapabilities schema

* Wed Feb 25 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.25-1.fmi
- Fixed WMS GetCapabilities GetMap-URI
- WMS product definition directory structure is now recursive (BRAINSTORM-426)
- Added intersect property for isolines and isobands to enable clipping them by isobands of other parameters
- Added isoband.inside, isoband.outside, isoline.inside and isoline.outside to clip contours by maps

* Tue Feb 24 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.2.24-1.fmi
- Recompiled due to changes in newbase linkage

* Mon Feb 23 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.23-1.fmi
- Fixed bug in projection axis ordering
- Fixed GetCapabilities BoundingBox attributes
- Added development configuration file
- Added more supported EPSGs for WMS

* Fri Feb 20 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.20-1.fmi
- WMS::GetCapabilities response corrected

* Mon Feb 16 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.2.16-1.fmi
- Now using memory-filesystem cache
- allow overriding everything in the product JSON from query string
- disable_format and enable_format are now named disable and enable
- image format is now selected with "type" instead of format (except for WMS)
- changed how crs, bboxcrs, xsize, ysize and bbox are passed from WMS to Dali

* Wed Jan 28 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.1.28-1.fmi
- Fixed error in GetCapabilities message generation

* Mon Jan 26 2015 Mika Heiskanen <mika.heiskanen@fmi.fi> - 15.1.26-1.fmi
- Round symbol coordinates in LocationLayer to integers, full precision is unnecessary
- Added caching of rendered images

* Tue Jan 20 2015 Tuomo Lauri <tuomo.lauri@fmi.fi> - 15.1.20-1.fmi
- WMS file scanner now skips failed product definitions

* Thu Dec 18 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.18-2.fmi
- Fixed WMS filename validator

* Thu Dec 18 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.18-1.fmi
- WMS will no longer read backup files, files not ending with ".json" etc

* Wed Dec 17 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.17-1.fmi
- Added WindRoseLayer

* Wed Dec 10 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.10-1.fmi
- Disable SVG size limits in librsvg

* Mon Dec  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.12.8-1.fmi
- Added IceLayer for rendering ice maps
- Added support for PDF and PS image formats
- Added enable_format and disable_format variables

* Mon Nov 24 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.11.24-1.fmi
- RHEL7 no longed needs a global lock for rendering PNG images

* Fri Oct 24 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.10.24-1.fmi
- Prevent crashes if the given CRS is not valid
- WMS implementation modified: dali products handled as WMS-layers

* Wed Sep 17 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.17-1.fmi
- Added support for minresolution and maxresolution requirements
- WMS support

* Mon Sep  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.8-1.fmi
- Recompiled due to geoengine API changes

* Fri Sep  5 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.5-1.fmi
- Added LocationLayer

* Wed Sep  3 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.3-1.fmi
- Added settings for label: locale, precision, prefix, suffix

* Tue Sep  2 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.9.2-1.fmi
- Lock the SVG renderer until system libraries (fontconfig, pango, cairo) are thread safe

* Fri Aug 29 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.29-2.fmi
- Added multiplier and offset to NumberLayer

* Fri Aug 29 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.29-1.fmi
- Fixed behaviour of query string 'time' option for relative offsets

* Tue Aug 26 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.26-1.fmi
- Added 'marker' as a valid attribute

* Fri Aug 22 2014 Tuomo Lauri <tuomo.lauri@fmi.fi> - 14.8.22-1.fmi
- Fixed memory leaks in layer generation

* Fri Aug  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.8-3.fmi
- Relative times are now rounded up to the next hour

* Fri Aug  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.8-2.fmi
- Rewritten GisEngine API

* Fri Aug  8 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.8-1.fmi
- Changed map.simplification to map.mindistance for consistency

* Thu Aug  7 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.7-4.fmi
- Do not draw arrows when data is missing

* Thu Aug  7 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.7-3.fmi
- Added safety checks against empty maps and contours

* Thu Aug  7 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.7-2.fmi
- Added support for removing too small polygons from maps

* Thu Aug  7 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.7-1.fmi
- Added support for simplification of the map data

* Wed Aug  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.6-2.fmi
- Improved definition for the resolution setting for geographic coordinate systems

* Wed Aug  6 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.8.6-1.fmi
- WKT for latlon and rotated latlon is fixed not to include PROJCS

* Fri Jul 25 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.7.25-1.fmi
- Rebuilt with the latest GDAL

* Wed Jul  2 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.7.2-1.fmi
- JSON files must now give an explicit .css suffix to style files (consistency)
- inside/outside requirements for arrows, numbers etc can now use where-clauses
- overriding SQL related variables is no longer allowed for safety reasons
- isobands now allow specifying the interpolation method, previously "linear" was assumed

* Mon Jun 30 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.6.30-1.fmi
- New release with layers for arrows, numbers, legends, symbols, background

* Wed Jun 11 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.6.11-1.fmi
- New release with refactored style features. Added option style=name.

* Wed Jun  4 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.6.4-1.fmi
- Beta version

* Wed Feb 26 2014 Mika Heiskanen <mika.heiskanen@fmi.fi> - 14.2.26-1.fmi
- Alpha version

