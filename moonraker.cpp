#include "moonraker.h"
#include "config.h"
#ifdef DEBUG
#include <Arduino.h>
#endif
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────
MoonrakerClient::MoonrakerClient(const char* host, int port)
    : _host(host), _port(port) {}

// ─────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────
String MoonrakerClient::_url(const char* path) const {
    return String("http://") + _host + ":" + _port + path;
}

int MoonrakerClient::_post(const char* path, const char* body) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(_url(path));

    int code = -1;
    if (body) {
        http.addHeader("Content-Type", "application/json");
        code = http.POST(String(body));
    } else {
        code = http.POST("");
    }
    http.end();
    return code;
}

// ─────────────────────────────────────────────────────
// JSON parsing
// ─────────────────────────────────────────────────────
bool MoonrakerClient::_parseObjects(const String& payload, PrinterState& state) {
    // 8 KB – toolhead + print_stats JSON can exceed 4 KB when fields are broad
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
#ifdef DEBUG
        Serial.printf("[Moonraker] JSON error: %s\n", err.c_str());
#endif
        return false;
    }

    JsonObject s = doc["result"]["status"];

    // Temperatures
    state.extruderTemp   = s["extruder"]["temperature"]  | 0.0f;
    state.extruderTarget = s["extruder"]["target"]        | 0.0f;
    state.bedTemp        = s["heater_bed"]["temperature"] | 0.0f;
    state.bedTarget      = s["heater_bed"]["target"]      | 0.0f;

    // Fan
    state.fanSpeed = s["fan"]["speed"] | 0.0f;

    // Print stats
    const char* fname = s["print_stats"]["filename"];
    if (fname) strlcpy(state.filename, fname, sizeof(state.filename));
    else        state.filename[0] = '\0';

    // print_duration is a float in Moonraker; cast explicitly to avoid ArduinoJson
    // returning 0 when using the int-defaulted '| 0' form on a float variant.
    state.printDuration = (int)s["print_stats"]["print_duration"].as<float>();

    const char* ps = s["print_stats"]["state"];
    if (ps) {
        if      (strcmp(ps, "printing")  == 0) state.status = PrinterStatus::PRINTING;
        else if (strcmp(ps, "paused")    == 0) state.status = PrinterStatus::PAUSED;
        else if (strcmp(ps, "complete")  == 0) state.status = PrinterStatus::COMPLETE;
        else if (strcmp(ps, "error")     == 0) state.status = PrinterStatus::ERROR;
        else if (strcmp(ps, "standby")   == 0) state.status = PrinterStatus::STANDBY;
        else                                   state.status = PrinterStatus::IDLE;
    } else {
        state.status = PrinterStatus::IDLE;
    }

    // Progress and estimated total
    state.progress = s["virtual_sdcard"]["progress"].as<float>();
    if (state.status == PrinterStatus::COMPLETE) {
        state.progress = 1.0f;
    }
    if (state.progress > 0.005f && state.printDuration > 0) {
        state.totalDuration = (int)((float)state.printDuration / state.progress);
    } else {
        state.totalDuration = 0;
    }

    // Toolhead position [x, y, z, e]
    JsonArray pos = s["toolhead"]["position"];
    if (pos.size() >= 3) {
        state.posX = pos[0];
        state.posY = pos[1];
        state.posZ = pos[2];
    }

    // Display message
    const char* msg = s["display_status"]["message"];
    if (msg) strlcpy(state.message, msg, sizeof(state.message));
    else     state.message[0] = '\0';

    state.connected = true;
    return true;
}

