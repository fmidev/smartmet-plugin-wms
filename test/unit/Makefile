PROGS = test_label_placement test_subdivide_gate

CXX      = g++
CXXFLAGS = -std=c++17 -O0 -g -Wall -Wextra \
           -DBOOST_TEST_DYN_LINK \
           -I../../wms \
           -I/usr/include/smartmet

LIBS     = -lboost_unit_test_framework

LABEL_SRCS = test_label_placement.cpp \
             ../../wms/LabelPlacement.cpp

SUBDIVIDE_SRCS = test_subdivide_gate.cpp

.PHONY: all test clean

all: $(PROGS)

test_label_placement: $(LABEL_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

test_subdivide_gate: $(SUBDIVIDE_SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) -lsmartmet-gis

test: $(PROGS)
	./test_label_placement --log_level=message
	./test_subdivide_gate --log_level=message

clean:
	rm -f $(PROGS)
