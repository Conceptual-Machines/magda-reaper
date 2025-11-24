.PHONY: build clean install setup

# Default build directory
BUILD_DIR := build
CMAKE := cmake

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	PLATFORM := macOS
else ifeq ($(OS),Windows_NT)
	PLATFORM := Windows
else
	PLATFORM := Linux
endif

# Setup submodules and symlink
setup:
	@echo "Setting up Reaper SDK and WDL submodules..."
	git submodule update --init --recursive
	@if [ ! -e reaper-sdk/WDL ]; then \
		echo "Creating WDL symlink..."; \
		cd reaper-sdk && ln -s ../WDL WDL; \
	fi
	@echo "Setup complete!"

# Configure and build
build: setup
	@echo "Building MAGDA Reaper extension for $(PLATFORM)..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) ..
	$(CMAKE) --build $(BUILD_DIR)
	@echo "Build complete!"

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete!"

# Rebuild from scratch
rebuild: clean build

# Show help
help:
	@echo "MAGDA Reaper Extension - Build Commands"
	@echo ""
	@echo "  make setup    - Initialize submodules and create WDL symlink"
	@echo "  make build    - Build the extension (runs setup if needed)"
	@echo "  make clean    - Remove build directory"
	@echo "  make rebuild  - Clean and rebuild"
	@echo "  make help     - Show this help message"
	@echo ""

