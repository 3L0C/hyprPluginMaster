#include "PluginMasterLayout.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <ranges>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

SPluginMasterNodeData* CPluginMasterLayout::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& nd : m_masterNodesData) {
        if (nd.pWindow.lock() == pWindow)
            return &nd;
    }

    return nullptr;
}

int CPluginMasterLayout::getNodesOnWorkspace(const WORKSPACEID& ws) {
    int no = 0;
    for (auto const& n : m_masterNodesData) {
        if (n.workspaceID == ws)
            no++;
    }

    return no;
}

int CPluginMasterLayout::getMastersOnWorkspace(const WORKSPACEID& ws) {
    int no = 0;
    for (auto const& n : m_masterNodesData) {
        if (n.workspaceID == ws && n.isMaster)
            no++;
    }

    return no;
}

SPluginMasterWorkspaceData* CPluginMasterLayout::getMasterWorkspaceData(const WORKSPACEID& ws) {
    for (auto& n : m_masterWorkspacesData) {
        if (n.workspaceID == ws)
            return &n;
    }

    //create on the fly if it doesn't exist yet
    const auto PWORKSPACEDATA   = &m_masterWorkspacesData.emplace_back();
    PWORKSPACEDATA->workspaceID = ws;
    static auto* const PORIENTATION = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:orientation")->getDataStaticPtr();
    std::string        SORIENTATION = *PORIENTATION;

    if (SORIENTATION == "top")
        PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_TOP;
    else if (SORIENTATION == "right")
        PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_RIGHT;
    else if (SORIENTATION == "bottom")
        PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_BOTTOM;
    else if (SORIENTATION == "center")
        PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_CENTER;
    else
        PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_LEFT;

    return PWORKSPACEDATA;
}

std::string CPluginMasterLayout::getLayoutName() {
    return "PluginMaster";
}

SPluginMasterNodeData* CPluginMasterLayout::getMasterNodeOnWorkspace(const WORKSPACEID& ws) {
    for (auto& n : m_masterNodesData) {
        if (n.isMaster && n.workspaceID == ws)
            return &n;
    }

    return nullptr;
}

void CPluginMasterLayout::onWindowCreatedTiling(PHLWINDOW pWindow, eDirection direction) {
    if (pWindow->m_isFloating)
        return;

    static auto* const PNEWONACTIVE  = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:new_on_active")->getDataStaticPtr();
    std::string        SNEWONACTIVE  = *PNEWONACTIVE;
    static auto* const PNEWONTOP     = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:new_on_top")->getDataStaticPtr();
    const  bool        BNEWONTOP     = **PNEWONTOP ? true : false;
    static auto* const PNEWSTATUS    = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:new_status")->getDataStaticPtr();
    std::string        SNEWSTATUS    = *PNEWSTATUS;

    const auto  PMONITOR = pWindow->m_monitor.lock();

    const bool  BNEWBEFOREACTIVE = SNEWONACTIVE == "before";
    const bool  BNEWISMASTER     = SNEWSTATUS == "master";

    const auto  PNODE = [&]() {
        if (SNEWONACTIVE != "none" && !BNEWISMASTER) {
            const auto pLastNode = getNodeFromWindow(g_pCompositor->m_lastWindow.lock());
            if (pLastNode && !(pLastNode->isMaster && (getMastersOnWorkspace(pWindow->workspaceID()) == 1 || *PNEWSTATUS == "slave"))) {
                auto it = std::ranges::find(m_masterNodesData, *pLastNode);
                if (!BNEWBEFOREACTIVE)
                    ++it;
                return &(*m_masterNodesData.emplace(it));
            }
        }
        return **PNEWONTOP ? &m_masterNodesData.emplace_front() : &m_masterNodesData.emplace_back();
    }();

    PNODE->workspaceID = pWindow->workspaceID();
    PNODE->pWindow     = pWindow;

    const auto   WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    static auto* const PMFACT       = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:mfact")->getDataStaticPtr();
    float              FMFACT       = **PMFACT;

    auto         OPENINGON = isWindowTiled(g_pCompositor->m_lastWindow.lock()) && g_pCompositor->m_lastWindow->m_workspace == pWindow->m_workspace ?
                getNodeFromWindow(g_pCompositor->m_lastWindow.lock()) :
                getMasterNodeOnWorkspace(pWindow->workspaceID());

    const auto   MOUSECOORDS   = g_pInputManager->getMouseCoordsInternal();
    static auto* const PDROPATCURSOR = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:drop_at_cursor")->getDataStaticPtr();
    int64_t            IDROPATCURSOR = **PDROPATCURSOR;
    ePluginOrientation orientation   = getDynamicOrientation(pWindow->m_workspace);
    const auto   NODEIT        = std::ranges::find(m_masterNodesData, *PNODE);

    bool         forceDropAsMaster = false;
    // if dragging window to move, drop it at the cursor position instead of bottom/top of stack
    if (IDROPATCURSOR && g_pInputManager->m_dragMode == MBIND_MOVE) {
        if (WINDOWSONWORKSPACE > 2) {
            for (auto it = m_masterNodesData.begin(); it != m_masterNodesData.end(); ++it) {
                if (it->workspaceID != pWindow->workspaceID())
                    continue;
                const CBox box = it->pWindow->getWindowIdealBoundingBoxIgnoreReserved();
                if (box.containsPoint(MOUSECOORDS)) {
                    switch (orientation) {
                        case PLUGIN_ORIENTATION_LEFT:
                        case PLUGIN_ORIENTATION_RIGHT:
                            if (MOUSECOORDS.y > it->pWindow->middle().y)
                                ++it;
                            break;
                        case PLUGIN_ORIENTATION_TOP:
                        case PLUGIN_ORIENTATION_BOTTOM:
                            if (MOUSECOORDS.x > it->pWindow->middle().x)
                                ++it;
                            break;
                        case PLUGIN_ORIENTATION_CENTER: break;
                        default: UNREACHABLE();
                    }
                    m_masterNodesData.splice(it, m_masterNodesData, NODEIT);
                    break;
                }
            }
        } else if (WINDOWSONWORKSPACE == 2) {
            // when dropping as the second tiled window in the workspace,
            // make it the master only if the cursor is on the master side of the screen
            for (auto const& nd : m_masterNodesData) {
                if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                    switch (orientation) {
                        case PLUGIN_ORIENTATION_LEFT:
                        case PLUGIN_ORIENTATION_CENTER:
                            if (MOUSECOORDS.x < nd.pWindow->middle().x)
                                forceDropAsMaster = true;
                            break;
                        case PLUGIN_ORIENTATION_RIGHT:
                            if (MOUSECOORDS.x > nd.pWindow->middle().x)
                                forceDropAsMaster = true;
                            break;
                        case PLUGIN_ORIENTATION_TOP:
                            if (MOUSECOORDS.y < nd.pWindow->middle().y)
                                forceDropAsMaster = true;
                            break;
                        case PLUGIN_ORIENTATION_BOTTOM:
                            if (MOUSECOORDS.y > nd.pWindow->middle().y)
                                forceDropAsMaster = true;
                            break;
                        default: UNREACHABLE();
                    }
                    break;
                }
            }
        }
    }

    if ((BNEWISMASTER && g_pInputManager->m_dragMode != MBIND_MOVE)                //
        || WINDOWSONWORKSPACE == 1                                                 //
        || (WINDOWSONWORKSPACE > 2 && !pWindow->m_firstMap && OPENINGON->isMaster) //
        || forceDropAsMaster                                                       //
        || (SNEWSTATUS == "inherit" && OPENINGON && OPENINGON->isMaster && g_pInputManager->m_dragMode != MBIND_MOVE)) {

        if (BNEWBEFOREACTIVE) {
            for (auto& nd : m_masterNodesData | std::views::reverse) {
                if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                    nd.isMaster      = false;
                    FMFACT = nd.percMaster;
                    break;
                }
            }
        } else {
            for (auto& nd : m_masterNodesData) {
                if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                    nd.isMaster      = false;
                    FMFACT = nd.percMaster;
                    break;
                }
            }
        }

        PNODE->isMaster   = true;
        PNODE->percMaster = FMFACT;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize(); MAXSIZE.x < PMONITOR->m_size.x * FMFACT || MAXSIZE.y < PMONITOR->m_size.y) {
            // we can't continue. make it floating.
            pWindow->m_isFloating = true;
            m_masterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        PNODE->isMaster   = false;
        PNODE->percMaster = FMFACT;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize();
            MAXSIZE.x < PMONITOR->m_size.x * (1 - FMFACT) || MAXSIZE.y < PMONITOR->m_size.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_isFloating = true;
            m_masterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    // recalc
    recalculateMonitor(pWindow->monitorID());
}

