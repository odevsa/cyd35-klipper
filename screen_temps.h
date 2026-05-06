#pragma once
#include "screen_manager.h"
#include "printer_state.h"
#include "moonraker.h"

// Hotend + bed temperature adjustment with presets
namespace ScreenTemps {
    void      resetInitialized();
    void      draw(const PrinterState& state);
    ScreenID  handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client);
}
