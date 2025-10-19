# Compiler and Flags
CXX = g++
# Added -pthread for multi-threading support
CXXFLAGS = -std=c++11 -O3 -Wall

# Executable name
TARGET = floorplanner

# Source files
SRC = 110502529_PA4.cpp

# Phony targets
.PHONY: all run clean

# Default target
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# Command to run the program
# Usage: make run input=path/to/input.block output=path/to/output.out
run: $(TARGET)
	@./$(TARGET) -i $(input) -o $(output)

# Command to clean up the directory
clean:
	@rm -f $(TARGET) *.o
	@rm -f *.output