// ─────────────────────────────────────────────────────
// Klipper Monitor for CYD (Cheap Yellow Display)
// Display  : ST7796  480×320  (landscape, rotation=1)
// Touch    : XPT2046 resistive
// API      : Moonraker REST (HTTP polling)
// Libraries: TFT_eSPI, ArduinoJson 6.x, WiFi, HTTPClient
// ─────────────────────────────────────────────────────
#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>

#include "config.h"
#include "printer_state.h"
#include "ui_theme.h"        // also declares extern TFT_eSPI tft
#include "moonraker.h"
#include "screen_manager.h"

// ── Global singletons ────────────────────────────────
// tft is declared extern in ui_theme.h; defined here.
TFT_eSPI        tft       = TFT_eSPI();

PrinterState    g_state;
MoonrakerClient g_moonraker(MOONRAKER_HOST, MOONRAKER_PORT);
ScreenManager   g_screenMgr;

static unsigned long s_lastPoll = 0;

// ─────────────────────────────────────────────────────
// WiFi helpers
// ─────────────────────────────────────────────────────
static void showConnecting() {
    tft.fillScreen(COLOR_BG);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.drawString("Connecting to WiFi...", SCREEN_W / 2, SCREEN_H / 2 - 20);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString(WIFI_SSID, SCREEN_W / 2, SCREEN_H / 2 + 8);
}

static void connectWiFi() {
    showConnecting();
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(500);
        // Animate dots to show activity
        static int dots = 0;
        char buf[8];
        snprintf(buf, sizeof(buf), "%s%s%s",
                 dots >= 1 ? "." : " ",
                 dots >= 2 ? "." : " ",
                 dots >= 3 ? "." : " ");
        tft.setTextColor(COLOR_ACCENT, COLOR_BG);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(buf, SCREEN_W / 2, SCREEN_H / 2 + 40);
        dots = (dots + 1) % 4;
    }

    if (WiFi.status() != WL_CONNECTED) {
        tft.setTextColor(COLOR_ERROR, COLOR_BG);
        tft.drawString("WiFi failed - running offline", SCREEN_W / 2, SCREEN_H / 2 + 60);
        delay(2000);
    }
}

static void ensureWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
    }
}

// ─────────────────────────────────────────────────────
// Arduino entry points
// ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Backlight (keep identical to original project)
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // Display (keep identical to original project)
    tft.init();
    tft.setRotation(1);   // landscape

    connectWiFi();

    // First data fetch
    g_moonraker.update(g_state);
    s_lastPoll = millis();

    g_screenMgr.begin();
}

void loop() {
    // ── Periodic Moonraker poll ──────────────────────
    if (millis() - s_lastPoll >= UPDATE_INTERVAL_MS) {
        ensureWiFi();
        g_moonraker.update(g_state);
        s_lastPoll = millis();
        g_screenMgr.invalidate();
    }

    // ── Touch handling ──────────────────────────────
    uint16_t tx, ty;
    if (tft.getTouch(&tx, &ty, TOUCH_PRESSURE)) {
        g_screenMgr.handleTouch((int)tx, (int)ty, g_state, g_moonraker);
        delay(150);   // simple debounce
    }

    // ── Render ──────────────────────────────────────
    g_screenMgr.draw(g_state);
}