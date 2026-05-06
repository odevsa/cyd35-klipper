#pragma once
#include "printer_state.h"
#include "moonraker.h"

// ─────────────────────────────────────────────────────
// Available screens
// ─────────────────────────────────────────────────────
enum class ScreenID : uint8_t {
    HOME = 0,
    PRINT,
    TEMPS,
    MOVE
};

// ─────────────────────────────────────────────────────
// Central coordinator: status bar, nav bar, and
// delegation to the active screen module.
// ─────────────────────────────────────────────────────
class ScreenManager {
public:
    void begin();

    // Call every loop() – draws only when dirty
    void draw(const PrinterState& state);

    // Call from loop() on touch event
    void handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client);

    // Force a full redraw on the next draw() call
    void invalidate();

    ScreenID currentScreen() const { return _screen; }

private:
    ScreenID      _screen       = ScreenID::HOME;
    bool          _contentDirty = true;
    bool          _navDirty     = true;
    // Status bar cache
    PrinterStatus _prevStatus    = PrinterStatus::DISCONNECTED;
    bool          _prevConnected = false;

    void _switchTo(ScreenID id);
    void _drawStatusBar(const PrinterState& state);
    void _drawNavBar();
    bool _navBarTouch(int x, int y);
};
