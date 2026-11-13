CXX 	  := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I. -I/usr/local/include
PODOFOLIB := -L/usr/local/lib -lpodofo


# PYTHON_FLAGS := $(shell python3-config --includes)
# CXXFLAGS += $(PYTHON_FLAGS)

LDFLAGS := $(PODOFOLIB)

BUILD_DIR := build
SRCS := $(wildcard *.cpp) $(wildcard utils/*.cpp)
OBJS := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRCS))
TARGET := $(BUILD_DIR)/butler

all: $(TARGET)
	@echo "Build complete: $(TARGET)"

# Link object files into the final executable
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@) 
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	#$(CXX) $(OBJS) -o $@ -lpython3.12 -ldl -lm
	

# Compile source files into object files inside build/
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
