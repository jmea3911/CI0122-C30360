CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

all: papa

papa: papa.cc Buzon.cc Buzon.h
	$(CXX) $(CXXFLAGS) -o papa papa.cc Buzon.cc

clean:
	rm -f papa
	-ipcrm -Q 0xB54321 2>/dev/null
	-ipcrm -M 0xB54322 2>/dev/null
	-ipcrm -S 0xB54323 2>/dev/null

.PHONY: all clean
