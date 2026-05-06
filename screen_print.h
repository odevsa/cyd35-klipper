#pragma once
#include "screen_manager.h"
#include "printer_state.h"
#include "moonraker.h"

// Print job overview: progress, time, and job controls
namespace ScreenPrint {
    void      resetInitialized();
    void      draw(const PrinterState& state);
    ScreenID  handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client);
}