void CPluginMasterLayout::onWindowRemovedTiling(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto  WORKSPACEID = PNODE->workspaceID;
    const auto  MASTERSLEFT = getMastersOnWorkspace(WORKSPACEID);
    static auto* const SMALLSPLIT  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:allow_small_split")->getDataStaticPtr();
    int64_t            ISMALLSPLIT = **SMALLSPLIT;

    pWindow->unsetWindowData(PRIORITY_LAYOUT);
    pWindow->updateWindowData();

    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    if (PNODE->isMaster && (MASTERSLEFT <= 1 || ISMALLSPLIT == 1)) {
        // find a new master from top of the list
        for (auto& nd : m_masterNodesData) {
            if (!nd.isMaster && nd.workspaceID == WORKSPACEID) {
                nd.isMaster   = true;
                nd.percMaster = PNODE->percMaster;
                break;
            }
        }
    }

    m_masterNodesData.remove(*PNODE);

    if (getMastersOnWorkspace(WORKSPACEID) == getNodesOnWorkspace(WORKSPACEID) && MASTERSLEFT > 1) {
        for (auto& nd : m_masterNodesData | std::views::reverse) {
            if (nd.workspaceID == WORKSPACEID) {
                nd.isMaster = false;
                break;
            }
        }
    }
    // BUGFIX: correct bug where closing one master in a stack of 2 would leave
    // the screen half bare, and make it difficult to select remaining window
    if (getNodesOnWorkspace(WORKSPACEID) == 1) {
        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID == WORKSPACEID && !nd.isMaster) {
                nd.isMaster = true;
                break;
            }
        }
    }
    recalculateMonitor(pWindow->monitorID());
}

void CPluginMasterLayout::recalculateMonitor(const MONITORID& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

    if (!PMONITOR || !PMONITOR->m_activeWorkspace)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->m_activeSpecialWorkspace)
        calculateWorkspace(PMONITOR->m_activeSpecialWorkspace);

    calculateWorkspace(PMONITOR->m_activeWorkspace);
}

