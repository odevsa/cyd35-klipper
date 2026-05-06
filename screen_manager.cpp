#include "config.h"
#include "screen_manager.h"
#include "ui_theme.h"
#include "screen_home.h"
#include "screen_print.h"
#include "screen_temps.h"
#include "screen_move.h"

// ─────────────────────────────────────────────────────
// Status bar (y=0..29) – only changed fields are redrawn
// ─────────────────────────────────────────────────────
void ScreenManager::_drawStatusBar(const PrinterState& state) {
    // ── App title (static, drawn every call – same pixels, no flicker) ──
    // Double-draw at x=8 and x=9 to simulate bold.
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_ACCENT, COLOR_NAV_BG);   // opaque – clears bg first
    tft.drawString(PROJECT_NAME, 8,  STATUS_BAR_H / 2);
    tft.setTextColor(COLOR_ACCENT);                  // transparent – overlay +1px
    tft.drawString(PROJECT_NAME, 9,  STATUS_BAR_H / 2);

    // ── Status text (centre) – only if changed ───────
    if (state.status != _prevStatus) {
        // fillRect clears any leftover from a longer previous label
        tft.fillRect(SCREEN_W / 2 - 65, 2, 130, STATUS_BAR_H - 4, COLOR_NAV_BG);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(statusColor(state.status), COLOR_NAV_BG);
        tft.drawString(statusLabel(state.status), SCREEN_W / 2, STATUS_BAR_H / 2);
        _prevStatus = state.status;
    }

    // ── Connected indicator (right) – only if changed ─
    if (state.connected != _prevConnected) {
        tft.fillRect(SCREEN_W - 98, 2, 92, STATUS_BAR_H - 4, COLOR_NAV_BG);
        tft.setTextFont(2);
        tft.setTextDatum(MR_DATUM);
        if (state.connected) {
            tft.setTextColor(COLOR_SUCCESS, COLOR_NAV_BG);
            tft.drawString("CONNECTED", SCREEN_W - 6, STATUS_BAR_H / 2);
            tft.setTextColor(COLOR_SUCCESS);
            tft.drawString("CONNECTED", SCREEN_W - 5, STATUS_BAR_H / 2);
        } else {
            tft.setTextColor(COLOR_ERROR, COLOR_NAV_BG);
            tft.drawString("OFFLINE", SCREEN_W - 6, STATUS_BAR_H / 2);
            tft.setTextColor(COLOR_ERROR);
            tft.drawString("OFFLINE", SCREEN_W - 5, STATUS_BAR_H / 2);
        }
        _prevConnected = state.connected;
    }
}

// ─────────────────────────────────────────────────────
// Navigation bar (y=265..319)
// 4 buttons: HOME | PRINT | TEMPS | MOVE
// ─────────────────────────────────────────────────────
static const char* NAV_LABELS[4] = { "HOME", "PRINT", "TEMPS", "MOVE" };

void ScreenManager::_drawNavBar() {
    tft.fillRect(0, NAV_BAR_Y, SCREEN_W, NAV_BAR_H, COLOR_NAV_BG);

    for (int i = 0; i < 4; i++) {
        int x = i * NAV_BTN_W;
        bool active = ((int)_screen == i);
        uint16_t bg  = active ? COLOR_NAV_ACT : COLOR_NAV_BG;
        uint16_t txt = active ? COLOR_ACCENT   : COLOR_TEXT_DIM;
        tft.fillRect(x, NAV_BAR_Y, NAV_BTN_W, NAV_BTN_H, bg);
        tft.fillRect(x, NAV_BAR_Y, NAV_BTN_W, 4, active ? COLOR_ACCENT : COLOR_NAV_BG);
        
        // Vertical separator
        if (i > 0) {
            tft.drawFastVLine(x, NAV_BAR_Y, NAV_BTN_H, COLOR_DIVIDER);
        }
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(txt, bg);
        tft.drawString(NAV_LABELS[i],
                       x + NAV_BTN_W / 2, NAV_BAR_Y + NAV_BTN_H / 2);
    }
}

