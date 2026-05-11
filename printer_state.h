#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────
// Printer lifecycle states (mirrors Klipper print_stats)
// ─────────────────────────────────────────────────────
enum class PrinterStatus : uint8_t {
    DISCONNECTED = 0,
    IDLE,
    STANDBY,
    PRINTING,
    PAUSED,
    COMPLETE,
    ERROR
};

inline const char* statusLabel(PrinterStatus s) {
    switch (s) {
        case PrinterStatus::IDLE:         return "IDLE";
        case PrinterStatus::STANDBY:      return "STANDBY";
        case PrinterStatus::PRINTING:     return "PRINTING";
        case PrinterStatus::PAUSED:       return "PAUSED";
        case PrinterStatus::COMPLETE:     return "COMPLETE";
        case PrinterStatus::ERROR:        return "ERROR";
        default:                          return "OFFLINE";
    }
}

// ─────────────────────────────────────────────────────
// All data polled from Moonraker
// ─────────────────────────────────────────────────────
struct PrinterState {
    bool          connected      = false;
    PrinterStatus status         = PrinterStatus::DISCONNECTED;

    // Temperatures
    float extruderTemp    = 0.0f;
    float extruderTarget  = 0.0f;
    float bedTemp         = 0.0f;
    float bedTarget       = 0.0f;

    // Cooling fan (0.0 – 1.0)
    float fanSpeed        = 0.0f;

    // Active print
    char  filename[80]    = "";
    float progress        = 0.0f;   // 0.0 – 1.0
    int   printDuration   = 0;      // elapsed seconds
    int   totalDuration   = 0;      // estimated total seconds (0 = unknown)

    // Toolhead position
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;

    // Display message from Klipper
    char message[80] = "";

    // Thumbnail URL for the current print job.
    // Populated by MoonrakerClient::update() when filename changes.
    // Empty string means no thumbnail is available.
    char thumbnailPath[160] = "";
};
