# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -O2

# Target executable name
TARGET := lz-maxocc

# Source files
SRCS := lz-maxocc.cpp lzhb-decode.cpp libsais64.c libsais.c

# Object files (replace .cpp with .o)
OBJS := $(SRCS:.cpp=.o)

# Default rule: build the target
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

# Rule to compile each .cpp into .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the program after building
.PHONY: run
run: $(TARGET)
	./$(TARGET) $(ARGS)

# Clean up build artifacts
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)