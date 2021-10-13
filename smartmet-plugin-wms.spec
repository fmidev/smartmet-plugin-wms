%bcond_without authentication
%bcond_without observation
%define DIRNAME wms
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet WMS/Dali plugin
Name: %{SPECNAME}
Version: 21.10.13
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-wms
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: rpm-build
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: boost169-devel
BuildRequires: rpm-build
BuildRequires: smartmet-library-giza-devel >= 21.6.18
BuildRequires: smartmet-library-grid-content-devel >= 21.10.11
BuildRequires: smartmet-library-grid-files-devel >= 21.10.11
BuildRequires: smartmet-library-macgyver-devel >= 21.10.4
BuildRequires: smartmet-library-spine-devel >= 21.10.11
BuildRequires: smartmet-library-giza-devel
%if %{with authentication}
BuildRequires: smartmet-engine-authentication-devel >= 21.9.9
%endif
%if %{with observation}
BuildRequires: smartmet-engine-observation-devel >= 21.9.20
%endif
BuildRequires: smartmet-engine-gis-devel >= 21.9.13
BuildRequires: smartmet-engine-grid-devel >= 21.10.11
BuildRequires: smartmet-engine-geonames-devel >= 21.9.28
BuildRequires: smartmet-engine-querydata-devel >= 21.9.13
BuildRequires: smartmet-engine-contour-devel >= 21.9.13
BuildRequires: smartmet-library-gis-devel >= 21.9.24
BuildRequires: fmt-devel >= 7.1.3
BuildRequires: ctpp2 >= 2.8.8
BuildRequires: jsoncpp-devel
# BuildRequires: flex-devel
BuildRequires: cairo-devel
BuildRequires: bzip2-devel
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
Requires: fmt >= 7.1.3
Requires: jsoncpp
Requires: ctpp2 >= 2.8.8
# Default font for some layers:
Requires: google-roboto-fonts
Requires: smartmet-library-grid-content >= 21.10.11
Requires: smartmet-library-grid-files >= 21.10.11
Requires: smartmet-library-gis >= 21.9.24
Requires: smartmet-library-macgyver >= 21.10.4
Requires: smartmet-library-spine >= 21.10.11
Requires: smartmet-library-giza >= 21.6.18
%if %{with authentication}
Requires: smartmet-engine-authentication >= 21.9.9
%endif
Requires: smartmet-engine-querydata >= 21.9.13
Requires: smartmet-engine-contour >= 21.9.13
Requires: smartmet-engine-gis >= 21.9.13
Requires: smartmet-engine-grid >= 21.10.11
Requires: smartmet-engine-geonames >= 21.9.28
Requires: smartmet-server >= 21.9.7
Requires: smartmet-library-spine >= 21.10.11
Requires: boost169-date-time
Requires: boost169-filesystem
Requires: boost169-iostreams
Requires: boost169-regex
Requires: boost169-system
Requires: boost169-thread
Provides: %{SPECNAME}
Obsoletes: smartmet-brainstorm-dali < 16.11.1
Obsoletes: smartmet-brainstorm-dali-debuginfo < 16.11.1
#TestRequires: boost169-devel
#TestRequires: bzip2-devel
#TestRequires: fmt-devel
#TestRequires: gcc-c++
#TestRequires: jsoncpp-devel
#TestRequires: ImageMagick
#TestRequires: bc
#TestRequires: smartmet-engine-contour-devel >= 21.8.17
#TestRequires: smartmet-engine-geonames-devel >= 21.8.17
#TestRequires: smartmet-engine-gis-devel >= 21.9.7
#TestRequires: smartmet-engine-querydata-devel >= 21.9.13
#TestRequires: smartmet-library-giza-devel >= 21.6.18
#TestRequires: smartmet-library-newbase-devel >= 20.10.28
#TestRequires: smartmet-library-spine-devel >= 21.8.21
#TestRequires: smartmet-engine-grid-devel >= 21.9.7
#TestRequires: smartmet-engine-grid-test
#TestRequires: smartmet-test-data
#TestRequires: smartmet-test-db
#TestRequires: google-roboto-fonts
#TestRequires: zlib-devel
#TestRequires: cairo-devel
#TestRequires: redis
%if %{with observation}
#TestRequires: smartmet-engine-observation-devel >= 21.9.7
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
%{_sysconfdir}/smartmet/plugins/wms/tmpl/*.c2t

%changelog

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