void CPluginMasterLayout::calculateWorkspace(PHLWORKSPACE pWorkspace) {
    const auto PMONITOR = pWorkspace->m_monitor.lock();

    if (!PMONITOR)
        return;
    
    if (pWorkspace->m_hasFullscreenWindow) {
        // massive hack from the fullscreen func
        const auto PFULLWINDOW = pWorkspace->getFullscreenWindow();

        if (pWorkspace->m_fullscreenMode == FSMODE_FULLSCREEN) {
            *PFULLWINDOW->m_realPosition = PMONITOR->m_position;
            *PFULLWINDOW->m_realSize     = PMONITOR->m_size;
        } else if (pWorkspace->m_fullscreenMode == FSMODE_MAXIMIZED) {
            SPluginMasterNodeData fakeNode;
            fakeNode.pWindow                = PFULLWINDOW;
            fakeNode.position               = PMONITOR->m_position + PMONITOR->m_reservedTopLeft;
            fakeNode.size                   = PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight;
            fakeNode.workspaceID            = pWorkspace->m_id;
            PFULLWINDOW->m_position         = fakeNode.position;
            PFULLWINDOW->m_size             = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }

        // if has fullscreen, don't calculate the rest
        return;
    }

    const auto PMASTERNODE = getMasterNodeOnWorkspace(pWorkspace->m_id);

    if (!PMASTERNODE) {
        return;
    }
    
    const auto MASTERS = getMastersOnWorkspace(pWorkspace->m_id);
    const auto WINDOWS = getNodesOnWorkspace(pWorkspace->m_id);

    ePluginOrientation orientation          = getDynamicOrientation(pWorkspace);
    bool               centerMasterWindow   = false;
    static auto* const SLAVECOUNTFORCENTER  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:slave_count_for_center_master")->getDataStaticPtr();
    int64_t            ISLAVECOUNTFORCENTER = **SLAVECOUNTFORCENTER;
    static auto* const CMFALLBACK           = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:center_master_fallback")->getDataStaticPtr();
    std::string        SCMFALLBACK          = *CMFALLBACK;
    static auto* const PIGNORERESERVED      = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:center_ignores_reserved")->getDataStaticPtr();
    int64_t            IIGNORERESERVED      = **PIGNORERESERVED;
    static auto* const PSMARTRESIZING       = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:smart_resizing")->getDataStaticPtr();
    int64_t            ISMARTRESIZING       = **PSMARTRESIZING;

    const auto   STACKWINDOWS = WINDOWS - MASTERS;
    const auto   WSSIZE       = PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight;
    const auto   WSPOS        = PMONITOR->m_position + PMONITOR->m_reservedTopLeft;

    if (orientation == PLUGIN_ORIENTATION_CENTER) {
        if (STACKWINDOWS >= ISLAVECOUNTFORCENTER) {
            centerMasterWindow = true;
        } else {
            if (SCMFALLBACK == "left")
                orientation = PLUGIN_ORIENTATION_LEFT;
            else if (SCMFALLBACK == "right")
                orientation = PLUGIN_ORIENTATION_RIGHT;
            else if (SCMFALLBACK == "top")
                orientation = PLUGIN_ORIENTATION_TOP;
            else if (SCMFALLBACK == "bottom")
                orientation = PLUGIN_ORIENTATION_BOTTOM;
            else
                orientation = PLUGIN_ORIENTATION_LEFT;
        }
    }

    const float totalSize             = (orientation == PLUGIN_ORIENTATION_TOP || orientation == PLUGIN_ORIENTATION_BOTTOM) ? WSSIZE.x : WSSIZE.y;
    const float masterAverageSize     = totalSize / MASTERS;
    const float slaveAverageSize      = totalSize / STACKWINDOWS;
    float       masterAccumulatedSize = 0;
    float       slaveAccumulatedSize  = 0;

    if (ISMARTRESIZING) {
        // check the total width and height so that later
        // if larger/smaller than screen size them down/up
        for (auto const& nd : m_masterNodesData) {
            if (nd.workspaceID == pWorkspace->m_id) {
                if (nd.isMaster)
                    masterAccumulatedSize += totalSize / MASTERS * nd.percSize;
                else
                    slaveAccumulatedSize += totalSize / STACKWINDOWS * nd.percSize;
            }
        }
    }

    // compute placement of master window(s)
    if (WINDOWS == 1 && !centerMasterWindow) {
        static auto* const PALWAYSKEEPPOSITION = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:always_keep_position")->getDataStaticPtr();
        int64_t            IALWAYSKEEPPOSITION = **PALWAYSKEEPPOSITION;
        if (IALWAYSKEEPPOSITION) {
            const float WIDTH = WSSIZE.x * PMASTERNODE->percMaster;
            float       nextX = 0;

            if (orientation == PLUGIN_ORIENTATION_RIGHT)
                nextX = WSSIZE.x - WIDTH;
            else if (orientation == PLUGIN_ORIENTATION_CENTER)
                nextX = (WSSIZE.x - WIDTH) / 2;

            PMASTERNODE->size     = Vector2D(WIDTH, WSSIZE.y);
            PMASTERNODE->position = WSPOS + Vector2D((double)nextX, 0.0);
        } else {
            PMASTERNODE->size     = WSSIZE;
            PMASTERNODE->position = WSPOS;
        }

        applyNodeDataToWindow(PMASTERNODE);
        return;
    } else if (orientation == PLUGIN_ORIENTATION_TOP || orientation == PLUGIN_ORIENTATION_BOTTOM) {
        const float HEIGHT      = STACKWINDOWS != 0 ? WSSIZE.y * PMASTERNODE->percMaster : WSSIZE.y;
        float       widthLeft   = WSSIZE.x;
        int         mastersLeft = MASTERS;
        float       nextX       = 0;
        float       nextY       = 0;

        if (orientation == PLUGIN_ORIENTATION_BOTTOM)
            nextY = WSSIZE.y - HEIGHT;

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || !nd.isMaster)
                continue;

            float WIDTH = mastersLeft > 1 ? widthLeft / mastersLeft * nd.percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && mastersLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (ISMARTRESIZING) {
                nd.percSize *= WSSIZE.x / masterAccumulatedSize;
                WIDTH = masterAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            mastersLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else { // orientation left, right or center
        float WIDTH       = IIGNORERESERVED && centerMasterWindow ? PMONITOR->m_size.x : WSSIZE.x;
        float heightLeft  = WSSIZE.y;
        int   mastersLeft = MASTERS;
        float nextX       = 0;
        float nextY       = 0;

        if (STACKWINDOWS > 0 || centerMasterWindow)
            WIDTH *= PMASTERNODE->percMaster;

        if (orientation == PLUGIN_ORIENTATION_RIGHT) {
            nextX = WSSIZE.x - WIDTH;
        } else if (centerMasterWindow) {
            nextX = ((IIGNORERESERVED && centerMasterWindow ? PMONITOR->m_size.x : WSSIZE.x) - WIDTH) / 2;
        }

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || !nd.isMaster)
                continue;

            float HEIGHT = mastersLeft > 1 ? heightLeft / mastersLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && mastersLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (ISMARTRESIZING) {
                nd.percSize *= WSSIZE.y / masterAccumulatedSize;
                HEIGHT = masterAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = (IIGNORERESERVED && centerMasterWindow ? PMONITOR->m_position : WSPOS) + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            mastersLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;
        }
    }

    if (STACKWINDOWS == 0)
        return;

    // compute placement of slave window(s)
    int slavesLeft = STACKWINDOWS;
    if (orientation == PLUGIN_ORIENTATION_TOP || orientation == PLUGIN_ORIENTATION_BOTTOM) {
        const float HEIGHT    = WSSIZE.y - PMASTERNODE->size.y;
        float       widthLeft = WSSIZE.x;
        float       nextX     = 0;
        float       nextY     = 0;

        if (orientation == PLUGIN_ORIENTATION_TOP)
            nextY = PMASTERNODE->size.y;

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                continue;

            float WIDTH = slavesLeft > 1 ? widthLeft / slavesLeft * nd.percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && slavesLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (ISMARTRESIZING) {
                nd.percSize *= WSSIZE.x / slaveAccumulatedSize;
                WIDTH = slaveAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            slavesLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else if (orientation == PLUGIN_ORIENTATION_LEFT || orientation == PLUGIN_ORIENTATION_RIGHT) {
        const float WIDTH      = WSSIZE.x - PMASTERNODE->size.x;
        float       heightLeft = WSSIZE.y;
        float       nextY      = 0;
        float       nextX      = 0;

        if (orientation == PLUGIN_ORIENTATION_LEFT)
            nextX = PMASTERNODE->size.x;

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                continue;

            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (ISMARTRESIZING) {
                nd.percSize *= WSSIZE.y / slaveAccumulatedSize;
                HEIGHT = slaveAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            slavesLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;
        }
    } else { // slaves for centered master window(s)
        const float WIDTH       = ((IIGNORERESERVED ? PMONITOR->m_size.x : WSSIZE.x) - PMASTERNODE->size.x) / 2.0;
        float       heightLeft  = 0;
        float       heightLeftL = WSSIZE.y;
        float       heightLeftR = WSSIZE.y;
        float       nextX       = 0;
        float       nextY       = 0;
        float       nextYL      = 0;
        float       nextYR      = 0;
        bool        onRight     = SCMFALLBACK == "right";
        int         slavesLeftL = 1 + (slavesLeft - 1) / 2;
        int         slavesLeftR = slavesLeft - slavesLeftL;

        if (onRight) {
            slavesLeftR = 1 + (slavesLeft - 1) / 2;
            slavesLeftL = slavesLeft - slavesLeftR;
        }

        const float slaveAverageHeightL     = WSSIZE.y / slavesLeftL;
        const float slaveAverageHeightR     = WSSIZE.y / slavesLeftR;
        float       slaveAccumulatedHeightL = 0;
        float       slaveAccumulatedHeightR = 0;

        if (ISMARTRESIZING) {
            for (auto const& nd : m_masterNodesData) {
                if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                    continue;

                if (onRight) {
                    slaveAccumulatedHeightR += slaveAverageHeightR * nd.percSize;
                } else {
                    slaveAccumulatedHeightL += slaveAverageHeightL * nd.percSize;
                }
                onRight = !onRight;
            }

            onRight = SCMFALLBACK == "right";
        }

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                continue;

            if (onRight) {
                nextX      = WIDTH + PMASTERNODE->size.x - (IIGNORERESERVED ? PMONITOR->m_reservedTopLeft.x : 0);
                nextY      = nextYR;
                heightLeft = heightLeftR;
                slavesLeft = slavesLeftR;
            } else {
                nextX      = 0;
                nextY      = nextYL;
                heightLeft = heightLeftL;
                slavesLeft = slavesLeftL;
            }

            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (ISMARTRESIZING) {
                if (onRight) {
                    nd.percSize *= WSSIZE.y / slaveAccumulatedHeightR;
                    HEIGHT = slaveAverageHeightR * nd.percSize;
                } else {
                    nd.percSize *= WSSIZE.y / slaveAccumulatedHeightL;
                    HEIGHT = slaveAverageHeightL * nd.percSize;
                }
            }

            nd.size     = Vector2D(IIGNORERESERVED ? (WIDTH - (onRight ? PMONITOR->m_reservedBottomRight.x : PMONITOR->m_reservedTopLeft.x)) : WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            if (onRight) {
                heightLeftR -= HEIGHT;
                nextYR += HEIGHT;
                slavesLeftR--;
            } else {
                heightLeftL -= HEIGHT;
                nextYL += HEIGHT;
                slavesLeftL--;
            }

            onRight = !onRight;
        }
    }
}

void CPluginMasterLayout::applyNodeDataToWindow(SPluginMasterNodeData* pNode) {
    PHLMONITOR PMONITOR = nullptr;
    
    const auto PWINDOW = pNode->pWindow.lock();

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto const& m : g_pCompositor->m_monitors) {
            if (m->activeSpecialWorkspaceID() == pNode->workspaceID) {
                PMONITOR = m;
                break;
            }
        }
    } else
        PMONITOR = g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_monitor.lock();

    if (!PMONITOR) {
        return;
    }
    
    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);

    // const auto PWINDOW = pNode->pWindow.lock();
    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(PWINDOW->m_workspace);

    if (PWINDOW->isFullscreen() && !pNode->ignoreFullscreenChecks)
        return;

    PWINDOW->unsetWindowData(PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();

    static auto* const PANIMATE = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes");
    static auto* const PGAPSINDATA  = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_in");
    static auto* const PGAPSOUTDATA = (Hyprlang::CUSTOMTYPE* const*)g_pConfigManager->getConfigValuePtr("general:gaps_out");
    auto* const PGAPSIN      = (CCssGapData*)(*PGAPSINDATA)->getData();
    auto* const PGAPSOUT     = (CCssGapData*)(*PGAPSOUTDATA)->getData();

    auto        gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto        gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);

    if (!validMapped(PWINDOW)) {
        return;
    }

    PWINDOW->m_size     = pNode->size;
    PWINDOW->m_position = pNode->position;

    PWINDOW->updateWindowDecos();

    auto       calcPos  = PWINDOW->m_position;
    auto       calcSize = PWINDOW->m_size;

    const auto OFFSETTOPLEFT = Vector2D((double)(DISPLAYLEFT ? gapsOut.m_left : gapsIn.m_left), (double)(DISPLAYTOP ? gapsOut.m_top : gapsIn.m_top));

    const auto OFFSETBOTTOMRIGHT = Vector2D((double)(DISPLAYRIGHT ? gapsOut.m_right : gapsIn.m_right), (double)(DISPLAYBOTTOM ? gapsOut.m_bottom : gapsIn.m_bottom));

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (PWINDOW->onSpecialWorkspace() && !PWINDOW->isFullscreen()) {
        static auto* const PSCALEFACTOR = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:special_scale_factor")->getDataStaticPtr();
        float              FSCALEFACTOR = **PSCALEFACTOR;

        CBox        wb = {calcPos + (calcSize - calcSize * FSCALEFACTOR) / 2.f, calcSize * FSCALEFACTOR};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realPosition = wb.pos();
        *PWINDOW->m_realSize     = wb.size();
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realPosition = wb.pos();
        *PWINDOW->m_realSize     = wb.size();
    }
    
    if (m_forceWarps && !**PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_realPosition->warp();
        PWINDOW->m_realSize->warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CPluginMasterLayout::isWindowTiled(PHLWINDOW pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CPluginMasterLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, PHLWINDOW pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_lastWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        *PWINDOW->m_realSize =
            (PWINDOW->m_realSize->goal() + pixResize)
                .clamp(PWINDOW->m_windowData.minSize.valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), PWINDOW->m_windowData.maxSize.valueOr(Vector2D{INFINITY, INFINITY}));
        PWINDOW->updateWindowDecos();
        return;
    }

    const auto   PMONITOR            = PWINDOW->m_monitor.lock();
    static auto* const SLAVECOUNTFORCENTER  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:slave_count_for_center_master")->getDataStaticPtr();
    int64_t            ISLAVECOUNTFORCENTER = **SLAVECOUNTFORCENTER;
    static auto* const PSMARTRESIZING = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:smart_resizing")->getDataStaticPtr();
    int64_t            ISMARTRESIZING = **PSMARTRESIZING;

    const bool   DISPLAYBOTTOM = STICKS(PWINDOW->m_position.y + PWINDOW->m_size.y, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);
    const bool   DISPLAYRIGHT  = STICKS(PWINDOW->m_position.x + PWINDOW->m_size.x, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool   DISPLAYTOP    = STICKS(PWINDOW->m_position.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool   DISPLAYLEFT   = STICKS(PWINDOW->m_position.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);

    const bool   LEFT = corner == CORNER_TOPLEFT || corner == CORNER_BOTTOMLEFT;
    const bool   TOP  = corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT;
    const bool   NONE = corner == CORNER_NONE;

    const auto   MASTERS      = getMastersOnWorkspace(PNODE->workspaceID);
    const auto   WINDOWS      = getNodesOnWorkspace(PNODE->workspaceID);
    const auto   STACKWINDOWS = WINDOWS - MASTERS;

    ePluginOrientation orientation = getDynamicOrientation(PWINDOW->m_workspace);
    bool         centered    = orientation == PLUGIN_ORIENTATION_CENTER && (STACKWINDOWS >= ISLAVECOUNTFORCENTER);
    double       delta       = 0;

    if (getNodesOnWorkspace(PWINDOW->workspaceID()) == 1 && !centered)
        return;

    m_forceWarps = true;

    switch (orientation) {
        case PLUGIN_ORIENTATION_LEFT: delta = pixResize.x / PMONITOR->m_size.x; break;
        case PLUGIN_ORIENTATION_RIGHT: delta = -pixResize.x / PMONITOR->m_size.x; break;
        case PLUGIN_ORIENTATION_BOTTOM: delta = -pixResize.y / PMONITOR->m_size.y; break;
        case PLUGIN_ORIENTATION_TOP: delta = pixResize.y / PMONITOR->m_size.y; break;
        case PLUGIN_ORIENTATION_CENTER:
            delta = pixResize.x / PMONITOR->m_size.x;
            if (STACKWINDOWS >= ISLAVECOUNTFORCENTER) {
                if (!NONE || !PNODE->isMaster)
                    delta *= 2;
                if ((!PNODE->isMaster && DISPLAYLEFT) || (PNODE->isMaster && LEFT && ISMARTRESIZING))
                    delta = -delta;
            }
            break;
        default: UNREACHABLE();
    }

    const auto workspaceIdForResizing = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();
    for (auto& n : m_masterNodesData) {
        if (n.isMaster && n.workspaceID == workspaceIdForResizing) {
            float oldPercMaster = n.percMaster;
            n.percMaster = std::clamp(n.percMaster + delta, 0.05, 0.95);
        }
    }

    // check the up/down resize
    const bool isStackVertical = orientation == PLUGIN_ORIENTATION_LEFT || orientation == PLUGIN_ORIENTATION_RIGHT || orientation == PLUGIN_ORIENTATION_CENTER;

    const auto RESIZEDELTA = isStackVertical ? pixResize.y : pixResize.x;
    const auto WSSIZE      = PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight;

    auto       nodesInSameColumn = PNODE->isMaster ? MASTERS : STACKWINDOWS;
    if (orientation == PLUGIN_ORIENTATION_CENTER && !PNODE->isMaster)
        nodesInSameColumn = DISPLAYRIGHT ? (nodesInSameColumn + 1) / 2 : nodesInSameColumn / 2;

    const auto SIZE = isStackVertical ? WSSIZE.y / nodesInSameColumn : WSSIZE.x / nodesInSameColumn;

    if (RESIZEDELTA != 0 && nodesInSameColumn > 1) {
        if (!ISMARTRESIZING) {
            PNODE->percSize = std::clamp(PNODE->percSize + RESIZEDELTA / SIZE, 0.05, 1.95);
        } else {
            const auto  NODEIT    = std::ranges::find(m_masterNodesData, *PNODE);
            const auto  REVNODEIT = std::ranges::find(m_masterNodesData | std::views::reverse, *PNODE);

            const float totalSize       = isStackVertical ? WSSIZE.y : WSSIZE.x;
            const float minSize         = totalSize / nodesInSameColumn * 0.2;
            const bool  resizePrevNodes = isStackVertical ? (TOP || DISPLAYBOTTOM) && !DISPLAYTOP : (LEFT || DISPLAYRIGHT) && !DISPLAYLEFT;

            int         nodesLeft = 0;
            float       sizeLeft  = 0;
            int         nodeCount = 0;
            // check the sizes of all the nodes to be resized for later calculation
            auto checkNodesLeft = [&sizeLeft, &nodesLeft, orientation, isStackVertical, &nodeCount, PNODE](auto it) {
                if (it.isMaster != PNODE->isMaster || it.workspaceID != PNODE->workspaceID)
                    return;
                nodeCount++;
                if (!it.isMaster && orientation == PLUGIN_ORIENTATION_CENTER && nodeCount % 2 == 1)
                    return;
                sizeLeft += isStackVertical ? it.size.y : it.size.x;
                nodesLeft++;
            };
            float resizeDiff;
            if (resizePrevNodes) {
                std::for_each(std::next(REVNODEIT), m_masterNodesData.rend(), checkNodesLeft);
                resizeDiff = -RESIZEDELTA;
            } else {
                std::for_each(std::next(NODEIT), m_masterNodesData.end(), checkNodesLeft);
                resizeDiff = RESIZEDELTA;
            }

            const float nodeSize        = isStackVertical ? PNODE->size.y : PNODE->size.x;
            const float maxSizeIncrease = sizeLeft - nodesLeft * minSize;
            const float maxSizeDecrease = minSize - nodeSize;

            // leaves enough room for the other nodes
            resizeDiff = std::clamp(resizeDiff, maxSizeDecrease, maxSizeIncrease);
            PNODE->percSize += resizeDiff / SIZE;

            // resize the other nodes
            nodeCount            = 0;
            auto resizeNodesLeft = [maxSizeIncrease, resizeDiff, minSize, orientation, isStackVertical, SIZE, &nodeCount, nodesLeft, PNODE](auto& it) {
                if (it.isMaster != PNODE->isMaster || it.workspaceID != PNODE->workspaceID)
                    return;
                nodeCount++;
                // if center orientation, only resize when on the same side
                if (!it.isMaster && orientation == PLUGIN_ORIENTATION_CENTER && nodeCount % 2 == 1)
                    return;
                const float size               = isStackVertical ? it.size.y : it.size.x;
                const float resizeDeltaForEach = maxSizeIncrease != 0 ? resizeDiff * (size - minSize) / maxSizeIncrease : resizeDiff / nodesLeft;
                it.percSize -= resizeDeltaForEach / SIZE;
            };
            if (resizePrevNodes) {
                std::for_each(std::next(REVNODEIT), m_masterNodesData.rend(), resizeNodesLeft);
            } else {
                std::for_each(std::next(NODEIT), m_masterNodesData.end(), resizeNodesLeft);
            }
        }
    }

    recalculateMonitor(PMONITOR->m_id);

    m_forceWarps = false;
}

void CPluginMasterLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) {
    const auto PMONITOR   = pWindow->m_monitor.lock();
    const auto PWORKSPACE = pWindow->m_workspace;

    // save position and size if floating
    if (pWindow->m_isFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
        pWindow->m_lastFloatingSize     = pWindow->m_realSize->goal();
        pWindow->m_lastFloatingPosition = pWindow->m_realPosition->goal();
        pWindow->m_position             = pWindow->m_realPosition->goal();
        pWindow->m_size                 = pWindow->m_realSize->goal();
    }

    if (EFFECTIVE_MODE == FSMODE_NONE) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            *pWindow->m_realPosition = pWindow->m_lastFloatingPosition;
            *pWindow->m_realSize     = pWindow->m_lastFloatingSize;

            pWindow->unsetWindowData(PRIORITY_LAYOUT);
            pWindow->updateWindowData();
        }
    } else {
        // apply new pos and size being monitors' box
        if (EFFECTIVE_MODE == FSMODE_FULLSCREEN) {
            *pWindow->m_realPosition = PMONITOR->m_position;
            *pWindow->m_realSize     = PMONITOR->m_size;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SPluginMasterNodeData fakeNode;
            fakeNode.pWindow                = pWindow;
            fakeNode.position               = PMONITOR->m_position + PMONITOR->m_reservedTopLeft;
            fakeNode.size                   = PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight;
            fakeNode.workspaceID            = pWindow->workspaceID();
            pWindow->m_position             = fakeNode.position;
            pWindow->m_size                 = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->changeWindowZOrder(pWindow, true);
}

void CPluginMasterLayout::recalculateWindow(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->monitorID());
}

