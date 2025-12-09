#!/bin/bash
# MAGDA REAPER Integration Test Runner
# Runs REAPER in headless mode with test scripts

set -e

REAPER_BIN="/Applications/REAPER.app/Contents/MacOS/REAPER"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}MAGDA REAPER Integration Test Runner${NC}"
echo "=========================================="

# Check if REAPER exists
if [ ! -f "$REAPER_BIN" ]; then
  echo -e "${RED}Error: REAPER not found at $REAPER_BIN${NC}"
  echo "Please update REAPER_BIN in this script to point to your REAPER installation"
  exit 1
fi

# Check if plugin is built
PLUGIN_PATH="$HOME/Library/Application Support/REAPER/UserPlugins/reaper_magda.dylib"
if [ ! -f "$PLUGIN_PATH" ]; then
  echo -e "${YELLOW}Warning: Plugin not found at $PLUGIN_PATH${NC}"
  echo "Building plugin first..."
  cd "$PROJECT_DIR"
  make build
fi

# Create test project directory
TEST_DIR="$PROJECT_DIR/tests/test_projects"
mkdir -p "$TEST_DIR"

# Run tests
echo ""
echo "Running integration tests..."
echo ""

# Run test scripts in headless REAPER
# -nosplash: Don't show splash screen
# -new: Start with new project
# -close:save:exit: Save and exit after script completes

echo "Running basic REAPER API tests..."
echo "Note: REAPER window will open - this is normal on macOS (no true headless mode)"
echo "      Tests run automatically and REAPER will close when done"
echo ""
# Run REAPER with script - it will exit when script calls os.exit()
# Capture both stdout and stderr, and show output in real-time
"$REAPER_BIN" \
  -nosplash \
  -new \
  -ReaScript "$SCRIPT_DIR/integration_test.lua" \
  2>&1 | tee /tmp/reaper_test_basic.log

BASIC_TEST_EXIT=${PIPESTATUS[0]}

# Check test results from output
if grep -q "All tests passed" /tmp/reaper_test_basic.log 2>/dev/null; then
  echo -e "${GREEN}✓ Basic tests passed${NC}"
elif grep -q "Some tests failed" /tmp/reaper_test_basic.log 2>/dev/null; then
  echo -e "${RED}✗ Basic tests failed${NC}"
  grep -A 5 -B 5 "FAIL\|ERROR" /tmp/reaper_test_basic.log || cat /tmp/reaper_test_basic.log
  exit 1
else
  echo -e "${YELLOW}⚠ Could not determine test results${NC}"
  cat /tmp/reaper_test_basic.log
  exit 1
fi

echo ""
echo "Running MAGDA action tests..."
echo "Note: REAPER window will open - this is normal on macOS"
echo "      Tests run automatically and REAPER will close when done"
echo ""
"$REAPER_BIN" \
  -nosplash \
  -new \
  -ReaScript "$SCRIPT_DIR/magda_integration_test.lua" \
  2>&1 | tee /tmp/reaper_test_magda.log

MAGDA_TEST_EXIT=${PIPESTATUS[0]}

# Check test results from output
if grep -q "All tests passed" /tmp/reaper_test_magda.log 2>/dev/null; then
  echo -e "${GREEN}✓ MAGDA tests passed${NC}"
  EXIT_CODE=0
elif grep -q "Some tests failed\|ERROR.*plugin not loaded" /tmp/reaper_test_magda.log 2>/dev/null; then
  echo -e "${RED}✗ MAGDA tests failed${NC}"
  grep -A 5 -B 5 "FAIL\|ERROR" /tmp/reaper_test_magda.log || cat /tmp/reaper_test_magda.log
  EXIT_CODE=1
else
  echo -e "${YELLOW}⚠ Could not determine test results${NC}"
  cat /tmp/reaper_test_magda.log
  EXIT_CODE=1
fi

if [ $EXIT_CODE -eq 0 ]; then
  echo -e "${GREEN}✓ All tests passed!${NC}"
  exit 0
else
  echo -e "${RED}✗ Some tests failed (exit code: $EXIT_CODE)${NC}"
  exit $EXIT_CODE
fi
