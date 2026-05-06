#include "screen_print.h"
#include "ui_theme.h"

static const struct { int x, y, w, h; } BTN_PAUSE_RESUME = { 20,  215, 200, 45 };
static const struct { int x, y, w, h; } BTN_CANCEL       = { 260, 215, 200, 45 };
static const struct { int x, y, w, h; } BTN_ESTOP        = { 385,  33,  85, 24 };

static bool _inRect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool s_initialized = false;

static struct {
    char          filename[80]  = "\x01";
    int           progPer10     = -1;
    int           printDuration = -1;
    int           remaining     = -1;
    char          message[80]   = "\x01";
    PrinterStatus status        = PrinterStatus::DISCONNECTED;
} s_prev;

static void drawStaticLayout() {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_SURFACE);
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.drawString("Elapsed",   20,  168);
    tft.drawString("Remaining", 260, 168);
    drawButton(BTN_ESTOP.x, BTN_ESTOP.y, BTN_ESTOP.w, BTN_ESTOP.h,
               "E-STOP", COLOR_ERROR, COLOR_TEXT, 2);
}

static void drawJobButtons(PrinterStatus status) {
    bool active = (status == PrinterStatus::PRINTING || status == PrinterStatus::PAUSED);
    bool paused = (status == PrinterStatus::PAUSED);
    if (active) {
        const char* pauseLabel = paused ? "RESUME" : "PAUSE";
        uint16_t pauseColor    = paused ? COLOR_SUCCESS : COLOR_WARNING;
        drawButton(BTN_PAUSE_RESUME.x, BTN_PAUSE_RESUME.y,
                   BTN_PAUSE_RESUME.w, BTN_PAUSE_RESUME.h,
                   pauseLabel, pauseColor, COLOR_BG, 4);
        drawButton(BTN_CANCEL.x, BTN_CANCEL.y,
                   BTN_CANCEL.w, BTN_CANCEL.h,
                   "CANCEL", COLOR_ERROR, COLOR_TEXT, 4);
    } else {
        tft.fillRect(0, 200, SCREEN_W, 64, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString("No active print job", SCREEN_W / 2, 230);
    }
}

static void drawDynamic(const PrinterState& state) {
    char buf[48];

    // ── Filename ─────────────────────────────────────
    if (strncmp(state.filename, s_prev.filename, sizeof(s_prev.filename)) != 0) {
        char fname[44];
        truncateFilename(state.filename[0] ? state.filename : "No active print",
                         fname, 42);
        tft.fillRect(0, 34, BTN_ESTOP.x - 1, 20, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(fname, SCREEN_W / 2, 44);
        strlcpy(s_prev.filename, state.filename, sizeof(s_prev.filename));
    }

    // ── Progress percentage + bar ─────────────────────
    int progPer10 = (int)(state.progress * 1000.0f);
    if (progPer10 != s_prev.progPer10) {
        snprintf(buf, sizeof(buf), "%5.1f%%", state.progress * 100.0f);
        tft.setTextFont(5);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_ACCENT, COLOR_SURFACE);
        tft.drawString(buf, SCREEN_W / 2, 95);
        drawProgressBar(20, 135, SCREEN_W - 40, 24, state.progress,
                        COLOR_ACCENT, COLOR_BG);
        s_prev.progPer10 = progPer10;
    }

    // ── Elapsed time ─────────────────────────────────
    if (state.printDuration != s_prev.printDuration) {
        String elapsed = formatTime(state.printDuration);
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%-9s", elapsed.c_str());
        tft.setTextFont(2);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(tbuf, 20, 184);
        s_prev.printDuration = state.printDuration;
    }

    // ── Remaining time ────────────────────────────────
    int rem = (state.totalDuration > state.printDuration)
              ? state.totalDuration - state.printDuration : 0;
    if (rem != s_prev.remaining) {
        String eta = (rem > 0) ? formatTime(rem) : "--";
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%-9s", eta.c_str());
        tft.setTextFont(2);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(tbuf, 260, 184);
        s_prev.remaining = rem;
    }

    // ── Status message ────────────────────────────────
    if (strncmp(state.message, s_prev.message, sizeof(s_prev.message)) != 0) {
        tft.fillRect(0, 195, SCREEN_W, 20, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(state.message, SCREEN_W / 2, 205);
        strlcpy(s_prev.message, state.message, sizeof(s_prev.message));
    }

    // ── Job buttons ───────────────────────────────────
    if (state.status != s_prev.status) {
        drawJobButtons(state.status);
        s_prev.status = state.status;
    }
}

// ─────────────────────────────────────────────────────
// Public
// ─────────────────────────────────────────────────────
namespace ScreenPrint {

void resetInitialized() {
    s_initialized = false;
    s_prev.filename[0]  = '\x01';
    s_prev.progPer10    = -1;
    s_prev.printDuration = -1;
    s_prev.remaining    = -1;
    s_prev.message[0]   = '\x01';
    s_prev.status       = PrinterStatus::DISCONNECTED;
}

void draw(const PrinterState& state) {
    if (!s_initialized) {
        drawStaticLayout();
        s_initialized = true;
    }
    drawDynamic(state);
}

ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client) {
    bool active = (state.status == PrinterStatus::PRINTING ||
                   state.status == PrinterStatus::PAUSED);

    if (_inRect(x, y, BTN_ESTOP.x, BTN_ESTOP.y, BTN_ESTOP.w, BTN_ESTOP.h)) {
        client.emergencyStop();
        return ScreenID::PRINT;
    }
    if (active) {
        if (_inRect(x, y, BTN_PAUSE_RESUME.x, BTN_PAUSE_RESUME.y,
                    BTN_PAUSE_RESUME.w, BTN_PAUSE_RESUME.h)) {
            if (state.status == PrinterStatus::PAUSED) client.resumePrint();
            else                                        client.pausePrint();
            return ScreenID::PRINT;
        }
        if (_inRect(x, y, BTN_CANCEL.x, BTN_CANCEL.y,
                    BTN_CANCEL.w, BTN_CANCEL.h)) {
            client.cancelPrint();
            return ScreenID::PRINT;
        }
    }
    return ScreenID::PRINT;
}

} // namespace ScreenPrint
