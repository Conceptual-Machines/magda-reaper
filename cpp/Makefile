# Makefile for MAGDA C++

BUILD_DIR=build

all: build

build:
	cmake -S . -B $(BUILD_DIR) -G Ninja
	cmake --build $(BUILD_DIR)

test:
	cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all
