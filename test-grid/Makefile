PROG = PluginTest TestConfigCreator

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

LIBS += -lsmartmet-grid-files \
	-lsmartmet-spine  \
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
	rm -f $(PROG) *~
	rm -rf failures

test: $(PROG) $(CONFIG_FILES)
	@mkdir -p failures
	@rm -f failures/*
	@echo 
	@echo "*************************************************************"
	@echo "****************** Running wms grid tests *******************"
	@echo "*************************************************************"
	@echo 
	@echo "*** Creating the Redis configuration file."
	@./TestConfigCreator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf
	@echo "*** Creating the WMS configuration file."
	@./TestConfigCreator cnf/environment.conf cnf/wms-template.conf cnf/wms.conf
	@echo "*** Executing plugin tests."
	@echo 
	@./PluginTest -c cnf/reactor.conf
	@rm -f cnf/grid/redis/redis.conf
#	@echo 
#	@echo "*** Shutting down the Redis database and removing useless files." 
#	@echo 
#	@cat cnf/grid/redis/redis-server.pid | xargs kill -9
#	@rm -f cnf/grid/redis/redis-server.pid
#	@rm -f cnf/grid/redis/redis-server.log

$(PROG) : % : %.cpp Makefile ../gribwms.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