SWindowRenderLayoutHints CPluginMasterLayout::requestRenderHints(PHLWINDOW pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesnt have any hints
}

void CPluginMasterLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
    if (!isDirection(dir))
        return;

    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(pWindow, dir[0]);

    if (!PWINDOW2)
        return;

    pWindow->setAnimationsToMove();

    if (pWindow->m_workspace != PWINDOW2->m_workspace) {
        // if different monitors, send to monitor
        onWindowRemovedTiling(pWindow);
        pWindow->moveToWorkspace(PWINDOW2->m_workspace);
        pWindow->m_monitor = PWINDOW2->m_monitor;
        if (!silent) {
            const auto pMonitor = pWindow->m_monitor.lock();
            g_pCompositor->setActiveMonitor(pMonitor);
        }
        onWindowCreatedTiling(pWindow);
    } else {
        // if same monitor, switch windows
        switchWindows(pWindow, PWINDOW2);
        if (silent)
            g_pCompositor->focusWindow(PWINDOW2);
    }

    pWindow->updateGroupOutputs();
    if (!pWindow->m_groupData.pNextWindow.expired()) {
        PHLWINDOW next = pWindow->m_groupData.pNextWindow.lock();
        while (next != pWindow) {
            next->updateToplevel();
            next = next->m_groupData.pNextWindow.lock();
        }
    }
}

void CPluginMasterLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_monitor, pWindow->m_monitor);
        std::swap(pWindow2->m_workspace, pWindow->m_workspace);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    pWindow->setAnimationsToMove();
    pWindow2->setAnimationsToMove();

    recalculateMonitor(pWindow->monitorID());
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->monitorID());

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);
}

void CPluginMasterLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PMASTER = getMasterNodeOnWorkspace(pWindow->workspaceID());

    float      newRatio = exact ? ratio : PMASTER->percMaster + ratio;
    float      oldPercMaster = PMASTER->percMaster;
    PMASTER->percMaster = std::clamp(newRatio, 0.05f, 0.95f);

    recalculateMonitor(pWindow->monitorID());
}

PHLWINDOW CPluginMasterLayout::getNextWindow(PHLWINDOW pWindow, bool next, bool loop) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    const auto PNODE = getNodeFromWindow(pWindow);

    auto       nodes = m_masterNodesData;
    if (!next)
        std::ranges::reverse(nodes);

    const auto NODEIT = std::ranges::find(nodes, *PNODE);

    const bool ISMASTER = PNODE->isMaster;

    auto CANDIDATE = std::find_if(NODEIT, nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER == other.isMaster && other.workspaceID == PNODE->workspaceID; });
    if (CANDIDATE == nodes.end())
        CANDIDATE = std::ranges::find_if(nodes, [&](const auto& other) { return other != *PNODE && ISMASTER != other.isMaster && other.workspaceID == PNODE->workspaceID; });

    if (CANDIDATE != nodes.end() && !loop) {
        if (CANDIDATE->isMaster && next)
            return nullptr;
        if (!CANDIDATE->isMaster && ISMASTER && !next)
            return nullptr;
    }

    return CANDIDATE == nodes.end() ? nullptr : CANDIDATE->pWindow.lock();
}

