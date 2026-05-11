#include "screen_temps.h"
#include "lang.h"
#include "ui_theme.h"
#include "temp_graph.h"

static const int DIVIDER_X =                CONTENT_CENTER_X;
static const int TOP_INNER_Y =              STATUS_BAR_H + PANEL_SPACING;
static const int BOTTOM_INNER_Y =           NAV_BAR_Y - (BUTTON_LARGE_H * 2) - (PANEL_SPACING * 4);
static const int HALF_BOX_W =               DIVIDER_X - (PANEL_SPACING * 2);
static const int HALF_BOX_H =               DIVIDER_CENTER_Y - STATUS_BAR_H - (PANEL_SPACING * 2);
static const int BOX_H =                    CONTENT_H - (PANEL_SPACING * 2);
static const int LEFT_CONTENT_X =           PANEL_SPACING;
static const int RIGHT_CONTENT_X =          DIVIDER_X + PANEL_SPACING;
static const int RIGHT_CONTENT_FINAL_X =    SCREEN_W - PANEL_SPACING;
static const int TARGET_BUTTON_W =          (DIVIDER_X - (PANEL_SPACING * 5)) / 4;
static const int TARGET_VALUE_W =           (TARGET_BUTTON_W * 2) + PANEL_SPACING;
static const int TEMPERATURE_HISTORIC_W =   140;
static const int TEMPERATURE_HISTORIC_H =   BOTTOM_INNER_Y - PANEL_SPACING - TOP_INNER_Y;
static const int MATERIAL_PRESET_Y =        BOTTOM_INNER_Y + BUTTON_LARGE_H + PANEL_SPACING;

static const int TEMP_LABEL_Y =             TOP_INNER_Y + 7;
static const int TEMP_VALUE_Y =             TOP_INNER_Y + (TEMPERATURE_HISTORIC_H / 2) - 20;
static const int TEMP_UNIT_Y =              TOP_INNER_Y + TEMPERATURE_HISTORIC_H - 18;

// Pre-computed graph X positions: right-aligned within each column.
static const int LEFT_GRAPH_X  =            DIVIDER_X - TEMPERATURE_HISTORIC_W - PANEL_SPACING;
static const int RIGHT_GRAPH_X =            SCREEN_W - TEMPERATURE_HISTORIC_W - PANEL_SPACING;

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
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    tft.drawString(label, ox, TEMP_LABEL_Y);

    tft.setTextFont(2);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.setCursor(ox, TEMP_UNIT_Y);
    tft.print("C");

    drawButton(ox, BOTTOM_INNER_Y, TARGET_BUTTON_W, BUTTON_LARGE_H, "- 5", COLOR_ACCENT, COLOR_BG, 2);
    drawButton(ox + TARGET_BUTTON_W + TARGET_VALUE_W + (PANEL_SPACING * 2),
               BOTTOM_INNER_Y, TARGET_BUTTON_W, BUTTON_LARGE_H, "+ 5", COLOR_ACCENT, COLOR_BG, 2);

    const char* labels[4] = { "PLA", "PETG", "ABS", L.PRESET_OFF };

    int currentPosition = ox;
    for (int i = 0; i < 4; i++) {
        drawButton(currentPosition, MATERIAL_PRESET_Y,
                   TARGET_BUTTON_W, BUTTON_LARGE_H, labels[i], COLOR_TEXT_DIM, COLOR_SURFACE, 2);
        currentPosition += TARGET_BUTTON_W + PANEL_SPACING;
    }
}

// ─────────────────────────────────────────────────────
// Dynamic – draws a single panel's changing values only
// if they differ from the cache.
// ─────────────────────────────────────────────────────
static void drawDynamicPanel(int ox, int graphX, float current, float target,
                              int& prevCurrent, int& prevTarget, bool& prevAtTemp,
                              TempGraph& graph, uint16_t lineCol, uint16_t tgtCol) {
    int  curI  = (int)current;
    int  tgtI  = (int)target;
    bool atTemp = (current >= target - 2 && target > 0);
    char buf[12];
    
    // ── Current temperature ──────────────────────────
    if (curI != prevCurrent || atTemp != prevAtTemp) {
        snprintf(buf, sizeof(buf), "%3d", curI);
        tft.setTextFont(6);
        tft.setTextColor(atTemp ? COLOR_SUCCESS : COLOR_TEXT, COLOR_SURFACE);
        tft.setCursor(ox, TEMP_VALUE_Y);
        tft.print(buf);
        prevCurrent = curI;
        prevAtTemp  = atTemp;
    }

    // ── Target temperature ───────────────────────────
    if (tgtI != prevTarget) {
        // Redraw target value box only
        int bx = ox + TARGET_BUTTON_W + PANEL_SPACING;
        tft.fillRoundRect(bx, BOTTOM_INNER_Y, TARGET_VALUE_W, BUTTON_LARGE_H, PANEL_ROUNDED, COLOR_BG);
        tft.drawRoundRect(bx, BOTTOM_INNER_Y, TARGET_VALUE_W, BUTTON_LARGE_H, PANEL_ROUNDED, COLOR_DIVIDER);
        snprintf(buf, sizeof(buf), "%3d C", tgtI);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_BG);
        tft.drawString(buf, bx + TARGET_VALUE_W / 2, BOTTOM_INNER_Y + BUTTON_LARGE_H / 2 + 3);
        prevTarget = tgtI;
    }

    // ── Temperature history ──────────────────────────
    graph.push(current, target);
    graph.draw(graphX, TOP_INNER_Y,
            TEMPERATURE_HISTORIC_W, TEMPERATURE_HISTORIC_H, lineCol, tgtCol);
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
    // g_hotendGraph and g_bedGraph are global – history is preserved across screen switches.
}

