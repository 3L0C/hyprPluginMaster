#!/usr/bin/env bash

export INSTANCE=1
export PLUGIN_PATH

hypr_plugin_load() {
    echo "MANUAL ACTION REQUIRED:"
    echo "Running: hyprctl -i $INSTANCE plugin load $PLUGIN_PATH"
    hyprctl -i $INSTANCE plugin load "$PLUGIN_PATH"
    echo "Click 'Allow' on the popup dialog, then press ENTER here to continue..."
    read -rp "Press ENTER when plugin is loaded: " RESULT

    [[ -n "$RESULT" ]] && return 1

    # Verify plugin is loaded
    echo "Verifying plugin load..."
    if hyprctl -i $INSTANCE layouts | grep -q "pluginmaster"; then
        echo "✅ Plugin loaded successfully"
    else
        echo "❌ Plugin not found in available layouts"
        echo "Available layouts:"
        hyprctl -i $INSTANCE layouts
        return 1
    fi
}

# Function to compare layout configuration settings
compare_layout_configs() {
    echo "=== Comparing Layout Configurations ==="

    # List of all master layout configuration options
    local config_options=(
        "allow_small_split"
        "special_scale_factor"
        "mfact"
        "new_status"
        "new_on_top"
        "new_on_active"
        "orientation"
        "inherit_fullscreen"
        "slave_count_for_center_master"
        "center_master_fallback"
        "smart_resizing"
        "drop_at_cursor"
        "always_keep_position"
    )

    local differences_found=0
    local master_config_file="master_config.txt"
    local plugin_config_file="plugin_config.txt"
    local config_diff_file="config_diff.txt"

    echo "Collecting master layout configuration..."
    printf '\n' > "$master_config_file"  # Clear file
    for option in "${config_options[@]}"; do
        echo "=== $option ===" >> "$master_config_file"
        hyprctl -i $INSTANCE getoption "master:$option" 2>/dev/null >> "$master_config_file" || echo "Error getting master:$option" >> "$master_config_file"
        echo "" >> "$master_config_file"
    done

    echo "Collecting plugin layout configuration..."
    printf '\n' > "$plugin_config_file"  # Clear file
    for option in "${config_options[@]}"; do
        echo "=== $option ===" >> "$plugin_config_file"
        hyprctl -i $INSTANCE getoption "plugin:pluginmaster:$option" 2>/dev/null >> "$plugin_config_file" || echo "Error getting plugin:pluginmaster:$option" >> "$plugin_config_file"
        echo "" >> "$plugin_config_file"
    done

    echo "Comparing configurations..."
    if diff -u "$master_config_file" "$plugin_config_file" > "$config_diff_file"; then
        echo "✅ Configuration comparison: All settings match"
        rm -f "$config_diff_file"  # Clean up empty diff file

        echo ""
        echo "Configuration files saved:"
        echo "- $master_config_file: Master layout configuration"
        echo "- $plugin_config_file: Plugin layout configuration"
        if [ -f "$config_diff_file" ]; then
            echo "- $config_diff_file: Configuration differences"
        fi

        return
    else
        echo "❌ Configuration comparison: Differences found"

        echo ""
        echo "=== Configuration Differences ==="
        cat "$config_diff_file"
        echo ""

        # Also show individual option comparison for clarity
        echo "=== Individual Option Comparison ==="
        for option in "${config_options[@]}"; do
            local master_value=$(hyprctl -i $INSTANCE getoption "master:$option" 2>/dev/null | grep -E "(int:|float:|str:|set to)" | head -1 || echo "ERROR")
            local plugin_value=$(hyprctl -i $INSTANCE getoption "plugin:pluginmaster:$option" 2>/dev/null | grep -E "(int:|float:|str:|set to)" | head -1 || echo "ERROR")

            if [ "$master_value" != "$plugin_value" ]; then
                echo "❌ $option:"
                echo "  Master:  $master_value"
                echo "  Plugin:  $plugin_value"
            else
                echo "✅ $option: $master_value"
            fi
        done

        echo ""
        echo "Configuration files saved:"
        echo "- $master_config_file: Master layout configuration"
        echo "- $plugin_config_file: Plugin layout configuration"
        if [ -f "$config_diff_file" ]; then
            echo "- $config_diff_file: Configuration differences"
        fi

        return 1
    fi
}

