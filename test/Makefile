PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

MAINFLAGS = -std=c++11 -Wall -W -Wno-unused-parameter -Wno-unknown-pragmas

EXTRAFLAGS = -Werror -pedantic -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Winline \
	-Wctor-dtor-privacy -Wnon-virtual-dtor -Wno-pmf-conversions \
	-Wsign-promo -Wchar-subscripts -Wold-style-cast \
	-Wshadow -Woverloaded-virtual

DIFFICULTFLAGS = -Weffc++ -Wunreachable-code -Wredundant-decls

CC = g++
CFLAGS = -DUNIX -O0 -g $(MAINFLAGS)

INCLUDES = \
	-I../include \
	-I/usr/include/smartmet \
	-I/usr/include/mysql \
	-I/usr/include/oracle/11.2/client64 \
	-I/usr/include/soci \
	`pkg-config --cflags libconfig++` \
	`pkg-config --cflags jsoncpp`

LIBS = 	-lsmartmet-spine  \
	-lsmartmet-newbase \
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
	-lbz2 -lz -lpthread -ldl

all: $(PROG)
clean:
	rm -f $(PROG) *~ Plugintest

test: $(PROG)
	@./TestRunner.sh
	@rm -f wms-access-log

$(PROG) : % : %.cpp Makefile ../wms.so
	$(CC) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
