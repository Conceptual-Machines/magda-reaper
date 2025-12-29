#!/bin/bash
# Run MAGDA unit tests locally
# These tests don't require REAPER

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNIT_DIR="$SCRIPT_DIR/unit"

echo "=== MAGDA Unit Tests ==="
echo ""

# Configure and build
echo "Configuring unit tests..."
cd "$UNIT_DIR"
cmake -B build . 2>&1

echo ""
echo "Building unit tests..."
cmake --build build --parallel 2>&1

echo ""
echo "Running tests..."
echo ""

# Run tests with verbose output
ctest --test-dir build --output-on-failure --verbose

echo ""
echo "=== All unit tests passed! ==="
