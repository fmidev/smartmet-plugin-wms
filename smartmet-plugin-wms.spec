%bcond_without authentication
%bcond_without observation
%define DIRNAME wms
%define SPECNAME smartmet-plugin-%{DIRNAME}
Summary: SmartMet WMS/Dali plugin
Name: %{SPECNAME}
Version: 17.3.29
Release: 1%{?dist}.fmi
License: MIT
Group: SmartMet/Plugins
URL: https://github.com/fmidev/smartmet-plugin-wms
Source0: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: boost-devel
BuildRequires: libconfig >= 1.4.9
BuildRequires: smartmet-library-giza-devel >= 16.12.21
BuildRequires: smartmet-library-macgyver >= 17.3.14
BuildRequires: smartmet-library-spine-devel >= 17.3.16
%if %{with authentication}
BuildRequires: smartmet-engine-authentication-devel >= 17.3.15
%endif
%if %{with authentication}
BuildRequires: smartmet-engine-observation-devel >= 17.3.15
%endif
BuildRequires: smartmet-engine-gis-devel >= 17.3.15
BuildRequires: smartmet-engine-geonames-devel >= 17.3.15
BuildRequires: smartmet-engine-querydata-devel >= 17.3.15
BuildRequires: smartmet-engine-contour-devel >= 17.3.15
BuildRequires: smartmet-library-gis-devel >= 17.3.14
BuildRequires: fmt-devel
BuildRequires: ctpp2 >= 2.8.2
BuildRequires: jsoncpp-devel
# BuildRequires: flex-devel
BuildRequires: librsvg2-devel >= 2.40.6
BuildRequires: cairo-devel
Requires: cairo
Requires: fmt
Requires: jsoncpp
Requires: ctpp2 >= 2.8.2
Requires: libconfig
Requires: librsvg2 >= 2.40.6
Requires: smartmet-library-gis >= 17.3.14
Requires: smartmet-library-macgyver >= 17.3.14
Requires: smartmet-library-giza >= 16.12.21
%if %{with authentication}
Requires: smartmet-engine-authentication >= 17.3.15
%endif
Requires: smartmet-engine-querydata >= 17.3.15
Requires: smartmet-engine-contour >= 17.3.15
Requires: smartmet-engine-gis >= 17.3.15
Requires: smartmet-engine-geonames >= 17.3.15
Requires: smartmet-server >= 17.3.15
Requires: smartmet-library-spine >= 17.3.16
Requires: boost-date-time
Requires: boost-filesystem
Requires: boost-iostreams
Requires: boost-regex
Requires: boost-system
Requires: boost-thread
Provides: %{SPECNAME}
Obsoletes: smartmet-brainstorm-dali < 16.11.1
Obsoletes: smartmet-brainstorm-dali-debuginfo < 16.11.1

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
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/smartmet/plugins/wms/tmpl

%clean
rm -rf $RPM_BUILD_ROOT

%Files
%defattr(0775,root,root,0775)
%{_datadir}/smartmet/plugins/%{DIRNAME}.so
%defattr(0664,root,root,0775)
%{_sysconfdir}/smartmet/plugins/%{DIRNAME}/tmpl/*.c2t

%changelog
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