# Filtering function to remove variable fields that should differ between runs
filter_json() {
    local input="$1"
    echo "$input" | sed \
        -e 's/"address": "[^"]*"/"address": "FILTERED"/g' \
        -e 's/"pid": [0-9]*/"pid": 0/g' \
        -e 's/"focusHistoryID": [0-9]*/"focusHistoryID": 0/g' \
        -e 's/"lastwindow": "[^"]*"/"lastwindow": "FILTERED"/g' \
        -e 's/"swallowing": "[^"]*"/"swallowing": "FILTERED"/g'
}

# Logging functions
log_filtered_command() {
    local phase="$1"
    local test="$2"
    local cmd="$3"
    local file="$4"

    echo "[$phase] $test: $cmd" | tee -a "${file}_raw.log"
    local output=$(eval "$cmd" 2>&1 | sed '/Layout/d')
    echo "$output" | tee -a "${file}_raw.log"

    # Apply filtering and save to filtered file
    echo "[______] $test: $cmd" >> "$file"
    filter_json "$output" >> "$file"
    echo "" >> "$file"
}

capture_filtered_state() {
    local phase="$1"
    local test="$2"
    local file="$3"

    echo "=== State Capture: $test ===" >> "$file"
    log_filtered_command "$phase" "$test" "hyprctl -i $INSTANCE clients -j" "$file"
    log_filtered_command "$phase" "$test" "hyprctl -i $INSTANCE activewindow -j" "$file"
    log_filtered_command "$phase" "$test" "hyprctl -i $INSTANCE activeworkspace -j" "$file"

    # Get layout-specific properties
    echo "[______] $test: Layout properties" >> "$file"
    local layout="$(hyprctl -i $INSTANCE getoption general:layout | sed '/set/d;s/str: //')"
    # echo "Layout: $layout" >> "$file"

    # Try to get mfact value (will work for both layouts)
    local properties=(
        orientation
        mfact
        new_status
        new_on_top
        new_on_active
        inherit_fullscreen
        special_scale_factor
        smart_resizing
        drop_at_cursor
        allow_small_split
        always_keep_position
        slave_count_for_center_master
        center_master_fallback
        center_ignores_reserved
    )

    for prop in "${properties[@]}"; do
        if echo "$layout" | grep -q "master"; then
            echo "$prop: $(hyprctl -i $INSTANCE getoption master:$prop | sed '/set/d')" >> "$file"
        elif echo "$layout" | grep -q "pluginmaster"; then
            echo "$prop: $(hyprctl -i $INSTANCE getoption plugin:pluginmaster:$prop | sed '/set/d')" >> "$file"
        fi
        echo "" >> "$file"
    done

    # local mfact=""
    # if echo "$layout" | grep -q "master"; then
    #     mfact=$(hyprctl -i $INSTANCE getoption master:mfact 2>/dev/null | grep -E "float:" || echo "mfact: unknown")
    # elif echo "$layout" | grep -q "pluginmaster"; then
    #     mfact=$(hyprctl -i $INSTANCE getoption plugin:pluginmaster:mfact 2>/dev/null | grep -E "float:" || echo "mfact: unknown")
    # fi
    # echo "mfact: $mfact" >> "$file"
    # echo "" >> "$file"
}

ensure_test_workspace() {
    echo "Ensuring we're on workspace 10..."
    hyprctl -i $INSTANCE dispatch workspace 10 >/dev/null 2>&1 || true
    sleep 0.5
}

wait_settle() {
    echo "Waiting for windows to settle..."
    sleep 2
}

