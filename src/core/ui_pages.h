#pragma once
#include "precomp.h"

namespace eth::ui::pages {

enum class Page {
    Mods = 0,
    Interfaces,
    Inspector,
    Settings,
    Console,
    About,
    Patch, // Append: existing saved page IDs remain unchanged.
    Count
};

Page CurrentPage();
void SetCurrentPage(Page page);

// Left navigation. In launcher mode the bottom shows Start/Exit buttons.
void DrawSidebar(float width, bool launcherMode, bool* startGame, bool* exitApp);

// Right content area for the given page.
void DrawContent(const ImVec2& avail, bool launcherMode, Page page);
// Shared responsive shell for launcher, overlay and off-screen previews.
void DrawManagerLayout(bool launcherMode, bool* startGame = nullptr, bool* exitApp = nullptr);

// Always-on game watermark (drawn by the overlay).
void DrawWatermark();

// Small status strip (used at the bottom of the launcher content area).
void DrawStatusStrip();

} // namespace eth::ui::pages
