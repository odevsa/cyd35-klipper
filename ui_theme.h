#pragma once
#include <TFT_eSPI.h>
#include "printer_state.h"
#include "config.h"

// ─────────────────────────────────────────────────────
// Display geometry (landscape, rotation = 1)
// ─────────────────────────────────────────────────────
#define SCREEN_W        480
#define SCREEN_H        320

#define STATUS_BAR_H     30
#define NAV_BAR_H        55
#define NAV_BAR_Y       (SCREEN_H - NAV_BAR_H)          // 265
#define CONTENT_Y        STATUS_BAR_H                    //  30
#define CONTENT_H       (SCREEN_H - STATUS_BAR_H - NAV_BAR_H)  // 235
#define CONTENT_BOTTOM  (NAV_BAR_Y)                      // 265

// ─────────────────────────────────────────────────────
// Colour palette (KlipperScreen dark-theme inspired)
// All values are RGB-565
// ─────────────────────────────────────────────────────
#define COLOR_BG        0x0841   // #101010  background
#define COLOR_SURFACE   0x18C3   // #181818  card / panel

// Accent colour – driven by COLOR_THEME defined in config.h
#define COLOR_ACCENT  COLOR_THEME

#define COLOR_SUCCESS   0x0604   // #008808  green
#define COLOR_WARNING   0xFD20   // #FFA000  amber
#define COLOR_ERROR     0xF800   // #FF0000  red
#define COLOR_TEXT      0xFFFF   // #FFFFFF  primary text
#define COLOR_TEXT_DIM  0x8410   // #808080  secondary text
#define COLOR_NAV_BG    0x10A2   // #101420  nav background
#define COLOR_NAV_ACT   0x2945   // #294428  active nav item
#define COLOR_DIVIDER   0x2965   // #294CA8  separator lines

// ─────────────────────────────────────────────────────
// Navigation bar – 4 equal sections
// ─────────────────────────────────────────────────────
#define NAV_BTN_W       (SCREEN_W / 4)   // 120 px
#define NAV_BTN_H        NAV_BAR_H

// ─────────────────────────────────────────────────────
// Global TFT object – defined once in cyd.ino
// ─────────────────────────────────────────────────────
extern TFT_eSPI tft;

// ─────────────────────────────────────────────────────
// Shared inline drawing helpers
// ─────────────────────────────────────────────────────

// Filled rounded-rect button with centred label
inline void drawButton(int x, int y, int w, int h,
                       const char* label,
                       uint16_t bgColor, uint16_t textColor,
                       uint8_t font = 2) {
    tft.fillRoundRect(x, y, w, h, 6, bgColor);
    tft.drawRoundRect(x, y, w, h, 6, COLOR_DIVIDER);
    tft.setTextFont(font);
    tft.setTextColor(textColor, bgColor);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x + w / 2, y + h / 2);
}

// Horizontal progress bar
inline void drawProgressBar(int x, int y, int w, int h,
                            float progress,
                            uint16_t fgColor, uint16_t bgColor) {
    tft.fillRoundRect(x, y, w, h, h / 2, bgColor);
    int filled = (int)(w * constrain(progress, 0.0f, 1.0f));
    if (filled > h) {
        tft.fillRoundRect(x, y, filled, h, h / 2, fgColor);
    }
    tft.drawRoundRect(x, y, w, h, h / 2, COLOR_DIVIDER);
}

// Status badge (colour changes with state)
inline uint16_t statusColor(PrinterStatus s) {
    switch (s) {
        case PrinterStatus::PRINTING:  return COLOR_SUCCESS;
        case PrinterStatus::PAUSED:    return COLOR_WARNING;
        case PrinterStatus::ERROR:     return COLOR_ERROR;
        case PrinterStatus::COMPLETE:  return COLOR_ACCENT;
        default:                       return COLOR_TEXT_DIM;
    }
}

// Format seconds → "1h 23m" or "45m 12s"
inline String formatTime(int seconds) {
    if (seconds <= 0) return "--";
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    char buf[16];
    if (h > 0) snprintf(buf, sizeof(buf), "%dh %02dm", h, m);
    else        snprintf(buf, sizeof(buf), "%dm %02ds", m, s);
    return String(buf);
}

// Truncate a long filename for display
inline void truncateFilename(const char* src, char* dst, size_t maxChars) {
    size_t len = strlen(src);
    if (len <= maxChars) {
        strncpy(dst, src, maxChars);
        dst[maxChars] = '\0';
    } else {
        strncpy(dst, src, maxChars - 3);
        dst[maxChars - 3] = '\0';
        strncat(dst, "...", 4);
    }
}
