#include "screen_print.h"
#include "lang.h"
#include "thumbnail.h"
#include "ui_theme.h"
#include "screen_files.h"

static const int TOP_INNER_Y =             STATUS_BAR_H + PANEL_SPACING;
static const int HALF_BOX_W =              CONTENT_CENTER_X - (PANEL_SPACING * 2);
static const int HALF_BOX_H =              DIVIDER_CENTER_Y - STATUS_BAR_H - (PANEL_SPACING * 2);
static const int BOX_H =                   CONTENT_H - (PANEL_SPACING * 2);
static const int RIGHT_CONTENT_X =         CONTENT_CENTER_X + PANEL_SPACING;
static const int RIGHT_CONTENT_CENTER_X =  CONTENT_CENTER_X + (CONTENT_CENTER_X / 2);
static const int RIGHT_CONTENT_FINAL_X =   SCREEN_W - PANEL_SPACING;
static const int COLUMN_HALF_BOX_W =       (HALF_BOX_W / 2) - PANEL_SPACING;
static const int BUTTON_Y =                NAV_BAR_Y - BUTTON_MEDIUM_H - PANEL_SPACING;

static const int PROGRESS_BAR_Y =           100;
static const int PROGRESS_BAR_H =           23;

// Button bounding boxes
static const struct { int x, y, w, h; } BTN_PAUSE_RESUME = { 
    RIGHT_CONTENT_X, BUTTON_Y, COLUMN_HALF_BOX_W, BUTTON_MEDIUM_H
};
static const struct { int x, y, w, h; } BTN_CANCEL = {
    SCREEN_W - COLUMN_HALF_BOX_W - PANEL_SPACING,  BUTTON_Y, COLUMN_HALF_BOX_W, BUTTON_MEDIUM_H
};
static const struct { int x, y, w, h; } BTN_ESTOP = {
    SCREEN_W - COLUMN_HALF_BOX_W - PANEL_SPACING, TOP_INNER_Y, COLUMN_HALF_BOX_W, BUTTON_SMALL_H
};

static bool s_initialized = false;

static struct {
    char          filename[80]      = "\x01";
    int           progPer10         = -1;
    int           printDuration     = -1;
    int           remaining         = -1;
    char          message[80]       = "\x01";
    PrinterStatus status            = PrinterStatus::DISCONNECTED;
    char          thumbFilename[80] = "\x01";
    bool          thumbShown        = false;
} s_prev;

// ─────────────────────────────────────────────────────
// Static layout – drawn once per activation
// ─────────────────────────────────────────────────────
static void drawStaticLayout() {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_SURFACE);
    tft.drawFastVLine(CONTENT_CENTER_X, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);

    // E-STOP (static, always visible)
    drawButton(BTN_ESTOP.x, BTN_ESTOP.y, BTN_ESTOP.w, BTN_ESTOP.h,
               L.BTN_ESTOP, COLOR_ERROR, COLOR_TEXT, 2);

    // Elapsed / Remaining labels
    tft.setTextFont(2);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(L.LBL_ELAPSED,   RIGHT_CONTENT_X, 134);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(L.LBL_REMAINING, RIGHT_CONTENT_FINAL_X, 134);
}

// ─────────────────────────────────────────────────────
// Pause/Resume/Cancel – redrawn on status change
// ─────────────────────────────────────────────────────
static void drawJobButtons(PrinterStatus status) {
    bool active = (status == PrinterStatus::PRINTING ||
                   status == PrinterStatus::PAUSED);
    bool paused = (status == PrinterStatus::PAUSED);
    
    tft.fillRect(RIGHT_CONTENT_X, BUTTON_Y, HALF_BOX_W, BTN_PAUSE_RESUME.h, COLOR_SURFACE);
    if (active) {
        drawButton(BTN_PAUSE_RESUME.x, BTN_PAUSE_RESUME.y,
                   BTN_PAUSE_RESUME.w, BTN_PAUSE_RESUME.h,
                   paused ? L.BTN_RESUME : L.BTN_PAUSE,
                   paused ? COLOR_SUCCESS : COLOR_WARNING, COLOR_BG, 2);
        drawButton(BTN_CANCEL.x, BTN_CANCEL.y,
                   BTN_CANCEL.w, BTN_CANCEL.h,
                   L.BTN_CANCEL, COLOR_ERROR, COLOR_TEXT, 2);
    } else {
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(L.NO_PRINT_JOB, RIGHT_CONTENT_CENTER_X, BUTTON_Y + BUTTON_MEDIUM_H / 2);
    }
}

