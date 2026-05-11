#include "screen_move.h"
#include "lang.h"
#include "ui_theme.h"

static const int LEFT_DIVIDER_X =           260;
static const int RIGHT_DIVIDER_X =          370;
static const int SCREEN_SPACING =           PANEL_SPACING;
static const int TOP_CONTENT_Y =            CONTENT_Y + SCREEN_SPACING;
static const int BOX_H =                    CONTENT_H - (SCREEN_SPACING * 2) - 18;
static const int CONTENT_CENTER_Y =         TOP_CONTENT_Y + (BOX_H / 2);
static const int LEFT_CONTENT_X =           SCREEN_SPACING;
static const int LEFT_CONTENT_CENTER_X =    LEFT_DIVIDER_X / 2;
static const int LEFT_BOX_W =               LEFT_DIVIDER_X - LEFT_CONTENT_X - SCREEN_SPACING;
static const int MIDDLE_CONTENT_X =         LEFT_DIVIDER_X + SCREEN_SPACING;
static const int MIDDLE_BOX_W =             RIGHT_DIVIDER_X - MIDDLE_CONTENT_X - SCREEN_SPACING;
static const int RIGHT_CONTENT_X =          RIGHT_DIVIDER_X + SCREEN_SPACING;
static const int RIGHT_BOX_W =              SCREEN_W - RIGHT_CONTENT_X - SCREEN_SPACING;

static const int LEFT_BUTTON_W =            (LEFT_BOX_W - (SCREEN_SPACING * 2)) / 3;
static const int LEFT_BUTTON_H =            (BOX_H - (SCREEN_SPACING * 2)) / 3;
static const int LEFT_BUTTON_CENTER_X =     LEFT_CONTENT_CENTER_X - (LEFT_BUTTON_W / 2);
static const int LEFT_BUTTON_CENTER_Y =     CONTENT_CENTER_Y - (LEFT_BUTTON_H / 2);
static const int MIDDLE_BUTTON_W =          MIDDLE_BOX_W;
static const int MIDDLE_BUTTON_H =          (BOX_H - (SCREEN_SPACING * 2)) / 3;
static const int MIDDLE_BUTTON_CENTER_Y =   CONTENT_CENTER_Y - (MIDDLE_BUTTON_H / 2);
static const int RIGHT_BUTTON_W =           RIGHT_BOX_W;
static const int RIGHT_BUTTON_H =           (BOX_H - (SCREEN_SPACING * 4)) / 5;

static const int STATUS_Y =                 NAV_BAR_Y - ((NAV_BAR_Y - BOX_H - TOP_CONTENT_Y) / 2) - 2;

static const float STEPS[4]       = { 0.1f, 1.0f, 10.0f, 100.0f };
static const char* STEP_LABELS[4] = { "0.1 mm", "1 mm", "10 mm", "100 mm" };

static float s_step        = 10.0f;
static bool  s_initialized = false;

// Per-field cache
static struct {
    long  posX10  = -999999; // posX * 10 (%.1f precision)
    long  posY10  = -999999;
    long  posZ100 = -999999; // posZ * 100 (%.2f precision)
    float step    = -1.0f;
} s_prev;

