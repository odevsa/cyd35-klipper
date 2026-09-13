#pragma once
#include "screen_manager.h"
#include "../core/printer_state.h"
#include "../core/moonraker.h"

// Dashboard: temperatures (left) + print status (right)
namespace ScreenHome {
    void      resetInitialized();
    void      draw(const PrinterState& state);
    ScreenID  handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client);
}
