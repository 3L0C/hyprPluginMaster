# hyprPluginMaster

This is a plugin version of Hyprland's master layout.

This should be used as a forking point for other layouts
that want to implement master based layouts, spiral,
monocle, deck, etc.

# Configuration

The same as master, but wrapped in a plugin.

```
plugin {
    pluginmaster {
        allow_small_split = false
        special_scale_factor = 1.0
        mfact = 0.55
        new_status = slave
        new_on_top = false
        new_on_active = none
        orientation = left
        inherit_fullscreen = true
        slave_count_for_center_master = 2
        center_master_fallback = left
        smart_resizing = true
        drop_at_cursor = true
        always_keep_position = false
    }
}
```

# Installing

## Hyprpm (recommended)

```sh
hyprpm add https://github.com/3L0C/hyprPluginMaster
hyprpm enable hyprPluginMaster 
hyprctl keyword general:layout pluginmaster
```

## Manual

If you don’t have Hyprland headers installed, follow the
[Hyprland
wiki](https://wiki.hypr.land/Plugins/Using-Plugins/#manual).
Then: 

```sh
make 
mkdir -p ~/.config/hypr/plugins
cp masterLayoutPlugin.so ~/.config/hypr/plugins
# add this with `exec-once = ...` to your Hyprland config to autoload.
hyprctl plugin load ~/.config/hypr/plugins/masterLayoutPlugin.so 
hyprctl keyword general:layout pluginmaster
```

## Testing

To validate parity between builtin Master Layout and the
plugin:

1. Follow the manual install instructions
2. Start your test instance of Hyprland. The script assumes
you are running within an existing Hyprland instance (i.e.,
0). If you are not, modify hypr_plugin_testing_framework.sh
to the correct instance. 
3. Make the script executable `chmod +x
   hypr_plugin_master_test.sh`.
4. Run the script `./hypr_plugin_master_test.sh`.
5. Allow your test Hyprland to load the plugin.
6. Then press enter on the terminal where you ran the test script.

**NOTE**: The test will execute commands to spawn terminal
windows (how can you test a layout without windows?). The
terminal is `kitty`. To give some variety, it will spawn
`kitty` with some terminal programs, namely, `htop`, `top`,
`neofetch`, and `vim`. If you do not have these programs,
you can modify the `spawn_test_clients()` procedure in
`hypr_plugin_testing_framework.sh`. 