std::any CPluginMasterLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    auto switchToWindow = [&](PHLWINDOW PWINDOWTOCHANGETO) {
        if (!validMapped(PWINDOWTOCHANGETO))
            return;

        if (header.pWindow->isFullscreen()) {
            const auto  PWORKSPACE        = header.pWindow->m_workspace;
            const auto  FSMODE            = header.pWindow->m_fullscreenState.internal;
            static auto* const INHERITFULLSCREEN  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:inherit_fullscreen")->getDataStaticPtr();
            int64_t            IINHERITFULLSCREEN = **INHERITFULLSCREEN;
            g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            if (IINHERITFULLSCREEN)
                g_pCompositor->setWindowFullscreenInternal(PWINDOWTOCHANGETO, FSMODE);
        } else {
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->middle());
        }

        g_pInputManager->m_forcedFocus = PWINDOWTOCHANGETO;
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_forcedFocus.reset();
    };

    CVarList vars(message, 0, ' ');

    if (vars.size() < 1 || vars[0].empty()) {
        return 0;
    }

    auto command = vars[0];

    // swapwithmaster <master | child | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master
    // * child - keep the focus at the new child
    // * auto (default) - swap the focus (keep the focus of the previously selected window)
    if (command == "swapwithmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        if (!isWindowTiled(PWINDOW))
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        const auto NEWCHILD = PMASTER->pWindow.lock();

        if (PMASTER->pWindow.lock() != PWINDOW) {
            const auto NEWMASTER       = PWINDOW;
            const bool newFocusToChild = vars.size() >= 2 && vars[1] == "child";
            switchWindows(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
        } else {
            for (auto const& n : m_masterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    const auto NEWMASTER = n.pWindow.lock();
                    switchWindows(NEWMASTER, NEWCHILD);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    const auto NEWFOCUS         = newFocusToMaster ? NEWMASTER : NEWCHILD;
                    switchToWindow(NEWFOCUS);
                    break;
                }
            }
        }

        return 0;
    }
    // focusmaster <master | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master, even if it was focused before
    // * auto (default) - swap the focus with the first child, if the current focus was master, otherwise focus master
    else if (command == "focusmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        if (PMASTER->pWindow.lock() != PWINDOW) {
            switchToWindow(PMASTER->pWindow.lock());
        } else if (vars.size() >= 2 && vars[1] == "master") {
            return 0;
        } else {
            // if master is focused keep master focused (don't do anything)
            for (auto const& n : m_masterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchToWindow(n.pWindow.lock());
                    break;
                }
            }
        }

        return 0;
    } else if (command == "cyclenext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool NOLOOP      = vars.size() >= 2 && vars[1] == "noloop";
        const auto PNEXTWINDOW = getNextWindow(PWINDOW, true, !NOLOOP);
        switchToWindow(PNEXTWINDOW);
    } else if (command == "cycleprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool NOLOOP      = vars.size() >= 2 && vars[1] == "noloop";
        const auto PPREVWINDOW = getNextWindow(PWINDOW, false, !NOLOOP);
        switchToWindow(PPREVWINDOW);
    } else if (command == "swapnext") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating) {
            g_pKeybindManager->m_dispatchers["swapnext"]("");
            return 0;
        }

        const bool NOLOOP            = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, true, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            switchToWindow(header.pWindow);
        }
    } else if (command == "swapprev") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating) {
            g_pKeybindManager->m_dispatchers["swapnext"]("prev");
            return 0;
        }

        const bool NOLOOP            = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, false, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenClient(header.pWindow, FSMODE_NONE);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            switchToWindow(header.pWindow);
        }
    } else if (command == "addmaster") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating)
            return 0;

        const auto  PNODE = getNodeFromWindow(header.pWindow);

        const auto  WINDOWS    = getNodesOnWorkspace(header.pWindow->workspaceID());
        const auto  MASTERS    = getMastersOnWorkspace(header.pWindow->workspaceID());
        static auto* const SMALLSPLIT  = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:allow_small_split")->getDataStaticPtr();
        int64_t            ISMALLSPLIT = **SMALLSPLIT;

        if (MASTERS + 2 > WINDOWS && ISMALLSPLIT == 0)
            return 0;

        g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_masterNodesData) {
                if (n.workspaceID == header.pWindow->workspaceID() && !n.isMaster) {
                    n.isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        recalculateMonitor(header.pWindow->monitorID());

    } else if (command == "removemaster") {

        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->workspaceID());
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->workspaceID());

        if (WINDOWS < 2 || MASTERS < 2)
            return 0;

        g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto& nd : m_masterNodesData | std::views::reverse) {
                if (nd.workspaceID == header.pWindow->workspaceID() && nd.isMaster) {
                    nd.isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }

        recalculateMonitor(header.pWindow->monitorID());
    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());

        if (command == "orientationleft")
            PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_LEFT;
        else if (command == "orientationright")
            PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_RIGHT;
        else if (command == "orientationtop")
            PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_TOP;
        else if (command == "orientationbottom")
            PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_BOTTOM;
        else if (command == "orientationcenter")
            PWORKSPACEDATA->orientation = PLUGIN_ORIENTATION_CENTER;

        recalculateMonitor(header.pWindow->monitorID());

    } else if (command == "orientationnext") {
        runOrientationCycle(header, nullptr, 1);
    } else if (command == "orientationprev") {
        runOrientationCycle(header, nullptr, -1);
    } else if (command == "orientationcycle") {
        runOrientationCycle(header, &vars, 1);
    } else if (command == "mfact") {
        g_pKeybindManager->m_dispatchers["splitratio"](vars[1] + " " + vars[2]);
    } else if (command == "rollnext") {
        const auto PWINDOW = header.pWindow;
        const auto PNODE   = getNodeFromWindow(PWINDOW);

        if (!PNODE)
            return 0;

        const auto OLDMASTER = PNODE->isMaster ? PNODE : getMasterNodeOnWorkspace(PNODE->workspaceID);
        if (!OLDMASTER)
            return 0;

        const auto OLDMASTERIT = std::ranges::find(m_masterNodesData, *OLDMASTER);

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID == PNODE->workspaceID && !nd.isMaster) {
                nd.isMaster            = true;
                const auto NEWMASTERIT = std::ranges::find(m_masterNodesData, nd);
                m_masterNodesData.splice(OLDMASTERIT, m_masterNodesData, NEWMASTERIT);
                switchToWindow(nd.pWindow.lock());
                OLDMASTER->isMaster = false;
                m_masterNodesData.splice(m_masterNodesData.end(), m_masterNodesData, OLDMASTERIT);
                break;
            }
        }

        recalculateMonitor(PWINDOW->monitorID());
    } else if (command == "rollprev") {
        const auto PWINDOW = header.pWindow;
        const auto PNODE   = getNodeFromWindow(PWINDOW);

        if (!PNODE)
            return 0;

        const auto OLDMASTER = PNODE->isMaster ? PNODE : getMasterNodeOnWorkspace(PNODE->workspaceID);
        if (!OLDMASTER)
            return 0;

        const auto OLDMASTERIT = std::ranges::find(m_masterNodesData, *OLDMASTER);

        for (auto& nd : m_masterNodesData | std::views::reverse) {
            if (nd.workspaceID == PNODE->workspaceID && !nd.isMaster) {
                nd.isMaster            = true;
                const auto NEWMASTERIT = std::ranges::find(m_masterNodesData, nd);
                m_masterNodesData.splice(OLDMASTERIT, m_masterNodesData, NEWMASTERIT);
                switchToWindow(nd.pWindow.lock());
                OLDMASTER->isMaster = false;
                m_masterNodesData.splice(m_masterNodesData.begin(), m_masterNodesData, OLDMASTERIT);
                break;
            }
        }

        recalculateMonitor(PWINDOW->monitorID());
    }

    return 0;
}

