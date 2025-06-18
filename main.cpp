#include "globals.hpp"
#include "PluginMasterLayout.hpp"
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/Workspace.hpp>

// Global layout instance - using plugin-specific class name
inline std::unique_ptr<CPluginMasterLayout> g_pPluginMasterLayout;

// Do NOT change this function - required by plugin API
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

// Workspace cleanup helper
static void deleteWorkspaceData(int ws) {
    if (g_pPluginMasterLayout) {
        g_pPluginMasterLayout->removeWorkspaceData(ws);
    }
}

// Callback for workspace move events
void moveWorkspaceCallback(void* self, SCallbackInfo& cinfo, std::any data) {
    std::vector<std::any> moveData = std::any_cast<std::vector<std::any>>(data);
    PHLWORKSPACE          ws       = std::any_cast<PHLWORKSPACE>(moveData.front());
    deleteWorkspaceData(ws->m_id);
}

// REQUIRED: Plugin initialization function
APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    // Add all master layout config values with plugin namespace
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:orientation", Hyprlang::STRING{"left"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:mfact", Hyprlang::FLOAT{0.55f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:new_status", Hyprlang::STRING{"slave"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:new_on_top", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:new_on_active", Hyprlang::STRING{"none"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:inherit_fullscreen", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:special_scale_factor", Hyprlang::FLOAT{1.0f});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:smart_resizing", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:drop_at_cursor", Hyprlang::INT{1});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:allow_small_split", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:always_keep_position", Hyprlang::INT{0});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:slave_count_for_center_master", Hyprlang::INT{2});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:center_master_fallback", Hyprlang::STRING{"left"});
    HyprlandAPI::addConfigValue(PHANDLE, "plugin:pluginmaster:center_ignores_reserved", Hyprlang::INT{0});

    // Create plugin master layout instance
    g_pPluginMasterLayout = std::make_unique<CPluginMasterLayout>();

    // Register workspace event callbacks for cleanup
    static auto MWCB = HyprlandAPI::registerCallbackDynamic(PHANDLE, "moveWorkspace", moveWorkspaceCallback);

    static auto DWCB = HyprlandAPI::registerCallbackDynamic(PHANDLE, "destroyWorkspace", [&](void* self, SCallbackInfo&, std::any data) {
        CWorkspace* ws = std::any_cast<CWorkspace*>(data);
        deleteWorkspaceData(ws->m_id);
    });

    // Register the layout with Hyprland using a distinct name
    HyprlandAPI::addLayout(PHANDLE, "pluginmaster", g_pPluginMasterLayout.get());

    // Reload config to apply new values
    HyprlandAPI::reloadConfig();

    return {"hyprPluginMaster", "Hyprland Master Layout Plugin", "Community", "1.0"};
}

// OPTIONAL: Plugin cleanup function
APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::invokeHyprctlCommand("seterror", "disable");
}
