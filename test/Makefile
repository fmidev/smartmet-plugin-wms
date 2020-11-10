PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always
CXX_STD ?= c++11

MAINFLAGS = -std=$(CXX_STD) -Wall -W -Wno-unused-parameter -Wno-unknown-pragmas

EXTRAFLAGS = -Werror -pedantic -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Winline \
	-Wctor-dtor-privacy -Wnon-virtual-dtor -Wno-pmf-conversions \
	-Wsign-promo -Wchar-subscripts -Wold-style-cast \
	-Wshadow -Woverloaded-virtual

DIFFICULTFLAGS = -Weffc++ -Wunreachable-code -Wredundant-decls

CFLAGS = -DUNIX -O0 -g $(MAINFLAGS)

# Boost 1.69

ifneq "$(wildcard /usr/include/boost169)" ""
  INCLUDES += -isystem /usr/include/boost169
  LIBS += -L/usr/lib64/boost169
endif

ifneq "$(wildcard /usr/gdal30/include)" ""
  INCLUDES += -isystem /usr/gdal30/include
  LIBS += -L$(PREFIX)/gdal30/lib
else
  INCLUDES += -isystem /usr/include/gdal
endif

INCLUDES += \
	-I../wms \
	-I/usr/include/smartmet \
	-isystem /usr/include/mysql \
	-isystem /usr/include/oracle/11.2/client64 \
	-isystem /usr/include/soci \
	`pkg-config --cflags libconfig++` \
	`pkg-config --cflags jsoncpp`

LIBS += -lsmartmet-spine  \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-giza \
	`pkg-config --libs libconfig++` \
	`pkg-config --libs jsoncpp` \
	-lctpp2 \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_serialization \
	-lboost_system \
	-lfmt \
	-lbz2 -lz -lpthread -ldl

all: $(PROG)
clean:
	rm -f $(PROG) *~ Plugintest failures/*

ifdef CI
test: $(PROG) fminames
else
test: $(PROG)
endif
	rm -f failures/*
	./TestRunner.sh
	@if [ -e output/wms_getcapabilities.get ]; then \
	   echo Validating output/wms_getcapabilities.get; \
	   cat output/wms_getcapabilities.get | sed '/<inspire_vs:ExtendedCapabilities>/,/<\/inspire_vs:ExtendedCapabilities>/d' | xmllint -o /dev/null --schema http://schemas.opengis.net/wms/1.3.0/capabilities_1_3_0.xsd -; \
	fi
	@rm -f wms-access-log
	@if [ -e failures/wms_getcapabilities.get ]; then \
	   echo Validating failures/wms_getcapabilities.get; \
	   cat failures/wms_getcapabilities.get | sed '/<inspire_vs:ExtendedCapabilities>/,/<\/inspire_vs:ExtendedCapabilities>/d' | xmllint -o /dev/null --schema http://schemas.opengis.net/wms/1.3.0/capabilities_1_3_0.xsd -; \
	fi


fminames:
	@sed -i -e 's/"smartmet-test"/"localhost"/g' cnf/*.conf
	@/usr/share/smartmet/test/db/init-and-start.sh && /usr/share/smartmet/test/db/install-test-db.sh > /dev/null


$(PROG) : % : %.cpp Makefile ../wms.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
