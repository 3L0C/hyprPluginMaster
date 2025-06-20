#!/usr/bin/env bash

# Hyprland Master Layout vs Plugin Master Layout Comparison Test
# This script tests both layouts with identical scenarios and compares results using diff

set -e

source ./hypr_plugin_testing_framework.sh
TEST_DIR="hypr_layout_test"
PLUGIN_PATH="$HOME/.config/hypr/plugins/masterLayoutPlugin.so"

# Create test directory
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"

echo "=== Hyprland Master Layout Comparison Test ==="
echo "Started at: $(date)"
echo "Test directory: $TEST_DIR"
echo ""

# Test prerequisite: Load plugin first
echo "=== PREREQUISITE: Loading Plugin ==="
if ! hypr_plugin_load; then
    exit 1
fi

echo ""

# Phase 0: Configuration Comparison
echo "=== PHASE 0: Configuration Comparison ==="
compare_layout_configs
if [ $? -eq 1 ]; then
    echo "❌ Configuration differences detected between layouts"
    echo "Please resolve these before testing..."
    exit 1
else
    echo "✅ Configuration verification passed"
fi

# Phase 1: Standard Master Layout
echo "=== PHASE 1: Standard Master Layout Testing ==="
ensure_test_workspace
clear_workspace

echo "Setting layout to master..."
hyprctl -i $INSTANCE keyword general:layout master >/dev/null

spawn_test_clients
run_test_scenarios "STANDARD" "master_results.log"

# Phase 2: Plugin Master Layout
echo ""
echo "=== PHASE 2: Plugin Master Layout Testing ==="
ensure_test_workspace
clear_workspace

echo "Setting layout to pluginmaster..."
hyprctl -i $INSTANCE keyword general:layout pluginmaster >/dev/null

spawn_test_clients
run_test_scenarios "PLUGIN" "plugin_results.log"

# Phase 3: Comparison using diff
echo ""
echo "=== PHASE 3: Comparison using diff ==="

echo "Comparing filtered results..."
if diff -u master_results.log plugin_results.log > comparison_diff.txt; then
    echo "✅ SUCCESS: No differences found between layouts!"
    echo "hyprMasterLayout behaves identically to standard MasterLayout"
    exit_code=0
else
    echo "❌ DIFFERENCES FOUND: Layouts behave differently"
    echo "See comparison_diff.txt for details"
    exit_code=1
fi

# Final cleanup: Move any remaining test windows to workspace 2
echo "Performing final cleanup..."
clear_workspace

echo ""
echo "=== TEST COMPLETE ==="
echo "Results saved in: $TEST_DIR/"
echo "- master_results.log: Standard layout test results (filtered)"
echo "- plugin_results.log: Plugin layout test results (filtered)"
echo "- master_results_raw.log: Standard layout raw output"
echo "- plugin_results_raw.log: Plugin layout raw output"
echo "- comparison_diff.txt: Diff comparison results"
echo ""

if [ -f comparison_diff.txt ] && [ -s comparison_diff.txt ]; then
    echo "=== DIFF SUMMARY (first 50 lines) ==="
    head -50 comparison_diff.txt
    echo ""
    echo "Full diff saved to: comparison_diff.txt"
else
    echo "No differences detected in filtered output."
fi

echo ""
echo "Completed at: $(date)"
echo "Exit code: $exit_code"

exit $exit_code
