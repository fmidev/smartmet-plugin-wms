PROGS = test_label_placement test_subdivide_gate test_isoline_filter_validation

CXX      = g++
CXXFLAGS = -std=c++17 -O0 -g -Wall -Wextra \
           -DBOOST_TEST_DYN_LINK \
           -I../../wms \
           -I/usr/include/smartmet

LIBS     = -lboost_unit_test_framework

# GDAL lives in a versioned prefix (e.g. /usr/gdal312); discover it the same way
# the shared makefile.inc does. Used by the IsolineFilter validation test, which
# pulls in OGR geometry headers and links the custom GDAL.
GDAL_CONFIG := $(firstword $(wildcard /usr/gdal*/bin/gdal-config) gdal-config)
GDAL_CFLAGS := $(shell $(GDAL_CONFIG) --cflags 2>/dev/null)
GDAL_LIBS   := $(shell $(GDAL_CONFIG) --libs 2>/dev/null)
GDAL_PREFIX := $(shell $(GDAL_CONFIG) --prefix 2>/dev/null)

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

# Build any missing plugin objects via the top-level Makefile.
$(ISOFILTER_OBJS):
	$(MAKE) -C ../.. obj/$(notdir $@)

test_isoline_filter_validation: test_isoline_filter_validation.cpp $(ISOFILTER_OBJS)
	$(CXX) $(CXXFLAGS) $(GDAL_CFLAGS) -o $@ $< $(ISOFILTER_OBJS) \
	  -lsmartmet-gis -lsmartmet-spine -lsmartmet-macgyver -lsmartmet-newbase \
	  $(GDAL_LIBS) -Wl,-rpath,$(GDAL_PREFIX)/lib -ljsoncpp -lfmt $(LIBS)

test: $(PROGS)
	./test_label_placement --log_level=message
	./test_subdivide_gate --log_level=message
	./test_isoline_filter_validation --log_level=message

clean:
	rm -f $(PROGS)