// If vars is null, we use the default list
void CPluginMasterLayout::runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int direction) {
    std::vector<ePluginOrientation> cycle;
    if (vars != nullptr)
        buildOrientationCycleVectorFromVars(cycle, *vars);

    if (cycle.empty())
        buildOrientationCycleVectorFromEOperation(cycle);

    const auto PWINDOW = header.pWindow;

    if (!PWINDOW)
        return;

    g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());

    int        nextOrPrev = 0;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (PWORKSPACEDATA->orientation == cycle[i]) {
            nextOrPrev = i + direction;
            break;
        }
    }

    if (nextOrPrev >= (int)cycle.size())
        nextOrPrev = nextOrPrev % (int)cycle.size();
    else if (nextOrPrev < 0)
        nextOrPrev = cycle.size() + (nextOrPrev % (int)cycle.size());

    PWORKSPACEDATA->orientation = cycle.at(nextOrPrev);
    recalculateMonitor(header.pWindow->monitorID());
}

void CPluginMasterLayout::buildOrientationCycleVectorFromEOperation(std::vector<ePluginOrientation>& cycle) {
    for (int i = 0; i <= PLUGIN_ORIENTATION_CENTER; ++i) {
        cycle.push_back((ePluginOrientation)i);
    }
}

void CPluginMasterLayout::buildOrientationCycleVectorFromVars(std::vector<ePluginOrientation>& cycle, CVarList& vars) {
    for (size_t i = 1; i < vars.size(); ++i) {
        if (vars[i] == "top") {
            cycle.push_back(PLUGIN_ORIENTATION_TOP);
        } else if (vars[i] == "right") {
            cycle.push_back(PLUGIN_ORIENTATION_RIGHT);
        } else if (vars[i] == "bottom") {
            cycle.push_back(PLUGIN_ORIENTATION_BOTTOM);
        } else if (vars[i] == "left") {
            cycle.push_back(PLUGIN_ORIENTATION_LEFT);
        } else if (vars[i] == "center") {
            cycle.push_back(PLUGIN_ORIENTATION_CENTER);
        }
    }
}

