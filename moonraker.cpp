#include "moonraker.h"
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
        Serial.print("[Moonraker] JSON error: ");
        Serial.println(err.c_str());
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
    if (state.progress > 0.005f && state.printDuration > 0) {
        state.totalDuration = (int)((float)state.printDuration / state.progress);
    } else {
        state.totalDuration = 0;
    }

    // Debug – visible in Serial Monitor (115200 baud)
    Serial.printf("[DBG] dur=%d  progress=%.4f  total=%d  rem=%d\n",
                  state.printDuration, state.progress,
                  state.totalDuration,
                  (state.totalDuration > state.printDuration
                       ? state.totalDuration - state.printDuration : 0));

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
        Serial.printf("[Moonraker] GET failed: %d\n", code);
        http.end();
        state.connected = false;
        state.status    = PrinterStatus::DISCONNECTED;
        return false;
    }

    String payload = http.getString();
    http.end();

    return _parseObjects(payload, state);
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
