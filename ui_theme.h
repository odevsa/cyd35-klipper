#pragma once
#include <TFT_eSPI.h>
#include "printer_state.h"
#include "config.h"

// ─────────────────────────────────────────────────────
// Display geometry (landscape, rotation = 1)
// ─────────────────────────────────────────────────────
#define SCREEN_W            480
#define SCREEN_H            320

#define PANEL_SPACING       8
#define PANEL_ROUNDED       5
#define STATUS_BAR_H        30
#define NAV_BAR_H           55
#define NAV_BAR_Y           (SCREEN_H - NAV_BAR_H)
#define CONTENT_Y           STATUS_BAR_H
#define CONTENT_H           (SCREEN_H - STATUS_BAR_H - NAV_BAR_H)
#define CONTENT_CENTER_X    (SCREEN_W / 2)
#define DIVIDER_CENTER_Y    (CONTENT_Y + (NAV_BAR_Y - CONTENT_Y) / 2)
#define CONTENT_BOTTOM      (NAV_BAR_Y)
#define BUTTON_SMALL_H      24
#define BUTTON_MEDIUM_H     38
#define BUTTON_LARGE_H      44


// ─────────────────────────────────────────────────────
// Colour palette (KlipperScreen dark-theme inspired)
// All values are RGB-565
// ─────────────────────────────────────────────────────
#define COLOR_BG            0x0841   // #080808  background
#define COLOR_SURFACE       0x18C3   // #181818  card / panel

// Accent colour – driven by COLOR_THEME defined in config.h
#define COLOR_ACCENT        COLOR_THEME

#define COLOR_SUCCESS       0x0604   // #00C020  green
#define COLOR_WARNING       0xFD20   // #F8A400  amber
#define COLOR_ERROR         0xF800   // #F80000  red
#define COLOR_TEXT          0xFFFF   // #F8F8F8  primary text
#define COLOR_TEXT_DIM      0x8410   // #808080  secondary text
#define COLOR_TEXT_MUTED    0x3186   // #303030  muted / secondary
#define COLOR_NAV_BG        0x1082   // #101010  nav background
#define COLOR_NAV_ACT       0x2945   // #292929  active nav item
#define COLOR_DIVIDER       0x2104   // #202020  separator lines

// ─────────────────────────────────────────────────────
// Navigation bar – 4 equal sections
// ─────────────────────────────────────────────────────
#define NAV_BTN_W           (SCREEN_W / 4)   // 120 px
#define NAV_BTN_H           NAV_BAR_H

// ─────────────────────────────────────────────────────
// Global TFT object – defined once in cyd35-klipper.ino
// ─────────────────────────────────────────────────────
extern TFT_eSPI tft;

// ─────────────────────────────────────────────────────
// Shared inline drawing helpers
// ─────────────────────────────────────────────────────

// Hit-test: returns true when (px, py) is inside the rect [x, y, x+w, y+h).
inline bool inRect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

// Filled rounded-rect button with centred label.
inline void drawButton(int x, int y, int w, int h,
                       const char* label,
                       uint16_t bgColor, uint16_t textColor,
                       uint8_t font = 2) {
    tft.fillRoundRect(x, y, w, h, PANEL_ROUNDED, bgColor);
    tft.drawRoundRect(x, y, w, h, PANEL_ROUNDED, COLOR_DIVIDER);
    tft.setTextFont(font);
    tft.setTextColor(textColor);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, x + (w / 2), y + h / 2);
    tft.drawString(label, x + (w / 2) + 1, y + h / 2);
}

// Horizontal progress bar
inline void drawProgressBar(int x, int y, int w, int h,
                            float progress,
                            uint16_t fgColor, uint16_t bgColor) {
    tft.fillRoundRect(x, y, w, h, PANEL_ROUNDED, bgColor);
    int filled = (int)(w * constrain(progress, 0.0f, 1.0f));
    if (filled > 0) {
        tft.fillRoundRect(x, y, filled, h, PANEL_ROUNDED, fgColor);
    }
    tft.drawRoundRect(x, y, w, h, PANEL_ROUNDED, COLOR_DIVIDER);
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

// Sanitize a directory path for display
inline const char* sanitizeDirPath(const char* path) {
    static char buf[128];
    if (!path || path[0] == '\0') {
        buf[0] = '/'; buf[1] = '\0';
        return buf;
    }

    // copy into buffer with truncation protection
    size_t len = strlen(path);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, path, len);
    buf[len] = '\0';

    // ensure leading slash
    if (buf[0] != '/') {
        // shift right if space allows
        if (len + 1 < sizeof(buf)) {
            memmove(buf + 1, buf, len + 1);
            buf[0] = '/';
        } else {
            // fallback: replace first char
            buf[0] = '/';
        }
    }

    // keep directory portion only (up to and including last '/')
    char* last = strrchr(buf, '/');
    if (last) {
        size_t idx = (size_t)(last - buf);
        buf[idx + 1] = '\0';
    } else {
        // should not happen, but ensure we return '/'
        buf[0] = '/'; buf[1] = '\0';
    }

    return buf;
}