// ─────────────────────────────────────────────────────
// URL-encode a string (query-parameter safe)
// ─────────────────────────────────────────────────────
static String _urlEncode(const char* str) {
    String out;
    out.reserve(strlen(str) + 16);
    for (const char* p = str; *p; ++p) {
        uint8_t c = (uint8_t)*p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────
// Fetch thumbnail URL from Moonraker file metadata
// ─────────────────────────────────────────────────────
void MoonrakerClient::_fetchThumbnailPath(PrinterState& state) {
    state.thumbnailPath[0] = '\0';

    String query = _url("/server/files/metadata?filename=")
                   + _urlEncode(state.filename);

#ifdef DEBUG
    Serial.printf("[Thumb] metadata query: %s\n", query.c_str());
#endif

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(query);
    int code = http.GET();

#ifdef DEBUG
    Serial.printf("[Thumb] metadata HTTP code: %d\n", code);
#endif

    if (code != 200) {
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

#ifdef DEBUG
    Serial.printf("[Thumb] metadata payload (%d bytes): %.300s\n",
        payload.length(), payload.c_str());
#endif

    DynamicJsonDocument doc(4096);
    DeserializationError derr = deserializeJson(doc, payload);
    if (derr != DeserializationError::Ok) {
#ifdef DEBUG
        Serial.printf("[Thumb] metadata JSON error: %s\n", derr.c_str());
#endif
        return;
    }

    JsonArray thumbs = doc["result"]["thumbnails"].as<JsonArray>();
    if (!thumbs || thumbs.size() == 0) {
#ifdef DEBUG
        Serial.println("[Thumb] no thumbnails in metadata");
#endif
        return;
    }

#ifdef DEBUG
    Serial.printf("[Thumb] found %d thumbnail(s):  freeHeap=%u  maxAlloc=%u\n",
        thumbs.size(), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
#endif

    // Peak memory during streaming decode (zlib mode):
    //   IDAT compressed bytes  (= size field from metadata)
    //   + 2 × row stride       (= 2 × (1 + width × 4) worst-case RGBA)
    //   + zlib internal state  (~40 KB: inflate_state struct + 32KB window)
    // For 400×300: 8KB + 3.2KB + 40KB ≈ 51KB – fits in 110KB maxAlloc.
    static const size_t ZLIB_OVERHEAD = 42 * 1024;  // conservative estimate
    const char* bestPath = nullptr;
    int bestW = 0;
    for (JsonObject t : thumbs) {
        int tw = t["width"]  | 0;
        int th = t["height"] | 0;
        int ts = t["size"]   | 0;  // compressed bytes
        const char* rp = t["relative_path"] | "";
        bool withinDim = (tw <= THUMBNAIL_MAX_SRC_DIM && th <= THUMBNAIL_MAX_SRC_DIM);
        size_t peak = (size_t)ts + 2 * (1 + (size_t)tw * 4) + ZLIB_OVERHEAD;
        size_t maxAlloc = (size_t)ESP.getMaxAllocHeap();
        bool fits = peak <= maxAlloc * 4 / 5;
#ifdef DEBUG
        Serial.printf("[Thumb]   %dx%d size=%d peak=%u fits=%s dim=%s path=%s\n",
                      tw, th, ts, (unsigned)peak,
                      fits ? "yes" : "no",
                      withinDim ? "ok" : "over", rp);
#endif
        // Prefer largest thumbnail within the configured max dimension.
        // Fall back: keep track of best "oversized" thumbnail separately.
        if (*rp && tw > 0 && th > 0 && fits && withinDim && tw > bestW) {
            bestW    = tw;
            bestPath = rp;
        }
    }

    // If nothing fit within THUMBNAIL_MAX_SRC_DIM, pick the largest that at
    // least fits in memory (ignoring the dimension cap as a fallback).
    if (!bestPath || bestPath[0] == '\0') {
        for (JsonObject t : thumbs) {
            int tw = t["width"]  | 0;
            int th = t["height"] | 0;
            int ts = t["size"]   | 0;
            const char* rp = t["relative_path"] | "";
            size_t peak = (size_t)ts + 2 * (1 + (size_t)tw * 4) + ZLIB_OVERHEAD;
            size_t maxAlloc = (size_t)ESP.getMaxAllocHeap();
            if (*rp && tw > 0 && th > 0 && peak <= maxAlloc * 4 / 5 && tw > bestW) {
                bestW    = tw;
                bestPath = rp;
            }
        }
    }
    if (!bestPath || bestPath[0] == '\0') {
#ifdef DEBUG
        Serial.println("[Thumb] no thumbnail fits in available heap - using placeholder");
#endif
        return;
    }

    // The relative_path is relative to the directory containing the gcode file.
    // Prepend the gcode file's directory so subdirectory layouts work correctly.
    // e.g. filename="prints/test.gcode", relative_path=".thumbs/test-400x300.png"
    //      → gcodes/prints/.thumbs/test-400x300.png
    String gcodeDir = "";
    const char* slash = strrchr(state.filename, '/');
    if (slash) {
        gcodeDir = String(state.filename).substring(0, slash - state.filename + 1);
    }

    String url = String("http://") + _host + ":" + _port
                 + "/server/files/gcodes/" + gcodeDir + bestPath;
    strlcpy(state.thumbnailPath, url.c_str(), sizeof(state.thumbnailPath));
#ifdef DEBUG
    Serial.printf("[Thumb] selected URL: %s\n", state.thumbnailPath);
#endif
}

// ─────────────────────────────────────────────────────
// Public interface
// ─────────────────────────────────────────────────────
bool MoonrakerClient::update(PrinterState& state) {
    if (WiFi.status() != WL_CONNECTED) {
        state.connected = false;
        state.status    = PrinterStatus::DISCONNECTED;
        return false;
    }

    // Request only the specific fields we need – keeps the JSON payload small
    // (~1 KB instead of ~4 KB) and avoids DynamicJsonDocument overflow.
    const char* query =
        "/printer/objects/query"
        "?extruder=temperature,target"
        "&heater_bed=temperature,target"
        "&print_stats=filename,print_duration,state,message"
        "&toolhead=position"
        "&fan=speed"
        "&virtual_sdcard=progress"
        "&display_status=message";

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(_url(query));
    int code = http.GET();

    if (code != 200) {
#ifdef DEBUG
        Serial.printf("[Moonraker] GET failed: %d\n", code);
#endif
        http.end();
        state.connected = false;
        state.status    = PrinterStatus::DISCONNECTED;
        return false;
    }

    String payload = http.getString();
    http.end();

    bool ok = _parseObjects(payload, state);

    // Re-fetch thumbnail metadata only when the filename changes
    if (ok && strncmp(state.filename, _prevFilename, sizeof(_prevFilename)) != 0) {
        strlcpy(_prevFilename, state.filename, sizeof(_prevFilename));
        if (state.filename[0]) {
            _fetchThumbnailPath(state);
        } else {
            state.thumbnailPath[0] = '\0';
        }
    }

    return ok;
}

bool MoonrakerClient::sendGCode(const char* script) {
    // Escape backslashes and quotes to build safe JSON
    String body = "{\"script\":\"";
    for (const char* p = script; *p; ++p) {
        if (*p == '"')       body += "\\\"";
        else if (*p == '\\') body += "\\\\";
        else if (*p == '\n') body += "\\n";
        else                 body += *p;
    }
    body += "\"}";
    int code = _post("/printer/gcode/script", body.c_str());
    return code == 200;
}

bool MoonrakerClient::pausePrint() {
    return _post("/printer/print/pause") == 200;
}

bool MoonrakerClient::resumePrint() {
    return _post("/printer/print/resume") == 200;
}

bool MoonrakerClient::cancelPrint() {
    return _post("/printer/print/cancel") == 200;
}

bool MoonrakerClient::emergencyStop() {
    return sendGCode("EMERGENCY_STOP");
}

bool MoonrakerClient::firmwareRestart() {
    return sendGCode("FIRMWARE_RESTART");
}

// ─────────────────────────────────────────────────────
// File browser helpers
// ─────────────────────────────────────────────────────

bool MoonrakerClient::listDirectory(const char* path, FileEntry* out,
                                    int maxOut, int& count) {
    count = 0;
    if (WiFi.status() != WL_CONNECTED) return false;

    // Build path: always rooted at "gcodes"
    String fullPath = "gcodes";
    if (path && path[0]) {
        fullPath += '/';
        fullPath += String(path);
    }

    String query = _url("/server/files/directory?path=")
                   + _urlEncode(fullPath.c_str());

    HTTPClient http;
    http.setTimeout(8000);
    http.begin(query);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;

    JsonObject result = doc["result"];

    // Directories first (sorted by the server already)
    for (JsonObject d : result["dirs"].as<JsonArray>()) {
        if (count >= maxOut) break;
        const char* name = d["dirname"] | "";
        if (!*name) continue;
        strlcpy(out[count].name, name, FM_NAME_LEN);
        out[count].isDir     = true;
        out[count].sizeBytes = 0;
        count++;
    }

    // Then .gcode / .gc files
    for (JsonObject f : result["files"].as<JsonArray>()) {
        if (count >= maxOut) break;
        const char* name = f["filename"] | "";
        if (!*name) continue;
        size_t nlen = strlen(name);
        bool isGcode = (nlen > 6 && strcasecmp(name + nlen - 6, ".gcode") == 0);
        bool isGc    = (nlen > 3 && strcasecmp(name + nlen - 3, ".gc")    == 0);
        if (!isGcode && !isGc) continue;
        strlcpy(out[count].name, name, FM_NAME_LEN);
        out[count].isDir     = false;
        out[count].sizeBytes = (long)f["size"].as<long>();
        count++;
    }

    return true;
}

bool MoonrakerClient::getFileInfo(const char* filepath, FileMetadata& meta) {
    meta = FileMetadata();  // zero-init
    if (WiFi.status() != WL_CONNECTED) return false;

    String query = _url("/server/files/metadata?filename=") + _urlEncode(filepath);

    HTTPClient http;
    http.setTimeout(6000);
    http.begin(query);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(6144);
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;

    JsonObject result = doc["result"];

    meta.sizeBytes     = (long)result["size"].as<long>();
    meta.estimatedTime = (int)result["estimated_time"].as<float>();
    meta.filamentMM    = result["filament_total"].as<float>();
    meta.filamentGrams = result["filament_weight_total"].as<float>();
    meta.nozzleTemp    = (int)result["first_layer_extr_temp"].as<float>();
    meta.bedTemp       = (int)result["first_layer_bed_temp"].as<float>();

    const char* mat = result["filament_type"] | "";
    if (!*mat) mat  = result["material"] | "";
    strlcpy(meta.material, mat, sizeof(meta.material));

    // ── Thumbnail selection (same logic as _fetchThumbnailPath) ──────
    static const size_t ZLIB_OVH = 42 * 1024;
    JsonArray thumbs = result["thumbnails"].as<JsonArray>();
    const char* bestPath = nullptr;
    int bestW = 0;

    if (thumbs) {
        for (JsonObject t : thumbs) {
            int tw = t["width"]  | 0;
            int ts = t["size"]   | 0;
            const char* rp = t["relative_path"] | "";
            if (!*rp || tw <= 0 || ts > THUMBNAIL_MAX_BYTES) continue;
            size_t peak = (size_t)ts + 2u * (1u + (size_t)tw * 4u) + ZLIB_OVH;
            if (peak > (size_t)ESP.getMaxAllocHeap() * 4 / 5) continue;
            if (tw > bestW) { bestW = tw; bestPath = rp; }
        }
    }

    if (bestPath && *bestPath) {
        const char* slash = strrchr(filepath, '/');
        String gcodeDir;
        if (slash) gcodeDir = String(filepath).substring(0, slash - filepath + 1);
        String url = String("http://") + _host + ":" + _port
                     + "/server/files/gcodes/" + gcodeDir + String(bestPath);
        strlcpy(meta.thumbUrl, url.c_str(), sizeof(meta.thumbUrl));
    }

    meta.valid = true;
    return true;
}

bool MoonrakerClient::startPrint(const char* filepath) {
    // Build JSON body with basic escaping
    String body = "{\"filename\":\"";
    for (const char* p = filepath; *p; ++p) {
        if      (*p == '"')  body += "\\\"";
        else if (*p == '\\') body += "\\\\";
        else                 body += *p;
    }
    body += "\"}";
    return _post("/printer/print/start", body.c_str()) == 200;
}
