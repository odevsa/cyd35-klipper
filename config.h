#pragma once

// ─────────────────────────────────────────────────────
// Name
// ─────────────────────────────────────────────────────
#define PROJECT_NAME "KlipperMonitor"

// ─────────────────────────────────────────────────────
// WiFi
// ─────────────────────────────────────────────────────
#define WIFI_SSID     "SSID"
#define WIFI_PASSWORD "PASSWORD"

// ─────────────────────────────────────────────────────
// Moonraker (Klipper REST API)
// ─────────────────────────────────────────────────────
#define MOONRAKER_HOST "192.168.0.100"
#define MOONRAKER_PORT  7125

// ─────────────────────────────────────────────────────
// Polling interval (ms)
// ─────────────────────────────────────────────────────
#define UPDATE_INTERVAL_MS 1000

// ─────────────────────────────────────────────────────
// Hardware – keep in sync with User_Setup.h
// ─────────────────────────────────────────────────────
#define TFT_BL_PIN      27
#define TOUCH_PRESSURE 600

// ─────────────────────────────────────────────────────
// Color theme – choose ONE accent colour
// ─────────────────────────────────────────────────────
#define THEME_CYAN    0x0555 // #00AAAA  cyan
#define THEME_ORANGE  0xEBA3 // #E87818  orange
#define THEME_BLUE    0x055F // #00AAFF  sky blue
#define THEME_GREEN   0x46F1 // #44DD88  mint green
#define THEME_PURPLE  0xBA3F // #BB44FF  purple

#define COLOR_THEME  THEME_CYAN
