PROG = PluginTest

REQUIRES = gdal jsoncpp configpp ctpp2 fmt librsvg

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -DUNIX -DWGS84 -O0 -g $(FLAGS) -Wno-unknown-pragmas

INCLUDES += \
	-I../wms \
	-I/usr/include/smartmet \
	-isystem /usr/include/mysql \
	-isystem /usr/include/oracle/11.2/client64 \
	-isystem /usr/include/soci

LIBS += 

LIBS += $(REQUIRED_LIBS) \
	-lsmartmet-grid-files \
	-lsmartmet-spine  \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-giza \
	-lboost_date_time \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_serialization \
	-lboost_system \
	-lbz2 -lz -lpthread -ldl

TEST_PREPARE_TARGETS := templates cnf/authentication.conf cnf/geonames.conf cnf/gis.conf
TEST_FINISH_TARGETS := dummy
TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

ifdef CI
GEONAMES_HOST_EDIT := sed -e 's/"smartmet-test"/"localhost"/g'
TEST_PREPARE_TARGETS += geonames-database start-geonames-db
TEST_FINISH_TARGETS += stop-geonames-db
else
GEONAMES_HOST_EDIT := cat
endif

all: $(PROG)

clean:
	rm -f $(PROG) *~
	rm -f cnf/authentication.conf cnf/geonames.conf cnf/gis.conf
	-$(MAKE) stop-geonames-db
	rm -rm tmp-geonames-db*

test: $(PROG) $(TEST_PREPARE_TARGETS)
	rm -f failures/*
	if ./TestRunner.sh; then \
		ok=true; $(MAKE) $(TEST_FINISH_TARGETS); \
	else \
		ok=false; $(MAKE) $(TEST_FINISH_TARGETS); \
	fi; $$ok
	@if [ -e output/wms_getcapabilities.get ]; then \
	   echo Validating output/wms_getcapabilities.get; \
	   cat output/wms_getcapabilities.get | sed '/<inspire_vs:ExtendedCapabilities>/,/<\/inspire_vs:ExtendedCapabilities>/d' | xmllint -o /dev/null --schema http://schemas.opengis.net/wms/1.3.0/capabilities_1_3_0.xsd -; \
	fi
	@rm -f wms-access-log
	@if [ -e failures/wms_getcapabilities.get ]; then \
	   echo Validating failures/wms_getcapabilities.get; \
	   cat failures/wms_getcapabilities.get | sed '/<inspire_vs:ExtendedCapabilities>/,/<\/inspire_vs:ExtendedCapabilities>/d' | xmllint -o /dev/null --schema http://schemas.opengis.net/wms/1.3.0/capabilities_1_3_0.xsd -; \
	fi

geonames-database:
	@-$(MAKE) stop-geonames-db
	rm -rf tmp-geonames-db
	if ! /usr/share/smartmet/test/db/create-local-db.sh $(TEST_DB_DIR) >tmp-geonames-db.log 2>&1 ; then \
	    cat tmp-geonames-db.log; \
	    false; \
	fi

start-geonames-db: geonames-database
	/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) start -w

stop-geonames-db:
	-/usr/share/smartmet/test/db/test-db-ctl.sh $(TEST_DB_DIR) stop

dummy:

cnf/authentication.conf cnf/geonames.conf cnf/gis.conf:
	$(GEONAMES_HOST_EDIT) $@.in >$@

templates:
	$(MAKE) -C .. $@

$(PROG) : % : %.cpp Makefile ../wms.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)

.PHONY: cnf/authentication.conf cnf/geonames.conf cnf/gis.conf dummy