// ─────────────────────────────────────────────────────
// Returns true if the touch landed on the nav bar
// ─────────────────────────────────────────────────────
bool ScreenManager::_navBarTouch(int x, int y) {
    if (y < NAV_BAR_Y) return false;
    int idx = x / NAV_BTN_W;
    if (idx < 0 || idx > 3) return false;
    ScreenID tapped = (ScreenID)idx;
    if (tapped != _screen) _switchTo(tapped);
    return true;
}

// ─────────────────────────────────────────────────────
// Screen switching
// ─────────────────────────────────────────────────────
void ScreenManager::_switchTo(ScreenID id) {
    _screen        = id;
    _contentDirty  = true;
    _navDirty      = true;
    // Reset status bar cache so title and indicators redraw after the area clear
    _prevStatus    = PrinterStatus::DISCONNECTED;
    _prevConnected = !_prevConnected; // force mismatch
    // Clear content area once so residual pixels from the old screen vanish
    // instantly, without flashing the whole display.
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_BG);
    // Tell the new screen to rebuild its static layout on the next draw()
    switch (id) {
        case ScreenID::HOME:  ScreenHome::resetInitialized();  break;
        case ScreenID::PRINT: ScreenPrint::resetInitialized(); break;
        case ScreenID::TEMPS: ScreenTemps::resetInitialized(); break;
        case ScreenID::MOVE:  ScreenMove::resetInitialized();  break;
    }
}

// ─────────────────────────────────────────────────────
// Public interface
// ─────────────────────────────────────────────────────
void ScreenManager::begin() {
    _screen        = ScreenID::HOME;
    _contentDirty  = true;
    _navDirty      = true;
    _prevStatus    = PrinterStatus::DISCONNECTED;
    _prevConnected = false;
    tft.fillScreen(COLOR_BG);
    // Draw status bar background once (static area)
    tft.fillRect(0, 0, SCREEN_W, STATUS_BAR_H, COLOR_NAV_BG);
}

void ScreenManager::invalidate() {
    // Called after a data poll – only content + status bar need refresh.
    // Nav bar is static between screen switches; skip it to avoid flash.
    _contentDirty = true;
}

void ScreenManager::draw(const PrinterState& state) {
    if (!_contentDirty && !_navDirty) return;

    // Nav bar: drawn only on screen switch or first boot
    if (_navDirty) {
        _drawNavBar();
        _navDirty = false;
    }

    // Content + status bar: drawn on every data update
    if (_contentDirty) {
        _contentDirty = false;
        _drawStatusBar(state);
        switch (_screen) {
            case ScreenID::HOME:  ScreenHome::draw(state);  break;
            case ScreenID::PRINT: ScreenPrint::draw(state); break;
            case ScreenID::TEMPS: ScreenTemps::draw(state); break;
            case ScreenID::MOVE:  ScreenMove::draw(state);  break;
        }
    }
}

void ScreenManager::handleTouch(int x, int y,
                                PrinterState& state,
                                MoonrakerClient& client) {
    if (_navBarTouch(x, y)) {
        return;
    }

    // Delegate to active screen; it may request a navigation change
    ScreenID next = _screen;
    switch (_screen) {
        case ScreenID::HOME:  next = ScreenHome::handleTouch(x, y, state, client);  break;
        case ScreenID::PRINT: next = ScreenPrint::handleTouch(x, y, state, client); break;
        case ScreenID::TEMPS: next = ScreenTemps::handleTouch(x, y, state, client); break;
        case ScreenID::MOVE:  next = ScreenMove::handleTouch(x, y, state, client);  break;
    }

    if (next != _screen) {
        _switchTo(next);
    } else {
        _contentDirty = true;   // redraw content after any interaction
    }
}