void draw(const PrinterState& state) {
    if (!s_initialized) {
        tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_SURFACE);
        tft.drawFastVLine(SCREEN_W / 2, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);
        drawStaticPanel(LEFT_CONTENT_X,  L.LBL_HOTEND);
        drawStaticPanel(RIGHT_CONTENT_X, L.LBL_BED);
        s_initialized = true;
    }

    drawDynamicPanel(LEFT_CONTENT_X,  LEFT_GRAPH_X,  state.extruderTemp, state.extruderTarget,
                     s_prev.extruderTemp, s_prev.extruderTarget, s_prev.extruderAtTemp,
                     g_hotendGraph, TGRAPH_HOT_LINE, TGRAPH_HOT_TGT);
    drawDynamicPanel(RIGHT_CONTENT_X, RIGHT_GRAPH_X, state.bedTemp,       state.bedTarget,
                     s_prev.bedTemp,      s_prev.bedTarget,      s_prev.bedAtTemp,
                     g_bedGraph, TGRAPH_BED_LINE, TGRAPH_BED_TGT);
}

ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client) {
    const int extruderPresets[4] = { 215, 235, 250, 0 };
    const int bedPresets[4]      = {  60,  75, 110, 0 };

    auto sendBoth = [&](int e, int b) {
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "M104 S%d\nM140 S%d", e, b);
        client.sendGCode(cmd);
    };

    auto adjustTemp = [](int target, int current, int delta, int minVal, int maxVal) -> int {
        int base = (target > 0) ? target : (int)(round((float)current / 5.0f) * 5);
        return max(minVal, min(maxVal, base + delta));
    };

    // ── Left panel buttons targets ───────────────────
    if (x < RIGHT_CONTENT_X) {
        int startX = LEFT_CONTENT_X;
        if (inRect(x, y, startX, BOTTOM_INNER_Y, TARGET_BUTTON_W, BUTTON_LARGE_H)) {
            int t = adjustTemp((int)state.extruderTarget, (int)state.extruderTemp, -5, 0, 300);
            state.extruderTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M104 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        // Hotend plus button
        int plusX = startX + TARGET_BUTTON_W + PANEL_SPACING + TARGET_VALUE_W + PANEL_SPACING;
        if (inRect(x, y, plusX, BOTTOM_INNER_Y, TARGET_BUTTON_W, BUTTON_LARGE_H)) {
            int t = adjustTemp((int)state.extruderTarget, (int)state.extruderTemp, +5, 0, 300);
            state.extruderTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M104 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        for (int i = 0; i < 4; i++) {
            int presetX = startX + (i * (TARGET_BUTTON_W + PANEL_SPACING));
            if (inRect(x, y, presetX, MATERIAL_PRESET_Y, TARGET_BUTTON_W, BUTTON_LARGE_H)) {
                state.extruderTarget = extruderPresets[i];
                state.bedTarget      = bedPresets[i];
                sendBoth(extruderPresets[i], bedPresets[i]);
                return ScreenID::TEMPS;
            }
        }
    }

    if (x >= RIGHT_CONTENT_X) {
        // Bed minus button
        int startX = RIGHT_CONTENT_X;
        if (inRect(x, y, startX, BOTTOM_INNER_Y, TARGET_BUTTON_W, BUTTON_LARGE_H)) {
            int t = adjustTemp((int)state.bedTarget, (int)state.bedTemp, -5, 0, 130);
            state.bedTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M140 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        // Bed plus button
        int plusX = startX + TARGET_BUTTON_W + PANEL_SPACING + TARGET_VALUE_W + PANEL_SPACING;
        if (inRect(x, y, plusX, BOTTOM_INNER_Y, TARGET_BUTTON_W, BUTTON_LARGE_H)) {
            int t = adjustTemp((int)state.bedTarget, (int)state.bedTemp, +5, 0, 130);
            state.bedTarget = t;
            char cmd[32]; snprintf(cmd, sizeof(cmd), "M140 S%d", t);
            client.sendGCode(cmd);
            return ScreenID::TEMPS;
        }
        // Bed preset buttons
        for (int i = 0; i < 4; i++) {
            int presetX = startX + (i * (TARGET_BUTTON_W + PANEL_SPACING));
            if (inRect(x, y, presetX, MATERIAL_PRESET_Y, TARGET_BUTTON_W, BUTTON_LARGE_H)) {
                state.bedTarget      = bedPresets[i];
                state.extruderTarget = extruderPresets[i];
                sendBoth(extruderPresets[i], bedPresets[i]);
                return ScreenID::TEMPS;
            }
        }
    }

    return ScreenID::TEMPS;
}

}