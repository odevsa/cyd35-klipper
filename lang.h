#pragma once
// ─────────────────────────────────────────────────────
// Multi-language support
//
// Set the active language in config.h:
//   #define LANG "pt-BR"
//
// Available codes:  en  pt-BR  es  fr  de  it  ja  zh-CN  ru  ko
//
// Non-Latin scripts (zh-CN, ja, ru, ko) use ASCII transliteration
// because TFT_eSPI fonts only cover the Latin-1 range.
// ─────────────────────────────────────────────────────

#include "printer_state.h"

struct LangStrings {
    // Navigation bar
    const char* NAV_HOME;
    const char* NAV_PRINT;
    const char* NAV_TEMPS;
    const char* NAV_MOVE;
    // Printer status labels
    const char* ST_IDLE;
    const char* ST_STANDBY;
    const char* ST_PRINTING;
    const char* ST_PAUSED;
    const char* ST_COMPLETE;
    const char* ST_ERROR;
    const char* ST_OFFLINE;
    // Status bar
    const char* CONNECTED;
    const char* OFFLINE;
    // Section labels
    const char* LBL_HOTEND;
    const char* LBL_BED;
    const char* LBL_ELAPSED;
    const char* LBL_REMAINING;
    const char* LBL_HEIGHT;
    const char* LBL_FAN;
    // Buttons
    const char* BTN_PAUSE;
    const char* BTN_RESUME;
    const char* BTN_CANCEL;
    const char* BTN_ESTOP;
    const char* BTN_RESTART;
    const char* BTN_HOME_ALL;
    const char* BTN_HOME_XY;
    const char* BTN_HOME_Z;
    // Misc labels
    const char* NO_FILE;
    const char* NO_PRINT_JOB;
    const char* NO_PREVIEW;
    const char* PRESET_OFF;
    // Offline screen
    const char* MSG_OFFLINE_TITLE;
    const char* MSG_OFFLINE_SUB;
    const char* MSG_WIFI_CONNECTING;
    const char* MSG_WIFI_FAILED;
    const char* MSG_OFFLINE_WAIT;
    // File manager
    const char* LBL_FILES;
    const char* BTN_PRINT;
    const char* MSG_PRINT_CONFIRM;
    const char* LBL_CHOOSE_FILE;  // standby icon hint
    const char* LBL_FILE_SIZE;
    const char* LBL_PRINT_TIME;
    const char* LBL_FILAMENT;
    const char* LBL_MATERIAL;
    const char* LBL_LOADING;
};

// Defined in lang.cpp; selected at startup from config.h LANG string.
extern const LangStrings L;

// ─────────────────────────────────────────────────────
// Helper: localized status label for the status bar
// ─────────────────────────────────────────────────────
inline const char* localStatusLabel(PrinterStatus s) {
    switch (s) {
        case PrinterStatus::IDLE:         return L.ST_IDLE;
        case PrinterStatus::STANDBY:      return L.ST_STANDBY;
        case PrinterStatus::PRINTING:     return L.ST_PRINTING;
        case PrinterStatus::PAUSED:       return L.ST_PAUSED;
        case PrinterStatus::COMPLETE:     return L.ST_COMPLETE;
        case PrinterStatus::ERROR:        return L.ST_ERROR;
        default:                          return L.ST_OFFLINE;
    }
}
