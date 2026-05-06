#pragma once
#include "printer_state.h"

// ─────────────────────────────────────────────────────
// HTTP client for Moonraker REST API
// Docs: https://moonraker.readthedocs.io/en/latest/web_api/
// ─────────────────────────────────────────────────────
class MoonrakerClient {
public:
    MoonrakerClient(const char* host, int port);

    // Poll all printer objects and fill state. Returns true on success.
    bool update(PrinterState& state);

    // Raw G-code execution (e.g. "G28", "M104 S215")
    bool sendGCode(const char* script);

    // Print job control
    bool pausePrint();
    bool resumePrint();
    bool cancelPrint();

    // Emergency stop – sends EMERGENCY_STOP
    bool emergencyStop();

    // Restart firmware – sends EMERGENCY_STOP
    bool firmwareRestart();

private:
    const char* _host;
    int         _port;

    // Build base URL prefix
    String _url(const char* path) const;

    // POST with optional JSON body; returns HTTP status code
    int _post(const char* path, const char* body = nullptr);

    // Parse the Moonraker objects/query response into state
    bool _parseObjects(const String& payload, PrinterState& state);
};
