#pragma once
#include "screen_manager.h"
#include "../core/printer_state.h"
#include "../core/moonraker.h"

// Manual axis movement + step-size selector
namespace ScreenMove {
    void      resetInitialized();
    void      draw(const PrinterState& state);
    ScreenID  handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client);
}
