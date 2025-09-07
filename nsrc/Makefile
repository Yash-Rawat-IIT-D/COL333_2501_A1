CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
SRCDIR = .
OBJDIR = ../object
TARGETDIR = ../target

# Source files
SOURCES = main.cpp utils.cpp solver.cpp
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)
TARGET = $(TARGETDIR)/nsolver

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(TARGETDIR)
	$(CXX) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGETDIR):
	mkdir -p $(TARGETDIR)

clean:
	rm -rf $(OBJDIR) $(TARGETDIR)

run: $(TARGET)
	$(TARGET) ../input/tc.txt ../output/output.txt

test: $(TARGET)
	$(TARGET) ../SampleInputOutput/input2.txt ../output/test_output2.txt
