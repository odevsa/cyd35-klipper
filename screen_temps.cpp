#include "screen_temps.h"
#include "ui_theme.h"

#define TP_LEFT_X    0
#define TP_RIGHT_X  240
#define TP_PW       235
#define TP_LBL_Y   (CONTENT_Y +  8)
#define TP_TMP_Y   (CONTENT_Y + 26)
#define TP_ADJ_Y   (CONTENT_Y + 80)
#define TP_PRE_Y   (CONTENT_Y + 135)
#define TP_ADJ_H    44
#define TP_PRE_H    38
#define TP_MINUS_W  55
#define TP_PLUS_W   55
#define TP_VAL_W   (TP_PW - TP_MINUS_W - TP_PLUS_W - 6)
#define TP_PR_W    (TP_PW / 4)

static bool _inBox(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool s_initialized = false;

// Per-field cache at display precision (%.0f → int)
static struct {
    int  extruderTemp   = -9999;
    int  extruderTarget = -9999;
    bool extruderAtTemp = false;
    int  bedTemp        = -9999;
    int  bedTarget      = -9999;
    bool bedAtTemp      = false;
} s_prev;

// ─────────────────────────────────────────────────────
// Static layout – drawn once per activation
// ─────────────────────────────────────────────────────
static void drawStaticPanel(int ox, const char* label) {
    tft.fillRect(ox, CONTENT_Y, TP_PW, CONTENT_H, COLOR_SURFACE);
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    tft.drawString(label, ox + 6, TP_LBL_Y + 8);

    tft.setTextFont(2);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.setCursor(ox + 6, TP_TMP_Y + 50);
    tft.print("C");

    drawButton(ox + 3,
               TP_ADJ_Y, TP_MINUS_W, TP_ADJ_H, "- 5", COLOR_BG, COLOR_TEXT, 2);
    drawButton(ox + 3 + TP_MINUS_W + 3 + TP_VAL_W + 3,
               TP_ADJ_Y, TP_PLUS_W, TP_ADJ_H, "+ 5", COLOR_BG, COLOR_TEXT, 2);

    const char* labels[4] = { "PLA", "PETG", "ABS", "OFF" };
    for (int i = 0; i < 4; i++) {
        drawButton(ox + 3 + i * TP_PR_W, TP_PRE_Y,
                   TP_PR_W - 3, TP_PRE_H, labels[i], COLOR_BG, COLOR_TEXT, 2);
    }
}

// ─────────────────────────────────────────────────────
// Dynamic – draws a single panel's changing values only
// if they differ from the cache.
// ─────────────────────────────────────────────────────
static void drawDynamicPanel(int ox, float current, float target,
                              int& prevCurrent, int& prevTarget, bool& prevAtTemp) {
    int  curI  = (int)current;
    int  tgtI  = (int)target;
    bool atTemp = (current >= target - 2 && target > 0);
    char buf[12];

    if (curI != prevCurrent || atTemp != prevAtTemp) {
        snprintf(buf, sizeof(buf), "%3d", curI);
        tft.setTextFont(6);
        tft.setTextColor(atTemp ? COLOR_SUCCESS : COLOR_TEXT, COLOR_SURFACE);
        tft.setCursor(ox + 6, TP_TMP_Y);
        tft.print(buf);
        prevCurrent = curI;
        prevAtTemp  = atTemp;
    }

    if (tgtI != prevTarget) {
        // Redraw target value box only
        int bx = ox + 3 + TP_MINUS_W + 3;
        tft.fillRoundRect(bx, TP_ADJ_Y, TP_VAL_W, TP_ADJ_H, 4, COLOR_BG);
        tft.drawRoundRect(bx, TP_ADJ_Y, TP_VAL_W, TP_ADJ_H, 4, COLOR_DIVIDER);
        snprintf(buf, sizeof(buf), "%3d C", tgtI);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString(buf, bx + TP_VAL_W / 2, TP_ADJ_Y + TP_ADJ_H / 2);
        prevTarget = tgtI;
    }
}

// ─────────────────────────────────────────────────────
// Public
// ─────────────────────────────────────────────────────
namespace ScreenTemps {

void resetInitialized() {
    s_initialized = false;
    s_prev.extruderTemp   = -9999;
    s_prev.extruderTarget = -9999;
    s_prev.extruderAtTemp = false;
    s_prev.bedTemp        = -9999;
    s_prev.bedTarget      = -9999;
    s_prev.bedAtTemp      = false;
}

void draw(const PrinterState& state) {
    if (!s_initialized) {
        tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_BG);
        tft.drawFastVLine(TP_RIGHT_X - 1, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);
        drawStaticPanel(TP_LEFT_X,  "HOTEND");
        drawStaticPanel(TP_RIGHT_X, "BED");
        s_initialized = true;
    }
    drawDynamicPanel(TP_LEFT_X,  state.extruderTemp, state.extruderTarget,
                     s_prev.extruderTemp, s_prev.extruderTarget, s_prev.extruderAtTemp);
    drawDynamicPanel(TP_RIGHT_X, state.bedTemp,       state.bedTarget,
                     s_prev.bedTemp,      s_prev.bedTarget,      s_prev.bedAtTemp);
}

ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client) {
    const int extruderPresets[4] = { 215, 235, 250, 0 };
    const int bedPresets[4]      = {  60,  75, 110, 0 };

    auto sendBoth = [&](int e, int b) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "M104 S%d\nM140 S%d", e, b);
        client.sendGCode(cmd);
    };

    if (x < TP_RIGHT_X) {
        if (_inBox(x, y, TP_LEFT_X + 3, TP_ADJ_Y, TP_MINUS_W, TP_ADJ_H)) {
            int t = max(0, (int)state.extruderTarget - 5);
            state.extruderTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M104 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        int plusX = TP_LEFT_X + 3 + TP_MINUS_W + 3 + TP_VAL_W + 3;
        if (_inBox(x, y, plusX, TP_ADJ_Y, TP_PLUS_W, TP_ADJ_H)) {
            int t = min(300, (int)state.extruderTarget + 5);
            state.extruderTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M104 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        for (int i = 0; i < 4; i++) {
            if (_inBox(x, y, TP_LEFT_X + 3 + i * TP_PR_W, TP_PRE_Y, TP_PR_W - 3, TP_PRE_H)) {
                state.extruderTarget = extruderPresets[i];
                state.bedTarget      = bedPresets[i];
                sendBoth(extruderPresets[i], bedPresets[i]);
                return ScreenID::TEMPS;
            }
        }
    }

    if (x >= TP_RIGHT_X) {
        int ox = TP_RIGHT_X;
        if (_inBox(x, y, ox + 3, TP_ADJ_Y, TP_MINUS_W, TP_ADJ_H)) {
            int t = max(0, (int)state.bedTarget - 5);
            state.bedTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M140 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        int plusX = ox + 3 + TP_MINUS_W + 3 + TP_VAL_W + 3;
        if (_inBox(x, y, plusX, TP_ADJ_Y, TP_PLUS_W, TP_ADJ_H)) {
            int t = min(130, (int)state.bedTarget + 5);
            state.bedTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M140 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        for (int i = 0; i < 4; i++) {
            if (_inBox(x, y, ox + 3 + i * TP_PR_W, TP_PRE_Y, TP_PR_W - 3, TP_PRE_H)) {
                state.bedTarget      = bedPresets[i];
                state.extruderTarget = extruderPresets[i];
                sendBoth(extruderPresets[i], bedPresets[i]);
                return ScreenID::TEMPS;
            }
        }
    }

    return ScreenID::TEMPS;
}

} // namespace ScreenTemps
