#pragma once
#include "printer_state.h"

// Full-screen overlay shown whenever the printer is offline.
// No interaction is allowed while this screen is visible.
namespace ScreenOffline {
    void resetInitialized();
    void draw(const PrinterState& state);
}
