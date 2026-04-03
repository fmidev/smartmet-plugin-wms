PROG = test_label_placement

CXX      = g++
CXXFLAGS = -std=c++17 -O0 -g -Wall -Wextra \
           -DBOOST_TEST_DYN_LINK \
           -I../../wms \
           -I/usr/include/smartmet

LIBS     = -lboost_unit_test_framework

SRCS = test_label_placement.cpp \
       ../../wms/LabelPlacement.cpp

.PHONY: all test clean

all: $(PROG)

$(PROG): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

test: $(PROG)
	./$(PROG) --log_level=message

clean:
	rm -f $(PROG)