// ─────────────────────────────────────────────────────
// Static layout – drawn once per activation
// ─────────────────────────────────────────────────────
static void drawStaticLayout() {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_SURFACE);
    tft.drawFastVLine(LEFT_DIVIDER_X, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);
    tft.drawFastVLine(RIGHT_DIVIDER_X, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);

    drawButton(LEFT_BUTTON_CENTER_X, LEFT_BUTTON_CENTER_Y - LEFT_BUTTON_H - SCREEN_SPACING, LEFT_BUTTON_W, LEFT_BUTTON_H, "Y+",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(LEFT_BUTTON_CENTER_X - LEFT_BUTTON_W - SCREEN_SPACING, LEFT_BUTTON_CENTER_Y, LEFT_BUTTON_W, LEFT_BUTTON_H, "X-",     COLOR_BG,     COLOR_TEXT, 2);
    drawButton(LEFT_BUTTON_CENTER_X, LEFT_BUTTON_CENTER_Y, LEFT_BUTTON_W, LEFT_BUTTON_H, L.BTN_HOME_XY, COLOR_ACCENT, COLOR_BG,   2);
    drawButton(LEFT_BUTTON_CENTER_X + LEFT_BUTTON_W + SCREEN_SPACING, LEFT_BUTTON_CENTER_Y, LEFT_BUTTON_W, LEFT_BUTTON_H, "X+",          COLOR_BG,     COLOR_TEXT, 2);
    drawButton(LEFT_BUTTON_CENTER_X, LEFT_BUTTON_CENTER_Y + LEFT_BUTTON_H + SCREEN_SPACING, LEFT_BUTTON_W, LEFT_BUTTON_H, "Y-",          COLOR_BG,     COLOR_TEXT, 2);
    
    drawButton(MIDDLE_CONTENT_X, MIDDLE_BUTTON_CENTER_Y - MIDDLE_BUTTON_H - SCREEN_SPACING, MIDDLE_BUTTON_W, MIDDLE_BUTTON_H, "Z+",          COLOR_BG,     COLOR_TEXT, 2);
    drawButton(MIDDLE_CONTENT_X, MIDDLE_BUTTON_CENTER_Y, MIDDLE_BUTTON_W, MIDDLE_BUTTON_H, L.BTN_HOME_Z,  COLOR_ACCENT, COLOR_BG,   2);
    drawButton(MIDDLE_CONTENT_X, MIDDLE_BUTTON_CENTER_Y + MIDDLE_BUTTON_H + SCREEN_SPACING, MIDDLE_BUTTON_W, MIDDLE_BUTTON_H, "Z-",          COLOR_BG,     COLOR_TEXT, 2);
    
    drawButton(RIGHT_CONTENT_X, TOP_CONTENT_Y, RIGHT_BUTTON_W, RIGHT_BUTTON_H, L.BTN_HOME_ALL, COLOR_ACCENT, COLOR_BG, 2);
}

