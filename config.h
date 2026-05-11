#pragma once

// ─────────────────────────────────────────────────────
// Debug / Serial logging
// Comment out to disable all Serial output and save ~5 KB flash.
// ─────────────────────────────────────────────────────
// #define DEBUG

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
#define THEME_TEAL      0x0410 // #008080  teal
#define THEME_ORANGE    0xEBA3 // #E87418  orange
#define THEME_SKY       0x055F // #00A8F8  sky blue
#define THEME_PURPLE    0xBA3F // #B844F8  purple
#define THEME_AMBER     0xFD20 // #F8A400  amber
#define THEME_CRIMSON   0xD800 // #D80000  crimson
#define THEME_LIME      0x87E0 // #80FC00  lime
#define THEME_MAGENTA   0xF81F // #F800F8  magenta
#define THEME_INDIGO    0x4A5F // #4848F8  indigo
#define THEME_EMERALD   0x46F1 // #40DE88  emerald

#define COLOR_THEME     THEME_EMERALD

// ─────────────────────────────────────────────────────
// Thumbnail preview
// Built-in PNG decoder (upng) – no external library needed.
// Max compressed image size in bytes.
// 128 KB is enough for a 400x300 PNG thumbnail.
// THUMBNAIL_BG_COLOR: RGB565 colour shown behind/around the image.
// ─────────────────────────────────────────────────────
#define THUMBNAIL_MAX_BYTES   131072   // 128 KB
#define THUMBNAIL_BG_COLOR    0x0000  // same as COLOR_BG (#000000)
// Largest thumbnail dimension (width or height) to consider.
// OrcaSlicer generates 32x32 and 400x300. Set to 200 to prefer
// any ≤200 px thumbnail (e.g. 200x200 from PrusaSlicer).
// If no thumbnail fits within this limit, the largest available is used.
#define THUMBNAIL_MAX_SRC_DIM 400

// ─────────────────────────────────────────────────────
// Language
// Set the desired language code (case-sensitive):
//   "en"     English
//   "pt-BR"  Portuguese (Brazil)
//   "es"     Spanish
//   "fr"     French
//   "de"     German
//   "it"     Italian
//   "ja"     Japanese (ASCII transliteration recommended)
//   "zh-CN"  Chinese (Mandarin, ASCII transliteration recommended)
//   "ru"     Russian (ASCII transliteration recommended)
//   "ko"     Korean (ASCII transliteration recommended)
// ─────────────────────────────────────────────────────
#define LANG "en"

// ─────────────────────────────────────────────────────
// Temperature graph
// Stores recent temperature history as a scrolling line chart.
// TEMP_GRAPH_SAMPLES : number of data points kept (circular buffer).
//   At 1 sample/s: 100 samples ≈ 1.6 min of history.
//   Memory: 4 bytes/sample × 100 × 2 graphs = 800 bytes.
// TEMP_GRAPH_MIN_TEMP / MAX_TEMP : Y-axis range in °C.
// ─────────────────────────────────────────────────────
#define TEMP_GRAPH_SAMPLES  100
#define TEMP_GRAPH_MIN_TEMP   0
#define TEMP_GRAPH_MAX_TEMP 300