// ─────────────────────────────────────────────────────
// Dynamic – changed values only
// ─────────────────────────────────────────────────────
static void drawDynamic(const PrinterState& state) {
    char buf[48];

    // ── Filename ─────────────────────────────────────
    if (strncmp(state.filename, s_prev.filename, sizeof(s_prev.filename)) != 0) {
        char fname[38];
        truncateFilename(state.filename[0] ? state.filename : L.NO_PRINT_JOB, fname, 35);
        tft.fillRect(RIGHT_CONTENT_X, 79, HALF_BOX_W, 18, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(state.filename[0] ? COLOR_TEXT : COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(fname, RIGHT_CONTENT_CENTER_X, 88);
        strlcpy(s_prev.filename, state.filename, sizeof(s_prev.filename));
    }

    // ── Progress percentage + bar ─────────────────────
    int progPer10 = (int)(state.progress * 1000.0f);
    if (progPer10 != s_prev.progPer10) {
        drawProgressBar(RIGHT_CONTENT_X, PROGRESS_BAR_Y, HALF_BOX_W, PROGRESS_BAR_H, state.progress, COLOR_ACCENT, COLOR_BG);
        snprintf(buf, sizeof(buf), "%5.1f%%", state.progress * 100.0f);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT);
        tft.drawString(buf, RIGHT_CONTENT_CENTER_X, PROGRESS_BAR_Y + 11);
        tft.drawString(buf, RIGHT_CONTENT_CENTER_X + 1, PROGRESS_BAR_Y + 11);
        s_prev.progPer10 = progPer10;
    }

    // ── Elapsed time ─────────────────────────────────
    if (state.printDuration != s_prev.printDuration) {
        tft.fillRect(RIGHT_CONTENT_X, 141, COLUMN_HALF_BOX_W, 18, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(formatTime(state.printDuration), RIGHT_CONTENT_X, 150);
        s_prev.printDuration = state.printDuration;
    }

    // ── Remaining time ────────────────────────────────
    int rem = (state.totalDuration > state.printDuration)
              ? state.totalDuration - state.printDuration : 0;
    if (rem != s_prev.remaining) {
        tft.fillRect(RIGHT_CONTENT_FINAL_X - COLUMN_HALF_BOX_W, 141, COLUMN_HALF_BOX_W, 18, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(rem > 0 ? formatTime(rem) : "--", RIGHT_CONTENT_FINAL_X, 150);
        s_prev.remaining = rem;
    }

    // ── Job buttons ───────────────────────────────────
    if (state.status != s_prev.status) {
        drawJobButtons(state.status);
        s_prev.status = state.status;
    }

    // ── Thumbnail (left column) ───────────────────────
    bool printing = (state.status == PrinterStatus::PRINTING ||
                     state.status == PrinterStatus::PAUSED   ||
                     state.status == PrinterStatus::COMPLETE);

    if (printing) {
        if (strncmp(state.filename, s_prev.thumbFilename,
                    sizeof(s_prev.thumbFilename)) != 0) {
            Thumbnail::drawOrPlaceholder(
                state.thumbnailPath,
                PANEL_SPACING, TOP_INNER_Y, HALF_BOX_W, BOX_H);
            strlcpy(s_prev.thumbFilename, state.filename,
                    sizeof(s_prev.thumbFilename));
            s_prev.thumbShown = true;
        }
    } else {
        if (state.status != s_prev.status ||
                s_prev.thumbShown ||
                s_prev.thumbFilename[0] == '\x01') {
            Thumbnail::drawStandby(PANEL_SPACING, TOP_INNER_Y, HALF_BOX_W, BOX_H);
            s_prev.thumbFilename[0] = '\0';
            s_prev.thumbShown       = false;
        }
    }
}

// ─────────────────────────────────────────────────────
// Public
// ─────────────────────────────────────────────────────
namespace ScreenPrint {

void resetInitialized() {
    s_initialized            = false;
    s_prev.filename[0]       = '\x01';
    s_prev.progPer10         = -1;
    s_prev.printDuration     = -1;
    s_prev.remaining         = -1;
    s_prev.message[0]        = '\x01';
    s_prev.status            = PrinterStatus::DISCONNECTED;
    s_prev.thumbFilename[0]  = '\x01';
    s_prev.thumbShown        = false;
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

    if (!active &&
        inRect(x, y, PANEL_SPACING, TOP_INNER_Y, HALF_BOX_W, BOX_H)) {
        ScreenFiles::setPreviousScreen(ScreenID::PRINT);
        return ScreenID::FILES;
    }

    if (inRect(x, y, BTN_ESTOP.x, BTN_ESTOP.y, BTN_ESTOP.w, BTN_ESTOP.h)) {
        client.emergencyStop();
        return ScreenID::PRINT;
    }
    if (active) {
        if (inRect(x, y, BTN_PAUSE_RESUME.x, BTN_PAUSE_RESUME.y,
                    BTN_PAUSE_RESUME.w, BTN_PAUSE_RESUME.h)) {
            if (state.status == PrinterStatus::PAUSED) client.resumePrint();
            else                                        client.pausePrint();
            return ScreenID::PRINT;
        }
        if (inRect(x, y, BTN_CANCEL.x, BTN_CANCEL.y,
                    BTN_CANCEL.w, BTN_CANCEL.h)) {
            client.cancelPrint();
            return ScreenID::PRINT;
        }
    }
    return ScreenID::PRINT;
}

} // namespace ScreenPrint
