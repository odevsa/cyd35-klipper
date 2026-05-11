#pragma once
#include "printer_state.h"

// ─────────────────────────────────────────────────────
// File browser entry (directory or .gcode file)
// ─────────────────────────────────────────────────────
#define FM_MAX_ENTRIES  24
#define FM_NAME_LEN     64

struct FileEntry {
    char name[FM_NAME_LEN];  // filename or dirname (no path prefix)
    bool isDir;
    long sizeBytes;          // 0 for directories
};

// ─────────────────────────────────────────────────────
// File metadata returned by getFileInfo()
// ─────────────────────────────────────────────────────
struct FileMetadata {
    bool  valid          = false;
    char  thumbUrl[200]  = "";
    long  sizeBytes      = 0;     // file size in bytes
    int   estimatedTime  = 0;     // seconds (0 = unknown)
    float filamentMM     = 0.0f;  // total extrusion length (mm)
    float filamentGrams  = 0.0f;  // filament weight (grams)
    int   nozzleTemp     = 0;     // first_layer_extr_temp (°C)
    int   bedTemp        = 0;     // first_layer_bed_temp (°C)
    char  material[24]   = "";    // filament type (e.g. "PLA")
};

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

    // Restart firmware – sends FIRMWARE_RESTART
    bool firmwareRestart();

    // ── File browser ─────────────────────────────────

    // List contents of a gcodes sub-directory (pass "" for root).
    // Directories are listed first, then .gcode files.
    // Returns true on success; count holds the number of entries filled.
    bool listDirectory(const char* path, FileEntry* out, int maxOut, int& count);

    // Fetch file metadata (print time, temps, filament) AND thumbnail URL in
    // one HTTP request.  Returns true if the API call succeeded.
    bool getFileInfo(const char* filepath, FileMetadata& meta);

    // Start printing a gcodes-relative file path.
    bool startPrint(const char* filepath);

private:
    const char* _host;
    int         _port;
    char        _prevFilename[80] = "";   // tracks filename for thumbnail re-fetch

    // Build base URL prefix
    String _url(const char* path) const;

    // POST with optional JSON body; returns HTTP status code
    int _post(const char* path, const char* body = nullptr);

    // Parse the Moonraker objects/query response into state
    bool _parseObjects(const String& payload, PrinterState& state);

    // Fetch the best-available thumbnail path from file metadata and
    // store the full URL in state.thumbnailPath.
    void _fetchThumbnailPath(PrinterState& state);
};