// ─────────────────────────────────────────────────────
// Dynamic – only changed values
// ─────────────────────────────────────────────────────
static void drawDynamic(const PrinterState& state) {
    // ── XY value ─────────────────────────────────────
    long posX10 = (long)(state.posX * 10.0f);
    long posY10 = (long)(state.posY * 10.0f);
    if (posX10 != s_prev.posX10 || posY10 != s_prev.posY10) {
        char posbuf[36];
        snprintf(posbuf, sizeof(posbuf), "X: %-6.1f  Y: %-6.1f",
                 state.posX, state.posY);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(posbuf, LEFT_CONTENT_CENTER_X, STATUS_Y);
        s_prev.posX10 = posX10;
        s_prev.posY10 = posY10;
    }
    
    // ── Z value ──────────────────────────────────────
    long posZ100 = (long)(state.posZ * 100.0f);
    if (posZ100 != s_prev.posZ100) {
        char zbuf[10];
        snprintf(zbuf, sizeof(zbuf), "Z: %-6.2f", state.posZ);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(zbuf, MIDDLE_CONTENT_X + (MIDDLE_BUTTON_W / 2), STATUS_Y);
        s_prev.posZ100 = posZ100;
    }

    // ── Step buttons ─────────────────────────────────
    if (s_step != s_prev.step) {
        int positionY = TOP_CONTENT_Y + RIGHT_BUTTON_H + SCREEN_SPACING;
        int incrementalHeight = RIGHT_BUTTON_H + SCREEN_SPACING;
        for (int i = 0; i < 4; i++) {
            bool active = (STEPS[i] == s_step);
            drawButton(RIGHT_CONTENT_X, positionY, RIGHT_BUTTON_W, RIGHT_BUTTON_H,
                       STEP_LABELS[i],
                       active ? COLOR_TEXT_DIM  : COLOR_BG,
                       active ? COLOR_BG        : COLOR_TEXT,
                       2);
            positionY += incrementalHeight;
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
        (void)state;
        char cmd[80];
        float feedrate = (s_step >= 10) ? 1500 : 600;

        // ── XY movement buttons (left column) ────────────
        if (x < LEFT_DIVIDER_X) {
            if (inRect(x, y, LEFT_BUTTON_CENTER_X, LEFT_BUTTON_CENTER_Y - LEFT_BUTTON_H - SCREEN_SPACING, LEFT_BUTTON_W, LEFT_BUTTON_H)) { // Y+
                snprintf(cmd, sizeof(cmd), "G91\nG0 Y%.2f F%.0f\nG90", s_step, feedrate);
                client.sendGCode(cmd);
            } else if (inRect(x, y, LEFT_BUTTON_CENTER_X - LEFT_BUTTON_W - SCREEN_SPACING, LEFT_BUTTON_CENTER_Y, LEFT_BUTTON_W, LEFT_BUTTON_H)) { // X-
                snprintf(cmd, sizeof(cmd), "G91\nG0 X-%.2f F%.0f\nG90", s_step, feedrate);
                client.sendGCode(cmd);
            } else if (inRect(x, y, LEFT_BUTTON_CENTER_X, LEFT_BUTTON_CENTER_Y, LEFT_BUTTON_W, LEFT_BUTTON_H)) { // HOME XY
                client.sendGCode("G28 X Y");
            } else if (inRect(x, y, LEFT_BUTTON_CENTER_X + LEFT_BUTTON_W + SCREEN_SPACING, LEFT_BUTTON_CENTER_Y, LEFT_BUTTON_W, LEFT_BUTTON_H)) { // X+
                snprintf(cmd, sizeof(cmd), "G91\nG0 X%.2f F%.0f\nG90", s_step, feedrate);
                client.sendGCode(cmd);
            } else if (inRect(x, y, LEFT_BUTTON_CENTER_X, LEFT_BUTTON_CENTER_Y + LEFT_BUTTON_H + SCREEN_SPACING, LEFT_BUTTON_W, LEFT_BUTTON_H)) { // Y-
                snprintf(cmd, sizeof(cmd), "G91\nG0 Y-%.2f F%.0f\nG90", s_step, feedrate);
                client.sendGCode(cmd);
            }
        }

        // ── Z axis buttons (middle column) ───────────────
        if (x >= LEFT_DIVIDER_X && x < RIGHT_DIVIDER_X) {
            if (inRect(x, y, MIDDLE_CONTENT_X, MIDDLE_BUTTON_CENTER_Y - MIDDLE_BUTTON_H - SCREEN_SPACING, MIDDLE_BUTTON_W, MIDDLE_BUTTON_H)) { // Z+
                snprintf(cmd, sizeof(cmd), "G91\nG0 Z%.2f F300\nG90", s_step);
                client.sendGCode(cmd);
            } else if (inRect(x, y, MIDDLE_CONTENT_X, MIDDLE_BUTTON_CENTER_Y, MIDDLE_BUTTON_W, MIDDLE_BUTTON_H)) { // HOME Z
                client.sendGCode("G28 Z");
            } else if (inRect(x, y, MIDDLE_CONTENT_X, MIDDLE_BUTTON_CENTER_Y + MIDDLE_BUTTON_H + SCREEN_SPACING, MIDDLE_BUTTON_W, MIDDLE_BUTTON_H)) { // Z-
                snprintf(cmd, sizeof(cmd), "G91\nG0 Z-%.2f F300\nG90", s_step);
                client.sendGCode(cmd);
            }
            return ScreenID::MOVE;
        }

        // ── Step selection + HOME ALL (right column) ──────
        if (x >= RIGHT_DIVIDER_X) {
            if (inRect(x, y, RIGHT_CONTENT_X, TOP_CONTENT_Y, RIGHT_BUTTON_W, RIGHT_BUTTON_H)) {
                client.sendGCode("G28"); // HOME ALL
            }

            int sy = TOP_CONTENT_Y + RIGHT_BUTTON_H + SCREEN_SPACING;
            for (int i = 0; i < 4; i++) {
                if (inRect(x, y, RIGHT_CONTENT_X, sy, RIGHT_BUTTON_W, RIGHT_BUTTON_H)) {
                    s_step = STEPS[i];
                    return ScreenID::MOVE;
                }
                sy += RIGHT_BUTTON_H + SCREEN_SPACING;
            }
            return ScreenID::MOVE;
        }

        return ScreenID::MOVE;
    }

}
