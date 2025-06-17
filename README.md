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
        orientation = left
        mfact = 0.55f
        new_status = slave
        new_on_top = 0
        new_on_active = none
        inherit_fullscreen = 1
        special_scale_factor = 1.0f
        smart_resizing = 1
        drop_at_cursor = 1
        allow_small_split = 0
        always_keep_position = 0
        slave_count_for_center_master = 2
        center_master_fallback = left
        center_ignores_reserved = 0
    }
}
```

# Installing

## Hyprpm (recommended)

```
hyprpm add https://github.com/3L0C/hyprPluginMaster
hyprpm enable hyprPluginMaster 
hyprctl keyword general:layout pluginmaster
```

## Manual

If you don’t have Hyprland headers installed, follow the
[Hyprland
wiki](https://wiki.hypr.land/Plugins/Using-Plugins/#manual).
Then: 

```
make 
mkdir -p ~/.config/hypr/plugins
cp masterLayoutPlugin.so ~/.config/hypr/plugins
# add this with `exec-once = ...` to your Hyprland config to autoload.
hyprctl plugin load ~/.config/hypr/plugins/masterLayoutPlugin.so 
hyprctl keyword general:layout pluginmaster
```

