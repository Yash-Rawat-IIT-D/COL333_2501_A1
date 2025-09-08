# Empty Makefile
all:
	@echo "Nothing to do"
CXX = g++
CXXFLAGS = -std=c++17 -O2 -I src

SRC = src/main.cpp src/io_handler.cpp src/solver.cpp
FORMAT_SRC = src/format_checker.cpp src/io_handler.cpp

MAIN = main
FORMAT = format_checker

all: $(MAIN) $(FORMAT)

$(MAIN): $(SRC) src/structures.h src/solver.h src/io_handler.h
	$(CXX) $(CXXFLAGS) $(SRC) -o $(MAIN)

$(FORMAT): $(FORMAT_SRC) src/structures.h src/io_handler.h
	$(CXX) $(CXXFLAGS) $(FORMAT_SRC) -o $(FORMAT)

checker: $(FORMAT)
	
clean:
	rm -f $(MAIN) $(FORMAT)