clear_workspace() {
    echo "Clearing workspace..."

    # Kill all clients in the test instance
    local windows=$(hyprctl -i $INSTANCE clients -j | jq -r '.[].address' 2>/dev/null || echo "")
    for window in $windows; do
        if [ -n "$window" ] && [ "$window" != "null" ]; then
            hyprctl -i $INSTANCE dispatch killwindow "address:$window" >/dev/null 2>&1 || true
        fi
    done

    wait_settle

    # Verify clean state
    local remaining=$(hyprctl -i $INSTANCE clients -j | jq length 2>/dev/null || echo "0")
    if [ "$remaining" != "0" ]; then
        echo "Warning: $remaining windows still remain after cleanup"
    fi
    echo "Workspace cleared"
}

spawn_test_clients() {
    echo "Spawning test clients..."

    # Spawn consistent test clients (keep neofetch running)
    hyprctl -i $INSTANCE dispatch exec kitty >/dev/null
    sleep 0.5
    hyprctl -i $INSTANCE dispatch exec 'kitty htop' >/dev/null
    sleep 0.5
    hyprctl -i $INSTANCE dispatch exec 'kitty top' >/dev/null
    sleep 0.5
    hyprctl -i $INSTANCE dispatch exec 'kitty sh -c "neofetch; read -p \"Press enter to close: \""' >/dev/null
    sleep 0.5
    hyprctl -i $INSTANCE dispatch exec 'kitty vim' >/dev/null

    wait_settle

    # Verify we have the expected number of clients
    local client_count=$(hyprctl -i $INSTANCE clients -j | jq length 2>/dev/null || echo "0")
    echo "Spawned $client_count clients"

    if [ "$client_count" != "5" ]; then
        echo "Warning: Expected 5 clients, got $client_count"
    fi
}

