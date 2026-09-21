PROG = PluginTest

REQUIRES = gdal jsoncpp configpp ctpp2 fmt librsvg libpqxx

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -DUNIX -O0 -g $(FLAGS) -Wno-unknown-pragmas -DWITHOUT_OSM

INCLUDES += \
	-I../wms \
	-I/usr/include/smartmet \
	-isystem /usr/include/mysql \
	-isystem /usr/include/oracle/11.2/client64 \
	-isystem /usr/include/soci

LIBS += 

LIBS += $(PREFIX_LDFLAGS) \
	$(REQUIRED_LIBS) \
	-lsmartmet-grid-files \
	-lsmartmet-spine  \
	-lsmartmet-newbase \
	-lsmartmet-macgyver \
	-lsmartmet-giza \
	-lboost_thread \
	-lboost_filesystem \
	-lboost_regex \
	-lboost_iostreams \
	-lboost_serialization \
	-lbz2 -lz -lpthread -ldl

# The satellite images come from the smartmet-test-data package
SATELLITE_TEST_DATA ?= /usr/share/smartmet/test/data/satellite
export SATELLITE_TEST_DATA

# Test input groups (resolved at parse time so filter-out works correctly)
ALL_TESTS   := $(wildcard input/*.get)
WMS_TESTS   := $(wildcard input/wms_*.get)
WMTS_TESTS  := $(wildcard input/wmts_*.get)
TILES_TESTS := $(wildcard input/tiles_*.get)
DALI_TESTS  := $(filter-out $(WMS_TESTS) $(WMTS_TESTS) $(TILES_TESTS), $(ALL_TESTS))

TEST_PREPARE_TARGETS := \
	templates \
	cnf/authentication.conf \
	cnf/geonames.conf \
	cnf/gis.conf \
	cnf/avi.conf \
	cnf/observation.conf \
	start-redis-db

PLUGIN_TEST_SRC := PluginTest.cpp
PLUGIN_TEST_OBJS := $(patsubst %.cpp,obj/%.o,$(PLUGIN_TEST_SRC))

TEST_FINISH_TARGETS := stop-redis-db

TEST_DB_DIR := $(shell pwd)/tmp-geonames-db

ifdef CI
GEONAMES_HOST_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
OBSERVATION_CONF_EDIT := sed -e 's|"@IOT_OBS@"|"$(TEST_DB_DIR)"|g'
AVI_CONF_EDIT := sed -e 's|"smartmet-test"|"$(TEST_DB_DIR)"|g'
TEST_PREPARE_TARGETS += geonames-database start-geonames-db
TEST_FINISH_TARGETS += stop-geonames-db
else
GEONAMES_HOST_EDIT := cat
OBSERVATION_CONF_EDIT := sed -e 's|"@IOT_OBS@"|"smartmet-test"|g'
AVI_CONF_EDIT := cat
endif

all: $(PROG)

clean:	clean-redis
	rm -f $(PROG) *~
	rm -f cnf/authentication.conf cnf/geonames.conf cnf/gis.conf cnf/observation.conf
	-if [ -f tmp-geonames-db/postmaster.pid ] ; then $(MAKE) stop-geonames-db; fi
	rm -rf tmp-geonames-db* obj failures

test: $(PROG) $(TEST_PREPARE_TARGETS)
	@rm -f failures/*
	@if ./TestRunner.sh; then \
		ok=true; $(MAKE) $(TEST_FINISH_TARGETS); \
	else \
		ok=false; $(MAKE) $(TEST_FINISH_TARGETS); \
	fi; \
	rm -f wms-access-log; \
	$(MAKE) validate-xml-schemas validate-tiles-schemas && $$ok

test-dali: $(PROG) $(TEST_PREPARE_TARGETS)
	@rm -f failures/*
	@if [ -z "$(DALI_TESTS)" ]; then \
		echo "No Dali tests found."; $(MAKE) $(TEST_FINISH_TARGETS); \
	elif ./TestRunner.sh $(DALI_TESTS); then \
		$(MAKE) $(TEST_FINISH_TARGETS); \
	else \
		$(MAKE) $(TEST_FINISH_TARGETS); false; \
	fi

test-wms: $(PROG) $(TEST_PREPARE_TARGETS)
	@rm -f failures/*
	@if [ -z "$(WMS_TESTS)" ]; then \
		echo "No WMS tests found."; $(MAKE) $(TEST_FINISH_TARGETS); \
	elif ./TestRunner.sh $(WMS_TESTS); then \
		ok=true; $(MAKE) $(TEST_FINISH_TARGETS); \
	else \
		ok=false; $(MAKE) $(TEST_FINISH_TARGETS); \
	fi; \
	rm -f wms-access-log; \
	$(MAKE) validate-xml-schemas && $${ok:-true}

test-wmts: $(PROG) $(TEST_PREPARE_TARGETS)
	@rm -f failures/*
	@if [ -z "$(WMTS_TESTS)" ]; then \
		echo "No WMTS tests found."; $(MAKE) $(TEST_FINISH_TARGETS); \
	elif ./TestRunner.sh $(WMTS_TESTS); then \
		ok=true; $(MAKE) $(TEST_FINISH_TARGETS); \
	else \
		ok=false; $(MAKE) $(TEST_FINISH_TARGETS); \
	fi; \
	$(MAKE) validate-xml-schemas && $$ok

test-tiles: $(PROG) $(TEST_PREPARE_TARGETS)
	@rm -f failures/*
	@if [ -z "$(TILES_TESTS)" ]; then \
		echo "No OGC Tiles tests found."; $(MAKE) $(TEST_FINISH_TARGETS); \
	elif ./TestRunner.sh $(TILES_TESTS); then \
		ok=true; $(MAKE) $(TEST_FINISH_TARGETS); \
	else \
		ok=false; $(MAKE) $(TEST_FINISH_TARGETS); \
	fi; \
	$(MAKE) validate-tiles-schemas && $$ok

# WMS and WMTS GetCapabilities are validated against the OGC XML schemas
# vendored in schemas/xsd (see the README there). The XML catalog maps the
# schemaLocation URLs to the local copies, so no network access is needed.
# Both the expected outputs and any actual outputs in failures/ are checked;
# a schema violation fails the target. INSPIRE ExtendedCapabilities are
# stripped from the WMS document since its schema is not vendored.
XSD_DIR := schemas/xsd
XSD_WMS := $(XSD_DIR)/wms/1.3.0/capabilities_1_3_0.xsd
XSD_WMTS := $(XSD_DIR)/wmts/1.0/wmtsGetCapabilities_response.xsd
# $(call validate_xsd,file,schema,filter-command)
define validate_xsd
if [ -e $(1) ]; then \
   if $(3) $(1) | XML_CATALOG_FILES=$(XSD_DIR)/catalog.xml xmllint --noout --nonet --schema $(2) - > xmllint.log 2>&1; then \
      echo "$(1): OK ($(notdir $(2)))"; \
   else \
      echo "$(1): FAIL: does not validate against $(2)"; \
      grep -v 'Skipping import\|I/O warning' xmllint.log | head -20; status=1; \
   fi; rm -f xmllint.log; \
fi;
endef
STRIP_INSPIRE := sed '/<inspire_vs:ExtendedCapabilities>/,/<\/inspire_vs:ExtendedCapabilities>/d'
validate-xml-schemas:
	@status=0; \
	$(call validate_xsd,output/wms_getcapabilities.get,$(XSD_WMS),$(STRIP_INSPIRE)) \
	$(call validate_xsd,failures/wms_getcapabilities.get,$(XSD_WMS),$(STRIP_INSPIRE)) \
	$(call validate_xsd,output/wmts_getcapabilities.get,$(XSD_WMTS),cat) \
	$(call validate_xsd,failures/wmts_getcapabilities.get,$(XSD_WMTS),cat) \
	exit $$status

# OGC API - Tiles JSON responses are validated against the OGC API Common Part 2 schemas in
# schemas/ogcapi (see the README there). Both the expected outputs and any
# actual outputs left in failures/ are checked, like the WMS GetCapabilities
# XML above. Add a line per response type as more schemas are vendored.
OGC_SCHEMAS := schemas/ogcapi
validate-tiles-schemas:
	@status=0; \
	files="$(wildcard output/tiles_getcollections.get failures/tiles_getcollections.get)"; \
	if [ -n "$$files" ]; then \
	   ./ValidateJsonSchema.py $(OGC_SCHEMAS)/common-geodata/collections-UAD.yaml $$files || status=1; \
	fi; \
	files="$(wildcard output/tiles_getcollection_*.get failures/tiles_getcollection_*.get)"; \
	if [ -n "$$files" ]; then \
	   ./ValidateJsonSchema.py $(OGC_SCHEMAS)/common-geodata/collectionDesc-UAD.yaml $$files || status=1; \
	fi; \
	exit $$status

# Copy actual test outputs from failures/ to output/ to accept them as new expected outputs.
# Workflow: run the tests, inspect failures/, then run this target to accept correct results.
update-outputs:
	@if ls failures/*.get 2>/dev/null | grep -q .; then \
		for f in failures/*.get; do \
			cp "$$f" "output/$$(basename $$f)"; \
			echo "Accepted output/$$(basename $$f)"; \
		done; \
	else \
		echo "No failures to accept."; \
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

start-redis-db:
	@rm -rf cnf/grid
	@cp -pr $(datadir)/smartmet/test/grid cnf/
	@echo "*** Creating the Redis configuration file."
	@smartmet-grid-test-config-creator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf

stop-redis-db:
	@if [ -f cnf/grid/redis/redis-server.pid ] ; then \
		cat cnf/grid/redis/redis-server.pid | xargs kill -9; \
		rm -f cnf/grid/redis/redis-server.pid; \
	fi

clean-redis:
	rm -rf cnf/grid

dummy:

cnf/authentication.conf cnf/geonames.conf cnf/gis.conf:
	$(GEONAMES_HOST_EDIT) $@.in >$@

cnf/observation.conf: cnf/observation.conf.in
	$(OBSERVATION_CONF_EDIT) $@.in >$@

cnf/avi.conf: cnf/avi.conf.in
	$(AVI_CONF_EDIT) $@.in >$@

templates:
	$(MAKE) -C .. $@

PluginTest : $(PLUGIN_TEST_OBJS)
	$(CXX) $(CFLAGS) $(PLUGIN_TEST_OBJS) -o $@ $(LIBS)

obj/%.o: %.cpp objdir
	$(CXX) -c $(CFLAGS) -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $< $(INCLUDES) $(LIBS)

objdir:
	mkdir -p $(objdir)

.PHONY: cnf/authentication.conf cnf/geonames.conf cnf/gis.conf cnf/avi.conf cnf/observation.conf dummy \
        test test-dali test-wms test-wmts test-tiles update-outputs

-include $(wildcard obj/*.d)
