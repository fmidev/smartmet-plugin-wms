SUBNAME = wms
SPEC = smartmet-plugin-$(SUBNAME)
INCDIR = smartmet/plugins/$(SUBNAME)

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always
CXX_STD ?= c++11

FLAGS = -std=$(CXX_STD) -MD -fPIC -rdynamic -Wall -W \
	-fdiagnostics-color=$(GCC_DIAG_COLOR) \
	-Wno-unused-parameter \
	-fno-omit-frame-pointer \
	-Wno-deprecated-declarations \
	-Wno-unknown-pragmas

FLAGS_DEBUG = \
	-Wcast-align \
	-Wcast-qual \
	-Winline \
	-Wno-multichar \
	-Wno-pmf-conversions \
	-Wpointer-arith \
	-Wwrite-strings \
	-Wsign-promo


FLAGS_RELEASE = -Wuninitialized

# Note:  -Weffc++ was bugged, use only with v3.2 or later
# Note:  -Wold-style-cast is OK in Fedora, not in RHEL6
# Ditto: -Wunreachable-code

DIFFICULTFLAGS = \
	-Weffc++ \
	-Wunreachable-code \
	-Wconversion \
	-Wold-style-cast \
	-ansi -pedantic


ARFLAGS = -r
DEFINES = -DUNIX -DWGS84 -D_REENTRANT

# Default compile options

CFLAGS_RELEASE = $(DEFINES) $(FLAGS) $(FLAGS_RELEASE) -DNDEBUG -O2 -g
CFLAGS_DEBUG   = $(DEFINES) $(FLAGS) $(FLAGS_DEBUG)   -Werror -O0 -g 

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  override CFLAGS += $(CFLAGS_DEBUG)
else
  override CFLAGS += $(CFLAGS_RELEASE)
endif

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(origin sysconfdir), undefined)
  sysconfdir = /etc
else
  sysconfdir = $(sysconfdir)
endif


ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

bindir = $(PREFIX)/bin
includedir = $(PREFIX)/include
datadir = $(PREFIX)/share
enginedir = $(datadir)/smartmet/engines
plugindir = $(datadir)/smartmet/plugins
confdir = $(sysconfdir)/smartmet/plugins/wms
objdir = obj

# Boost 1.69

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -I/usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

INCLUDES += -I$(includedir) \
	-I$(includedir)/smartmet \
	-I$(includedir)/mysql \
	-I$(includedir)/oracle/11.2/client64 \
	-I$(includedir)/soci \
	-I$(PREFIX)/gdal30/include \
	`pkg-config --cflags librsvg-2.0` \
	`pkg-config --cflags cairo` \
	`pkg-config --cflags jsoncpp`

LIBS += -L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-gis \
	-lsmartmet-giza \
	`pkg-config --libs librsvg-2.0` \
	`pkg-config --libs cairo` \
	`pkg-config --libs jsoncpp` \
	-lfmt \
	-lctpp2 \
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

LIBFILE = $(SUBNAME).so

# How to install

INSTALL_PROG = install -p -m 775
INSTALL_DATA = install -p -m 664

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)
vpath %.o $(objdir)

# The files to be compiled

SRCS = $(patsubst $(SUBNAME)/%,%,$(wildcard $(SUBNAME)/*.cpp))
HDRS = $(patsubst $(SUBNAME)/%,%,$(wildcard $(SUBNAME)/*.h))
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

INCLUDES := -I$(SUBNAME) $(INCLUDES)

.PHONY: test rpm jsontest

# The rules

all: objdir $(LIBFILE) $(BYTECODES)
debug: all
release: all
profile: all

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

$(LIBFILE): $(OBJS) Makefile
	$(CXX) $(CFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)

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
	@mkdir -p $(confdir)
	@mkdir -p $(confdir)/tmpl
	@list=`ls -1 tmpl/*.c2t`; \
	echo $(INSTALL_DATA) $$list "$(confdir)/tmpl"; \
        $(INSTALL_DATA) $$list "$(confdir)/tmpl";

test:	configtest
	cd test && make test

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -ta $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.c2t:	%.tmpl
	ctpp2c $< $@

-include $(wildcard obj/*.d)


