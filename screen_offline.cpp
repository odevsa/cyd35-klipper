#include "screen_offline.h"
#include "lang.h"
#include "ui_theme.h"

static bool s_initialized = false;

void ScreenOffline::resetInitialized() {
    s_initialized = false;
}

void ScreenOffline::draw(const PrinterState& /*state*/) {
    if (s_initialized) return;
    s_initialized = true;

    // ── Background ────────────────────────────────────
    tft.fillScreen(COLOR_BG);

    // ── Icon area: large red circle with "!" ─────────
    const int cx = SCREEN_W / 2;
    const int cy = SCREEN_H / 2 - 40;
    const int r  = 38;
    tft.fillCircle(cx, cy, r, COLOR_ERROR);
    tft.fillCircle(cx, cy, r - 3, 0x6000);   // dark red interior
    // "!" bar
    tft.fillRoundRect(cx - 4, cy - 18, 8, 22, 3, COLOR_ERROR);
    // "!" dot
    tft.fillCircle(cx, cy + 12, 4, COLOR_ERROR);

    // ── Title ─────────────────────────────────────────
    tft.setTextFont(4);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_ERROR, COLOR_BG);
    tft.drawString(L.MSG_OFFLINE_TITLE, cx, SCREEN_H / 2 + 20);

    // ── Subtitle ─────────────────────────────────────────
    tft.setTextFont(2);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString(L.MSG_OFFLINE_SUB,  cx, SCREEN_H / 2 + 52);
    tft.drawString(L.MSG_OFFLINE_WAIT, cx, SCREEN_H / 2 + 72);
}
