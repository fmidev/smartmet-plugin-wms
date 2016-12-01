SUBNAME = wms
SPEC = smartmet-plugin-$(SUBNAME)
INCDIR = smartmet/plugins/$(SUBNAME)

FLAGS = -std=c++11 -MD -fPIC -rdynamic -Wall -W \
	-fdiagnostics-color=always \
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
	-Woverloaded-virtual  \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wwrite-strings \
	-Wsign-promo


FLAGS_RELEASE = -Wuninitialized

# Note:  -Weffc++ was bugged, use only with v3.2 or later
# Note:  -Wold-style-cast is OK in Fedora, not in RHEL6
# Ditto: -Wunreachable-code

DIFFICULTFLAGS = \
	-Weffc++ \
	-Wunreachable-code \
	-Wshadow \
	-Wconversion \
	-Wold-style-cast \
	-ansi -pedantic


CXX = g++
ARFLAGS = -r
DEFINES = -DUNIX -D_REENTRANT

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

INCLUDES = -I$(includedir) \
	-I$(includedir)/smartmet \
	-I$(includedir)/mysql \
	-I$(includedir)/oracle/11.2/client64 \
	-I$(includedir)/soci \
	`pkg-config --cflags librsvg-2.0` \
	`pkg-config --cflags cairo` \
	`pkg-config --cflags jsoncpp`

LIBS = -L$(libdir) \
	-lsmartmet-spine \
	-lsmartmet_newbase \
	-lsmartmet_macgyver \
	-lsmartmet_gis \
	-lsmartmet_giza \
	`pkg-config --libs librsvg-2.0` \
	`pkg-config --libs cairo` \
	`pkg-config --libs jsoncpp` \
	-lctpp2 \
	-lboost_date_time \
	-lboost_thread \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_filesystem \
	-lboost_system \
	-lbz2 -lz

# rpm variables

rpmsourcedir = /tmp/$(shell whoami)/rpmbuild

rpmerr = "There's no spec file ($(SPEC).spec). RPM wasn't created. Please make a spec file or copy and rename it into $(SPEC).spec"

rpmversion := $(shell grep "^Version:" $(SPEC).spec  | cut -d\  -f 2 | tr . _)
rpmrelease := $(shell grep "^Release:" $(SPEC).spec  | cut -d\  -f 2 | tr . _)

# Templates

TEMPLATES = $(wildcard tmpl/*.tmpl)
BYTECODES = $(TEMPLATES:%.tmpl=%.c2t)

# What to install

LIBFILE = $(SUBNAME).so

# How to install

INSTALL_PROG = install -p -m 775
INSTALL_DATA = install -p -m 664

# Compilation directories

vpath %.cpp source
vpath %.h include
vpath %.o $(objdir)

# The files to be compiled

SRCS = $(patsubst source/%,%,$(wildcard source/*.cpp))
HDRS = $(patsubst include/%,%,$(wildcard include/*.h))
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

INCLUDES := -Iinclude $(INCLUDES)

.PHONY: test rpm jsontest

# The rules

all: objdir $(LIBFILE) $(BYTECODES)
debug: all
release: all
profile: all

configtest:
	@echo Validating test/cnf/wms.conf
	@cfgvalidate -v test/cnf/wms.conf

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
	@rm -f $(LIBFILE)
	@rm -rf $(objdir)
	@find . -name '*~' -exec rm -f {} ';'
	@find . -name '*.c2t' -exec rm -f {} ';'
	@find . -name '*.d' -exec rm -f {} ';'
	@find . -name '.#*' -exec rm -f {} ';'

format:
	clang-format -i -style=file include/*.h source/*.cpp test/*.cpp

install:
	@mkdir -p $(plugindir)
	$(INSTALL_PROG) $(LIBFILE) $(plugindir)/$(LIBFILE)
	@mkdir -p $(confdir)
	@mkdir -p $(confdir)/tmpl
	@mkdir -p $(confdir)/wms
	@list=`ls -1 tmpl/*.c2t`; \
	echo $(INSTALL_DATA) $$list "$(confdir)/tmpl"; \
        $(INSTALL_DATA) $$list "$(confdir)/tmpl";

test:	configtest
	cd test && make test

objdir:
	@mkdir -p $(objdir)

rpm: clean
	@if [ -e $(SPEC).spec ]; \
	then \
	  smartspecupdate $(SPEC).spec ; \
	  mkdir -p $(rpmsourcedir) ; \
	  tar -C ../../ -cf $(rpmsourcedir)/$(SPEC).tar plugins/$(SUBNAME) ; \
	  gzip -f $(rpmsourcedir)/$(SPEC).tar ; \
	  TAR_OPTIONS=--wildcards rpmbuild -v -ta $(rpmsourcedir)/$(SPEC).tar.gz ; \
	  rm -f $(rpmsourcedir)/$(SPEC).tar.gz ; \
	else \
	  echo $(rpmerr); \
	fi;

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o : %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.c2t:	%.tmpl
	ctpp2c $< $@

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif


