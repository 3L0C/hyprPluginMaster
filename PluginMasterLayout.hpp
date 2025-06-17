#pragma once

#include "globals.hpp"
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/varlist/VarList.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <vector>
#include <list>
#include <any>

enum eFullscreenMode : int8_t;

//orientation determines which side of the screen the master area resides
enum ePluginOrientation : uint8_t {
    PLUGIN_ORIENTATION_LEFT = 0,
    PLUGIN_ORIENTATION_TOP,
    PLUGIN_ORIENTATION_RIGHT,
    PLUGIN_ORIENTATION_BOTTOM,
    PLUGIN_ORIENTATION_CENTER
};

struct SPluginMasterNodeData {
    bool         isMaster   = false;
    float        percMaster = 0.5f;

    PHLWINDOWREF pWindow;

    Vector2D     position;
    Vector2D     size;

    float        percSize = 1.f; // size multiplier for resizing children

    WORKSPACEID  workspaceID = WORKSPACE_INVALID;

    bool         ignoreFullscreenChecks = false;

    //
    bool operator==(const SPluginMasterNodeData& rhs) const {
        return pWindow.lock() == rhs.pWindow.lock();
    }
};

struct SPluginMasterWorkspaceData {
    WORKSPACEID        workspaceID = WORKSPACE_INVALID;
    ePluginOrientation orientation = PLUGIN_ORIENTATION_LEFT;

    //
    bool operator==(const SPluginMasterWorkspaceData& rhs) const {
        return workspaceID == rhs.workspaceID;
    }
};

class CPluginMasterLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(PHLWINDOW);
    virtual bool                     isWindowTiled(PHLWINDOW);
    virtual void                     recalculateMonitor(const MONITORID&);
    virtual void                     recalculateWindow(PHLWINDOW);
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(PHLWINDOW);
    virtual void                     switchWindows(PHLWINDOW, PHLWINDOW);
    virtual void                     moveWindowTo(PHLWINDOW, const std::string& dir, bool silent);
    virtual void                     alterSplitRatio(PHLWINDOW, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to);
    virtual Vector2D                 predictSizeForNewWindowTiled();

    virtual void                     onEnable();
    virtual void                     onDisable();
    
    // Plugin-specific method for workspace cleanup
    void                             removeWorkspaceData(const WORKSPACEID& ws);

  private:
    std::list<SPluginMasterNodeData>        m_masterNodesData;
    std::vector<SPluginMasterWorkspaceData> m_masterWorkspacesData;

    bool                                    m_forceWarps = false;

    void                                    buildOrientationCycleVectorFromVars(std::vector<ePluginOrientation>& cycle, CVarList& vars);
    void                                    buildOrientationCycleVectorFromEOperation(std::vector<ePluginOrientation>& cycle);
    void                                    runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int next);
    ePluginOrientation                      getDynamicOrientation(PHLWORKSPACE);
    int                                     getNodesOnWorkspace(const WORKSPACEID&);
    void                                    applyNodeDataToWindow(SPluginMasterNodeData*);
    SPluginMasterNodeData*                  getNodeFromWindow(PHLWINDOW);
    SPluginMasterNodeData*                  getMasterNodeOnWorkspace(const WORKSPACEID&);
    SPluginMasterWorkspaceData*             getMasterWorkspaceData(const WORKSPACEID&);
    void                                    calculateWorkspace(PHLWORKSPACE);
    PHLWINDOW                               getNextWindow(PHLWINDOW, bool, bool);
    int                                     getMastersOnWorkspace(const WORKSPACEID&);

    friend struct SPluginMasterNodeData;
    friend struct SPluginMasterWorkspaceData;
};

template <typename CharT>
struct std::formatter<SPluginMasterNodeData*, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const SPluginMasterNodeData* const& node, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!node)
            return std::format_to(out, "[Node nullptr]");
        std::format_to(out, "[Node {:x}: workspace: {}, pos: {:j2}, size: {:j2}", (uintptr_t)node, node->workspaceID, node->position, node->size);
        if (node->isMaster)
            std::format_to(out, ", master");
        if (!node->pWindow.expired())
            std::format_to(out, ", window: {:x}", node->pWindow.lock());
        return std::format_to(out, "]");
    }
};