ePluginOrientation CPluginMasterLayout::getDynamicOrientation(PHLWORKSPACE pWorkspace) {
    const auto  WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(pWorkspace);
    std::string orientationString;
    if (WORKSPACERULE.layoutopts.contains("orientation"))
        orientationString = WORKSPACERULE.layoutopts.at("orientation");

    ePluginOrientation orientation = getMasterWorkspaceData(pWorkspace->m_id)->orientation;
    // override if workspace rule is set
    if (!orientationString.empty()) {
        if (orientationString == "top")
            orientation = PLUGIN_ORIENTATION_TOP;
        else if (orientationString == "right")
            orientation = PLUGIN_ORIENTATION_RIGHT;
        else if (orientationString == "bottom")
            orientation = PLUGIN_ORIENTATION_BOTTOM;
        else if (orientationString == "center")
            orientation = PLUGIN_ORIENTATION_CENTER;
        else
            orientation = PLUGIN_ORIENTATION_LEFT;
    }

    return orientation;
}

void CPluginMasterLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE);
}

Vector2D CPluginMasterLayout::predictSizeForNewWindowTiled() {
    static auto* const PNEWSTATUS = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:pluginmaster:new_status")->getDataStaticPtr();
    std::string        SNEWSTATUS = *PNEWSTATUS;

    if (!g_pCompositor->m_lastMonitor)
        return {};

    const int NODES = getNodesOnWorkspace(g_pCompositor->m_lastMonitor->m_activeWorkspace->m_id);

    if (NODES <= 0)
        return g_pCompositor->m_lastMonitor->m_size;

    const auto MASTER = getMasterNodeOnWorkspace(g_pCompositor->m_lastMonitor->m_activeWorkspace->m_id);
    if (!MASTER) // wtf
        return {};

    if (SNEWSTATUS == "master") {
        return MASTER->size;
    } else {
        const auto SLAVES = NODES - getMastersOnWorkspace(g_pCompositor->m_lastMonitor->m_activeWorkspace->m_id);

        // TODO: make this better
        return {g_pCompositor->m_lastMonitor->m_size.x - MASTER->size.x, g_pCompositor->m_lastMonitor->m_size.y / (SLAVES + 1)};
    }

    return {};
}

void CPluginMasterLayout::onEnable() {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_isFloating || !w->m_isMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w);
    }
}

void CPluginMasterLayout::onDisable() {
    m_masterNodesData.clear();
}

void CPluginMasterLayout::removeWorkspaceData(const WORKSPACEID& ws) {
    SPluginMasterWorkspaceData* wsdata = nullptr;
    for (auto& n : m_masterWorkspacesData) {
        if (n.workspaceID == ws)
            wsdata = &n;
    }

    if (wsdata)
        m_masterWorkspacesData.erase(std::remove(m_masterWorkspacesData.begin(), m_masterWorkspacesData.end(), *wsdata), m_masterWorkspacesData.end());
}
