PROGS = test_label_placement test_subdivide_gate test_isoline_filter_validation \
        test_smoother_options test_mvt_geometry

CXX      = g++
CXXFLAGS = -std=c++17 -O0 -g -Wall -Wextra \
           -DBOOST_TEST_DYN_LINK \
           -I../../wms \
           -I/usr/include/smartmet

LIBS     = -lboost_unit_test_framework

comma := ,

# GDAL lives in a versioned prefix (e.g. /usr/gdal312); discover it the same way
# the shared makefile.inc does. Used by the IsolineFilter validation test, which
# pulls in OGR geometry headers and links the custom GDAL.
GDAL_CONFIG := $(firstword $(wildcard /usr/gdal*/bin/gdal-config) gdal-config)
GDAL_CFLAGS := $(shell $(GDAL_CONFIG) --cflags 2>/dev/null)
GDAL_LIBS   := $(shell $(GDAL_CONFIG) --libs 2>/dev/null)
GDAL_PREFIX := $(shell $(GDAL_CONFIG) --prefix 2>/dev/null)

# libconfig also lives in a versioned prefix (e.g. /usr/libconfig17).
LIBCONFIG_INC := $(firstword $(wildcard /usr/libconfig*/include))
LIBCONFIG_LIB := $(firstword $(wildcard /usr/libconfig*/lib))

LABEL_SRCS = test_label_placement.cpp \
             ../../wms/LabelPlacement.cpp

SUBDIVIDE_SRCS = test_subdivide_gate.cpp

# The validation test exercises the real IsolineFilter::apply(), so it links the
# plugin objects (built by the top-level Makefile) rather than recompiling them
# with their libconfig/etc. include paths.
ISOFILTER_OBJS = ../../obj/IsolineFilter.o ../../obj/BezierFit.o ../../obj/BezierCache.o \
                 ../../obj/JsonTools.o ../../obj/Time.o

.PHONY: all test clean

all: $(PROGS)

test_label_placement: $(LABEL_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

test_subdivide_gate: $(SUBDIVIDE_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) -lsmartmet-gis

# Smoother::init() parsing only needs Smoother.o + Config.o (Config("") is a
# cheap empty config). Both are built by the top-level Makefile.
SMOOTHER_OBJS = ../../obj/Smoother.o ../../obj/Config.o

$(SMOOTHER_OBJS):
	$(MAKE) -C ../.. obj/$(notdir $@)

test_smoother_options: test_smoother_options.cpp $(SMOOTHER_OBJS)
	$(CXX) $(CXXFLAGS) $(if $(LIBCONFIG_INC),-I$(LIBCONFIG_INC)) -o $@ $< $(SMOOTHER_OBJS) \
	  -lsmartmet-spine -lsmartmet-gis -lsmartmet-macgyver -lsmartmet-newbase \
	  -lsmartmet-trax -ljsoncpp -lfmt -lpqxx \
	  $(if $(LIBCONFIG_LIB),-L$(LIBCONFIG_LIB) -Wl$(comma)-rpath$(comma)$(LIBCONFIG_LIB)) -lconfig++ $(LIBS)

# Build any missing plugin objects via the top-level Makefile.
$(ISOFILTER_OBJS):
	$(MAKE) -C ../.. obj/$(notdir $@)

test_isoline_filter_validation: test_isoline_filter_validation.cpp $(ISOFILTER_OBJS)
	$(CXX) $(CXXFLAGS) $(GDAL_CFLAGS) -o $@ $< $(ISOFILTER_OBJS) \
	  -lsmartmet-gis -lsmartmet-spine -lsmartmet-macgyver -lsmartmet-newbase \
	  $(GDAL_LIBS) -Wl,-rpath,$(GDAL_PREFIX)/lib -ljsoncpp -lfmt $(LIBS)

# The MVT geometry test exercises the real encoder, so it links the plugin
# objects (built by the top-level Makefile) and pulls in OGR + protobuf.
MVT_OBJS = ../../obj/MapboxVectorTile.o ../../obj/vector_tile.pb.o

$(MVT_OBJS):
	$(MAKE) -C ../.. obj/$(notdir $@)

test_mvt_geometry: test_mvt_geometry.cpp $(MVT_OBJS)
	$(CXX) $(CXXFLAGS) $(GDAL_CFLAGS) -o $@ $< $(MVT_OBJS) \
	  -lsmartmet-macgyver $(GDAL_LIBS) -Wl,-rpath,$(GDAL_PREFIX)/lib -lprotobuf $(LIBS)

test: $(PROGS)
	./test_label_placement --log_level=message
	./test_subdivide_gate --log_level=message
	./test_isoline_filter_validation --log_level=message
	./test_smoother_options --log_level=message
	./test_mvt_geometry --log_level=message

clean:
	rm -f $(PROGS)
