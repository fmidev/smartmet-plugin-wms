SUBNAME = wms
SPEC = smartmet-plugin-grib$(SUBNAME)
INCDIR = smartmet/plugins/$(SUBNAME)

REQUIRES = gdal jsoncpp cairo fmt librsvg ctpp2

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc
sysconfdir ?= /etc

# Compiler options

CFLAGS += -Wno-maybe-uninitialized

DEFINES = -DUNIX -D_REENTRANT -DWGS84
# DEFINES = -DUNIX -D_REENTRANT -DNEW_NFMIAREA

LIBS += -L$(libdir) \
	$(REQUIRED_LIBS) \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-gis \
	-lsmartmet-giza \
	-lsmartmet-locus \
	-lboost_date_time \
	-lboost_thread \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_filesystem \
	-lboost_system \
	-lbz2 -lz \
	-lheatmap

# Templates

TEMPLATES = $(wildcard tmpl/*.tmpl)
BYTECODES = $(TEMPLATES:%.tmpl=%.c2t)

# What to install

LIBFILE = grib$(SUBNAME).so

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)

# The files to be compiled

SRCS = $(wildcard $(SUBNAME)/*.cpp)
HDRS = $(wildcard $(SUBNAME)/*.h)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

INCLUDES := -I$(SUBNAME) $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE) templates
debug: all
release: all
profile: all

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
	find . -name '*~' -exec rm -f {} ';'
	find . -name '*.c2t' -exec rm -f {} ';'
	find . -name '*.d' -exec rm -f {} ';'
	find . -name '.#*' -exec rm -f {} ';'


format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp test/*.cpp

install:
	@mkdir -p $(plugindir)
	$(INSTALL_PROG) $(LIBFILE) $(plugindir)/$(LIBFILE)
	@mkdir -p $(sysconfdir)/smartmet/plugins/wms/tmpl
	@list=`ls -1 tmpl/*.c2t`; \
	echo $(INSTALL_DATA) $$list "$(sysconfdir)/smartmet/plugins/wms/tmpl"; \
        $(INSTALL_DATA) $$list "$(sysconfdir)/smartmet/plugins/wms/tmpl";

test: configtest
	cd test && make test

objdir:
	@mkdir -p $(objdir)

# Forcibly lower RPM_BUILD_NCPUs in CircleCI cloud(but not on local builds)
RPMBUILD=$(shell test "$$CIRCLE_BUILD_NUM" && echo RPM_BUILD_NCPUS=2 rpmbuild || echo rpmbuild)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	$(RPMBUILD) -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

templates: $(BYTECODES)

%.c2t:	%.tmpl
	ctpp2c $< $@

-include $(wildcard obj/*.d)
