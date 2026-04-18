SUBNAME = wms
SPEC = smartmet-plugin-$(SUBNAME)
INCDIR = smartmet/plugins/$(SUBNAME)

REQUIRES = gdal jsoncpp cairo fmt librsvg ctpp2 configpp webp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc
sysconfdir ?= /etc

# Compiler options

FLAGS += -Wno-maybe-uninitialized -Wno-variadic-macros -Wno-deprecated-declarations

DEFINES = -DUNIX -D_REENTRANT -DWITHOUT_OSM

# Local-build override for smartmet-library-dynlib: point DYNLIB_STAGE at
# a staging tree (e.g. $HOME/smartmet-dynlib-staging) to link against a
# not-yet-installed copy. If the directory does not exist, the build
# falls back to the system-installed headers and library, which is what
# the RPM / CI build uses.
DYNLIB_STAGE ?= $(HOME)/smartmet-dynlib-staging
ifneq ($(wildcard $(DYNLIB_STAGE)/include/smartmet/dynlib),)
  INCLUDES += -I$(DYNLIB_STAGE)/include/smartmet
  DYNLIB_LIB_FLAGS := -L$(DYNLIB_STAGE)/lib64 -Wl,-rpath,$(DYNLIB_STAGE)/lib64
endif

LIBS += $(PREFIX_LDFLAGS) \
	-lsmartmet-grid-content \
	-lsmartmet-timeseries \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-gis \
	-lsmartmet-giza \
	-lsmartmet-locus \
	$(DYNLIB_LIB_FLAGS) -lsmartmet-dynlib \
	$(REQUIRED_LIBS) \
	-lboost_thread \
	-lboost_regex \
	-lboost_iostreams \
	-lbz2 -lz \
	-lheatmap \
	-lprotobuf

# Templates

TEMPLATES = $(wildcard tmpl/*.tmpl)
BYTECODES = $(TEMPLATES:%.tmpl=%.c2t)

# What to install

LIBFILE = $(SUBNAME).so

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)

# The files to be compiled

SRCS = $(wildcard $(SUBNAME)/*.cpp)
HDRS = $(wildcard $(SUBNAME)/*.h)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

WMS_SRCS = $(wildcard $(SUBNAME)/wms/*.cpp)
WMS_OBJS = $(patsubst $(SUBNAME)/wms/%.cpp, obj/wms/%.o, $(WMS_SRCS))

OGC_SRCS = $(wildcard $(SUBNAME)/ogc/*.cpp)
OGC_OBJS = $(patsubst $(SUBNAME)/ogc/%.cpp, obj/ogc/%.o, $(OGC_SRCS))

WMTS_SRCS = $(wildcard $(SUBNAME)/wmts/*.cpp)
WMTS_OBJS = $(patsubst $(SUBNAME)/wmts/%.cpp, obj/wmts/%.o, $(WMTS_SRCS))

TILES_SRCS = $(wildcard $(SUBNAME)/tiles/*.cpp)
TILES_OBJS = $(patsubst $(SUBNAME)/tiles/%.cpp, obj/tiles/%.o, $(TILES_SRCS))

OBJS += $(WMS_OBJS) $(OGC_OBJS) $(WMTS_OBJS) $(TILES_OBJS)

# Protobuf-generated sources (.cc extension, not caught by the *.cpp wildcard)
PROTO_SRCS = $(SUBNAME)/vector_tile.pb.cc
PROTO_OBJS = obj/vector_tile.pb.o
OBJS += $(PROTO_OBJS)

INCLUDES := -I$(SUBNAME) $(INCLUDES)

.PHONY: test test-dali test-wms test-wmts test-tiles update-outputs rpm

# The rules

all: objdir proto $(LIBFILE) templates
debug: all
release: all
profile: all

.PHONY: proto
proto: $(SUBNAME)/vector_tile.pb.h $(SUBNAME)/vector_tile.pb.cc

$(SUBNAME)/vector_tile.pb.cc $(SUBNAME)/vector_tile.pb.h: $(SUBNAME)/vector_tile.proto
	protoc --cpp_out=$(SUBNAME) --proto_path=$(SUBNAME) $<

$(PROTO_OBJS): $(PROTO_SRCS) | objdir
	$(CXX) $(CFLAGS) $(INCLUDES) -Wno-error -c -MD -MF obj/vector_tile.pb.d -MT $@ -o $@ $<

# Ensure proto header is generated before any TU that includes it
$(OBJS): | $(SUBNAME)/vector_tile.pb.h

$(LIBFILE): $(OBJS)
	$(CXX) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol | grep -v SmartMet::Engine:: ; \
		then rm -v $(LIBFILE); \
		exit 1; \
	fi

configtest:
	@if [ -x "$$(command -v cfgvalidate)" ]; then cfgvalidate -v test/cnf/wms.conf; fi

jsontest:
	@if [ -e /usr/bin/json_verify ]; then \
	  echo Validating json files ; \
	  for json in $$(find test/dali -name '*.json'); do \
	    echo " -> $$json"; \
	    cat $$json | json_verify -c | grep -v "JSON is valid"; \
	  done; \
	fi

clean:
	rm -f $(LIBFILE)
	rm -rf $(objdir)
	rm -f $(SUBNAME)/vector_tile.pb.h $(SUBNAME)/vector_tile.pb.cc
	find . -name '*~' -exec rm -f {} ';'
	find . -name '*.c2t' -exec rm -f {} ';'
	find . -name '*.d' -exec rm -f {} ';'
	find . -name '.#*' -exec rm -f {} ';'
	$(MAKE) -C test $@


format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp $(SUBNAME)/wms/*.h $(SUBNAME)/wms/*.cpp $(SUBNAME)/ogc/*.h $(SUBNAME)/ogc/*.cpp $(SUBNAME)/wmts/*.h $(SUBNAME)/wmts/*.cpp $(SUBNAME)/tiles/*.h $(SUBNAME)/tiles/*.cpp test/*.cpp

install:
	@mkdir -p $(plugindir)
	$(INSTALL_PROG) $(LIBFILE) $(plugindir)/$(LIBFILE)
	@mkdir -p $(datadir)/smartmet/wms
	@list=`ls -1 tmpl/*.c2t`; \
	echo $(INSTALL_DATA) $$list "$(datadir)/smartmet/wms"; \
        $(INSTALL_DATA) $$list "$(datadir)/smartmet/wms";

test: configtest
	cd test && make test

test-dali test-wms test-wmts test-tiles update-outputs:
	cd test && make $@

objdir:
	@mkdir -p $(objdir) $(objdir)/wms $(objdir)/ogc $(objdir)/wmts $(objdir)/tiles

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude docs --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

obj/wms/%.o: $(SUBNAME)/wms/%.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF obj/wms/$*.d -MT $@ -o $@ $<

obj/ogc/%.o: $(SUBNAME)/ogc/%.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF obj/ogc/$*.d -MT $@ -o $@ $<

obj/wmts/%.o: $(SUBNAME)/wmts/%.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF obj/wmts/$*.d -MT $@ -o $@ $<

obj/tiles/%.o: $(SUBNAME)/tiles/%.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF obj/tiles/$*.d -MT $@ -o $@ $<

templates: $(BYTECODES)

%.c2t:	%.tmpl
	ctpp2c $< $@

-include $(wildcard obj/*.d)
-include $(wildcard obj/wms/*.d)
-include $(wildcard obj/ogc/*.d)
-include $(wildcard obj/wmts/*.d)
-include $(wildcard obj/tiles/*.d)
-include obj/vector_tile.pb.d