run_test_scenarios() {
    local phase="$1"
    local results_file="$2"

    if [ -f "$results_file" ]; then
        rm "$results_file"
    fi

    echo "=== Running Test Scenarios for ______ ===" | tee -a "$results_file"

    # Test 1: Orientation Changes
    echo "--- Test 1: Orientation Changes ---" >> "$results_file"
    capture_filtered_state "$phase" "Initial State" "$results_file"

    log_filtered_command "$phase" "Orient Bottom" "hyprctl -i $INSTANCE dispatch layoutmsg orientationbottom" "$results_file"
    capture_filtered_state "$phase" "After Orient Bottom" "$results_file"

    log_filtered_command "$phase" "Orient Top" "hyprctl -i $INSTANCE dispatch layoutmsg orientationtop" "$results_file"
    capture_filtered_state "$phase" "After Orient Top" "$results_file"

    log_filtered_command "$phase" "Orient Left" "hyprctl -i $INSTANCE dispatch layoutmsg orientationleft" "$results_file"
    capture_filtered_state "$phase" "After Orient Left" "$results_file"

    log_filtered_command "$phase" "Orient Right" "hyprctl -i $INSTANCE dispatch layoutmsg orientationright" "$results_file"
    capture_filtered_state "$phase" "After Orient Right" "$results_file"

    # Test 2: mfact Adjustments
    echo "--- Test 2: mfact Adjustments ---" >> "$results_file"
    log_filtered_command "$phase" "Set mfact 0.5" "hyprctl -i $INSTANCE dispatch layoutmsg 'mfact exact 0.5'" "$results_file"
    capture_filtered_state "$phase" "After mfact 0.5" "$results_file"

    log_filtered_command "$phase" "Set mfact 0.7" "hyprctl -i $INSTANCE dispatch layoutmsg 'mfact exact 0.7'" "$results_file"
    capture_filtered_state "$phase" "After mfact 0.7" "$results_file"

    log_filtered_command "$phase" "Set mfact 0.3" "hyprctl -i $INSTANCE dispatch layoutmsg 'mfact exact 0.3'" "$results_file"
    capture_filtered_state "$phase" "After mfact 0.3" "$results_file"

    log_filtered_command "$phase" "Reset mfact 0.55" "hyprctl -i $INSTANCE dispatch layoutmsg 'mfact exact 0.55'" "$results_file"
    capture_filtered_state "$phase" "After mfact Reset" "$results_file"

    # Test 3: Master Count Changes
    echo "--- Test 3: Master Count Changes ---" >> "$results_file"
    log_filtered_command "$phase" "Add Master" "hyprctl -i $INSTANCE dispatch layoutmsg addmaster" "$results_file"
    capture_filtered_state "$phase" "After Add Master" "$results_file"

    log_filtered_command "$phase" "Add Master Again" "hyprctl -i $INSTANCE dispatch layoutmsg addmaster" "$results_file"
    capture_filtered_state "$phase" "After Add Master x2" "$results_file"

    log_filtered_command "$phase" "Remove Master" "hyprctl -i $INSTANCE dispatch layoutmsg removemaster" "$results_file"
    capture_filtered_state "$phase" "After Remove Master" "$results_file"

    log_filtered_command "$phase" "Remove Master Again" "hyprctl -i $INSTANCE dispatch layoutmsg removemaster" "$results_file"
    capture_filtered_state "$phase" "After Remove Master x2" "$results_file"

    # Test 4: Window Focus/Cycling
    echo "--- Test 4: Window Focus/Cycling ---" >> "$results_file"
    log_filtered_command "$phase" "Focus Master" "hyprctl -i $INSTANCE dispatch layoutmsg focusmaster" "$results_file"
    capture_filtered_state "$phase" "After Focus Master" "$results_file"

    log_filtered_command "$phase" "Cycle Next" "hyprctl -i $INSTANCE dispatch layoutmsg cyclenext" "$results_file"
    capture_filtered_state "$phase" "After Cycle Next 1" "$results_file"

    log_filtered_command "$phase" "Cycle Next" "hyprctl -i $INSTANCE dispatch layoutmsg cyclenext" "$results_file"
    capture_filtered_state "$phase" "After Cycle Next 2" "$results_file"

    log_filtered_command "$phase" "Cycle Prev" "hyprctl -i $INSTANCE dispatch layoutmsg cycleprev" "$results_file"
    capture_filtered_state "$phase" "After Cycle Prev" "$results_file"

    # Test 5: Window Swapping
    echo "--- Test 5: Window Swapping ---" >> "$results_file"
    log_filtered_command "$phase" "Swap with Master" "hyprctl -i $INSTANCE dispatch layoutmsg swapwithmaster" "$results_file"
    capture_filtered_state "$phase" "After Swap with Master" "$results_file"

    log_filtered_command "$phase" "Swap Next" "hyprctl -i $INSTANCE dispatch layoutmsg swapnext" "$results_file"
    capture_filtered_state "$phase" "After Swap Next" "$results_file"

    log_filtered_command "$phase" "Swap Prev" "hyprctl -i $INSTANCE dispatch layoutmsg swapprev" "$results_file"
    capture_filtered_state "$phase" "After Swap Prev" "$results_file"

    # Test 6: Edge Cases
    echo "--- Test 6: Edge Cases ---" >> "$results_file"

    # Test invalid mfact values
    log_filtered_command "$phase" "Invalid mfact (too high)" "hyprctl -i $INSTANCE dispatch layoutmsg 'mfact exact 1.5'" "$results_file"
    capture_filtered_state "$phase" "After Invalid mfact High" "$results_file"

    log_filtered_command "$phase" "Invalid mfact (negative)" "hyprctl -i $INSTANCE dispatch layoutmsg 'mfact exact -0.1'" "$results_file"
    capture_filtered_state "$phase" "After Invalid mfact Negative" "$results_file"

    # Test center orientation
    log_filtered_command "$phase" "Orient Center" "hyprctl -i $INSTANCE dispatch layoutmsg orientationcenter" "$results_file"
    capture_filtered_state "$phase" "After Orient Center" "$results_file"

    echo "=== Test Scenarios Complete for ______ ===" >> "$results_file"
}
