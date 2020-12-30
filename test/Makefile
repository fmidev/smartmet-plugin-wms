PROG = PluginTest

REQUIRES = gdal jsoncpp configpp ctpp2 fmt

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -DUNIX -O0 -g $(FLAGS) -Wno-unknown-pragmas

INCLUDES += \
	-I../wms

LIBS += $(REQUIRED_LIBS) \
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

all: $(PROG)
clean:
	rm -f $(PROG) *~

ifdef CI
test: $(PROG) fminames
	@sed -i -e 's|"../tmpl"|"/etc/smartmet/plugins/wms/tmpl"|g' cnf/wms.conf
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
	sed -i -e 's/"smartmet-test"/"localhost"/g' cnf/*.conf
	@/usr/share/smartmet/test/db/init-and-start.sh && /usr/share/smartmet/test/db/install-test-db.sh > /dev/null

$(PROG) : % : %.cpp Makefile ../wms.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