inline uint16_t darkenRGB565(uint16_t color, float percentage) {
    uint32_t r = (color & 0xF800) >> 11;
    uint32_t g = (color & 0x07E0) >> 5;
    uint32_t b = (color & 0x001F);

    r = (uint32_t)((float)r * percentage);
    g = (uint32_t)((float)g * percentage);
    b = (uint32_t)((float)b * percentage);

    if (r > 0x1F) r = 0x1F;
    if (g > 0x3F) g = 0x3F;
    if (b > 0x1F) b = 0x1F;

    return (uint16_t)((r << 11) | (g << 5) | b);
}

#ifdef DEBUG
// Convert the raw uint16_t value stored by readRect into a packed RGB-888.
// readRect byte-swaps each pixel for pushRect compatibility, so we undo that
// first, then decode the resulting standard RGB-565 value.
inline uint32_t rgb565toRGB888(uint16_t raw) {
    uint16_t c = (raw << 8) | (raw >> 8);   // undo readRect byte-swap
    uint8_t r = ((c >> 11) & 0x1F) << 3;
    uint8_t g = ((c >>  5) & 0x3F) << 2;
    uint8_t b = ( c        & 0x1F) << 3;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Dumps the current display as hex-encoded raw readRect pixels over Serial.
//
// Protocol (before SCREENSHOT header):
//   "PIXEL_VERIFY: raw=0xXXXX decoded=0xXXXX R=NNN G=NNN B=NNN\n"
// Main protocol:
//   "SCREENSHOT:WxH\n", one row of W×4 hex chars per line, then "END\n".
//
// readRect stores each pixel byte-swapped for pushRect compatibility.
// Python undoes the swap and decodes as standard RGB-565 → RGB-888.
//
// If colours are still wrong after lowering SPI_READ_FREQUENCY to 6 MHz
// in User_Setup.h, the MISO line may have a hardware issue.
inline void screenshotToSerialMonitor() {
    static const char NIBBLE[] = "0123456789ABCDEF";
    static uint16_t pixBuf[SCREEN_W];
    static char     hexBuf[SCREEN_W * 4 + 2];  // 4 hex chars + '\n' + '\0'

    // Deassert touch CS so the XPT2046 tri-states its MISO output.
    // Both controllers share MISO=12; leaving TOUCH_CS low would cause
    // bus contention during TFT SPI reads.
    digitalWrite(TOUCH_CS, HIGH);
    delayMicroseconds(10);

    // ── Diagnostic: pixel (10, 15) – solid status-bar fill, no text ──────────
    {
        uint16_t rr;
        tft.readRect(10, 15, 1, 1, &rr);
        // readRect stores byte-swapped; undo to get the original RGB-565.
        uint16_t c = (uint16_t)((rr << 8) | (rr >> 8));
        uint8_t dr = ((c >> 11) & 0x1F) << 3;
        uint8_t dg = ((c >>  5) & 0x3F) << 2;
        uint8_t db = ( c        & 0x1F) << 3;
        Serial.printf(
            "PIXEL_VERIFY: raw=0x%04X decoded=0x%04X R=%d G=%d B=%d\n",
            rr, c, dr, dg, db);
        Serial.flush();
    }

    Serial.printf("SCREENSHOT:%dx%d\n", SCREEN_W, SCREEN_H);

    for (int32_t y = 0; y < SCREEN_H; y++) {
        tft.readRect(0, y, SCREEN_W, 1, pixBuf);
        char* p = hexBuf;
        for (int32_t x = 0; x < SCREEN_W; x++) {
            uint16_t c = pixBuf[x];
            *p++ = NIBBLE[(c >> 12) & 0xF];
            *p++ = NIBBLE[(c >>  8) & 0xF];
            *p++ = NIBBLE[(c >>  4) & 0xF];
            *p++ = NIBBLE[(c >>  0) & 0xF];
        }
        *p++ = '\n';
        *p   = '\0';
        // Send in 256-byte chunks to avoid TX-buffer stalls at high baud rates.
        const uint8_t* src = (const uint8_t*)hexBuf;
        size_t rem = (size_t)(SCREEN_W * 4 + 1);  // hex chars + '\n'
        while (rem > 0) {
            size_t chunk = (rem > 256) ? 256u : rem;
            Serial.write(src, chunk);
            src += chunk;
            rem -= chunk;
        }
    }

    Serial.println("END");
    Serial.flush();
}
#endif
