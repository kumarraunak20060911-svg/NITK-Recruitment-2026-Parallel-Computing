CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Iinclude
BUILD_DIR := build

.PHONY: all clean

all: $(BUILD_DIR)
	@echo "Project scaffold created. Add implementation targets as the algorithms are written."

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
