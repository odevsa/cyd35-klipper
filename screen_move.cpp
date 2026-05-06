#include "screen_move.h"
#include "ui_theme.h"

#define M 10
#define CY 140

#define AW 70
#define AH 60
#define XY_CX  130

static const int HM_X = XY_CX - AW/2,    HM_Y = CY - AH/2;
static const int XM_X = HM_X - M - AW,   XM_Y = HM_Y;
static const int XP_X = HM_X + AW + M,   XP_Y = HM_Y;
static const int YM_X = HM_X,            YM_Y = HM_Y + AH + M;
static const int YP_X = HM_X,            YP_Y = HM_Y - AH - M;

#define Z_OX   280
#define Z_BW    70
#define Z_BH    60
static const int ZH_Y = CY - Z_BH/2;
static const int ZP_Y = ZH_Y - M - Z_BH;
static const int ZM_Y = ZH_Y + Z_BH + M;

#define S_OX   385
#define S_BW   80
#define S_BH   34
static const float STEPS[4]       = { 0.1f, 1.0f, 10.0f, 100.0f };
static const char* STEP_LABELS[4] = { "0.1 mm", "1 mm", "10 mm", "100 mm" };
static const int   S_HOME_Y       = CONTENT_Y + M;

static float s_step        = 10.0f;
static bool  s_initialized = false;

// Per-field cache
static struct {
    long  posX10  = -999999; // posX * 10 (%.1f precision)
    long  posY10  = -999999;
    long  posZ100 = -999999; // posZ * 100 (%.2f precision)
    float step    = -1.0f;
} s_prev;

static bool _in(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

// ─────────────────────────────────────────────────────
// Static layout – drawn once per activation
// ─────────────────────────────────────────────────────
static void drawStaticLayout() {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_SURFACE);
    tft.drawFastVLine(260, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);
    tft.drawFastVLine(370, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);

    drawButton(YP_X, YP_Y, AW, AH, "Y+",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(XM_X, XM_Y, AW, AH, "X-",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(HM_X, HM_Y, AW, AH, "XY HOME",   COLOR_ACCENT, COLOR_BG,   2);
    drawButton(XP_X, XP_Y, AW, AH, "X+",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(YM_X, YM_Y, AW, AH, "Y-",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(Z_OX, ZP_Y, Z_BW, Z_BH, "Z+",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(Z_OX, ZM_Y, Z_BW, Z_BH, "Z-",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(Z_OX, ZH_Y, Z_BW, Z_BH, "Z HOME", COLOR_ACCENT, COLOR_BG,   2);
    drawButton(S_OX, S_HOME_Y, S_BW, S_BH, "HOME ALL", COLOR_ACCENT, COLOR_BG, 2);
}

// ─────────────────────────────────────────────────────
// Dynamic – only changed values
// ─────────────────────────────────────────────────────
static void drawDynamic(const PrinterState& state) {
    // ── XY position overlay ───────────────────────
    long posX10 = (long)(state.posX * 10.0f);
    long posY10 = (long)(state.posY * 10.0f);
    if (posX10 != s_prev.posX10 || posY10 != s_prev.posY10) {
        char posbuf[36];
        snprintf(posbuf, sizeof(posbuf), "X: %-6.1f  Y: %-6.1f",
                 state.posX, state.posY);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(posbuf, 120, NAV_BAR_Y - 12);
        s_prev.posX10 = posX10;
        s_prev.posY10 = posY10;
    }
    
    // ── Z value text ───────────────────────────────
    long posZ100 = (long)(state.posZ * 100.0f);
    if (posZ100 != s_prev.posZ100) {
        char zbuf[10];
        snprintf(zbuf, sizeof(zbuf), "Z: %-6.2f", state.posZ);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(zbuf, Z_OX + Z_BW / 2, NAV_BAR_Y - 12);
        s_prev.posZ100 = posZ100;
    }

    // ── Step buttons – only redraw when selection changes ─
    if (s_step != s_prev.step) {
        int sy = S_HOME_Y + S_BH + M;
        for (int i = 0; i < 4; i++) {
            bool active = (STEPS[i] == s_step);
            drawButton(S_OX, sy, S_BW, S_BH,
                       STEP_LABELS[i],
                       active ? COLOR_TEXT_DIM  : COLOR_BG,
                       active ? COLOR_BG        : COLOR_TEXT,
                       2);
            sy += S_BH + M;
        }
        s_prev.step = s_step;
    }
}

// ─────────────────────────────────────────────────────
// Public
// ─────────────────────────────────────────────────────
namespace ScreenMove {

void resetInitialized() {
    s_initialized = false;
    s_prev.posX10  = -999999;
    s_prev.posY10  = -999999;
    s_prev.posZ100 = -999999;
    s_prev.step    = -1.0f;
}

void draw(const PrinterState& state) {
    if (!s_initialized) {
        drawStaticLayout();
        s_initialized = true;
    }
    drawDynamic(state);
}

ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client) {
    char cmd[80];

    float feedrate = (s_step >= 10) ? 1500 : 600;
    if (_in(x, y, XM_X, XM_Y, AW, AH)) { // X-
        snprintf(cmd, sizeof(cmd), "G91\nG0 X-%.2f F%.0f\nG90", s_step, feedrate);
        client.sendGCode(cmd);
    } else if (_in(x, y, XP_X, XP_Y, AW, AH)) { // X+
        snprintf(cmd, sizeof(cmd), "G91\nG0 X%.2f F%.0f\nG90", s_step, feedrate);
        client.sendGCode(cmd);
    } else if (_in(x, y, YM_X, YM_Y, AW, AH)) { // Y-
        snprintf(cmd, sizeof(cmd), "G91\nG0 Y-%.2f F%.0f\nG90", s_step, feedrate);
        client.sendGCode(cmd);
    } else if(_in(x, y, YP_X, YP_Y, AW, AH)) { // Y+
        snprintf(cmd, sizeof(cmd), "G91\nG0 Y%.2f F%.0f\nG90", s_step, feedrate);
        client.sendGCode(cmd);
    } else if (_in(x, y, HM_X, HM_Y, AW, AH)) { // XY HOME
        client.sendGCode("G28 X Y");
    }

    // ── Z axis buttons on the right ─────────────────────
    if (x >= 240 && x < S_OX) {
        if (_in(x, y, Z_OX, ZP_Y, Z_BW, Z_BH)) { // Z+
            snprintf(cmd, sizeof(cmd), "G91\nG0 Z%.2f F300\nG90", s_step);
            client.sendGCode(cmd);
        } else if (_in(x, y, Z_OX, ZM_Y, Z_BW, Z_BH)) { // Z-
            snprintf(cmd, sizeof(cmd), "G91\nG0 Z-%.2f F300\nG90", s_step);
            client.sendGCode(cmd);
        } else if (_in(x, y, Z_OX, ZH_Y, Z_BW, Z_BH)) { // Z HOME
            client.sendGCode("G28 Z");
        }
        return ScreenID::MOVE;
    }

    // ── Step selection buttons on the right ───────────────
    if (x >= S_OX) {
        int sy = S_HOME_Y + S_BH + M;
        for (int i = 0; i < 4; i++) {
            if (_in(x, y, S_OX, sy, S_BW, S_BH)) {
                s_step = STEPS[i];
                return ScreenID::MOVE;
            }
            sy += S_BH + M;
        }
        if (_in(x, y, S_OX, S_HOME_Y, S_BW, S_BH)) {
            client.sendGCode("G28"); // HOME ALL
        }
        return ScreenID::MOVE;
    }

    return ScreenID::MOVE;
}

} // namespace ScreenMove
