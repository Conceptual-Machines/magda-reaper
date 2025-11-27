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
	@git submodule update --init --recursive 2>&1 | grep -v "fatal: No url found" || true
	@if [ ! -e reaper-sdk/WDL ]; then \
		echo "Creating WDL symlink..."; \
		cd reaper-sdk && ln -s ../WDL/WDL WDL; \
	fi
	@echo "Setup complete!"

# Configure and build
build: setup
	@echo "Closing Reaper (if running)..."
	@-killall REAPER 2>/dev/null || true
	@sleep 1
	@echo "Building MAGDA Reaper extension for $(PLATFORM)..."
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) ..
	$(CMAKE) --build $(BUILD_DIR)
	@echo "Build complete!"
	@echo "Opening Reaper..."
	@open -a REAPER

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Clean complete!"

# Rebuild from scratch
rebuild: clean build

# Format code using clang-format
format:
	@echo "Formatting code with clang-format..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i; \
		echo "Formatting complete!"; \
	else \
		echo "Error: clang-format not found. Install with: brew install clang-format"; \
		exit 1; \
	fi

# Check code formatting (dry run)
format-check:
	@echo "Checking code formatting..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find src include -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror; \
	else \
		echo "Error: clang-format not found. Install with: brew install clang-format"; \
		exit 1; \
	fi

# Show help
help:
	@echo "MAGDA Reaper Extension - Build Commands"
	@echo ""
	@echo "  make setup       - Initialize submodules and create WDL symlink"
	@echo "  make build       - Build the extension (runs setup if needed)"
	@echo "  make clean       - Remove build directory"
	@echo "  make rebuild     - Clean and rebuild"
	@echo "  make format      - Format code with clang-format"
	@echo "  make format-check - Check code formatting (dry run)"
	@echo "  make help        - Show this help message"
	@echo ""
