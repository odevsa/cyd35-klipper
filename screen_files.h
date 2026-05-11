#pragma once
#include "screen_manager.h"
#include "printer_state.h"
#include "moonraker.h"

// ─────────────────────────────────────────────────────
// File-manager overlay: browse gcodes folders, preview
// a selected file and start printing.
// ─────────────────────────────────────────────────────
namespace ScreenFiles {

    // Called by ScreenManager when switching to FILES.
    void resetInitialized();

    // Remember which screen to return to on close/print.
    void setPreviousScreen(ScreenID prev);

    // Render – redraws only when dirty flags are set.
    void draw(const PrinterState& state);

    // Touch-down / drag handler – returns next ScreenID.
    ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client);

    // Touch-up handler – fires the action on release (tap-to-open).
    ScreenID handleRelease(PrinterState& state, MoonrakerClient& client);

} // namespace ScreenFiles
