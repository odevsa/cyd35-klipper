#include "screen_home.h"
#include "screen_files.h"
#include "lang.h"
#include "thumbnail.h"
#include "ui_theme.h"
#include "temp_graph.h"

static const int TOP_INNER_Y =              STATUS_BAR_H + PANEL_SPACING;
static const int BOTTOM_INNER_Y =           DIVIDER_CENTER_Y + PANEL_SPACING;
static const int BOTTOM_CENTER_Y =          (NAV_BAR_Y - DIVIDER_CENTER_Y) / 2;
static const int FAN_X =                    CONTENT_CENTER_X + PANEL_SPACING;
static const int FAN_Y =                    NAV_BAR_Y - PANEL_SPACING - 18;
static const int HALF_BOX_W =               CONTENT_CENTER_X - (PANEL_SPACING * 2);
static const int HALF_BOX_H =               DIVIDER_CENTER_Y - STATUS_BAR_H - (PANEL_SPACING * 2);
static const int RIGHT_CONTENT_X =          CONTENT_CENTER_X + PANEL_SPACING;
static const int RIGHT_CONTENT_CENTER_X =   CONTENT_CENTER_X + (CONTENT_CENTER_X / 2);
static const int RIGHT_CONTENT_FINAL_X =    SCREEN_W - PANEL_SPACING;
static const int COLUMN_HALF_BOX_W =        (HALF_BOX_W / 2) - PANEL_SPACING;

static const int HOTEND_LABEL_Y =           TOP_INNER_Y + 7;
static const int HOTEND_TEMP_Y =            TOP_INNER_Y + (HALF_BOX_H / 2) - 20;
static const int HOTEND_TARGET_Y =          DIVIDER_CENTER_Y - PANEL_SPACING - 18;
static const int BED_LABEL_Y =              BOTTOM_INNER_Y + 7;
static const int BED_TEMP_Y =               BOTTOM_INNER_Y + (HALF_BOX_H / 2) - 20;
static const int BED_TARGET_Y =             NAV_BAR_Y - PANEL_SPACING - 18;

static const int TEMPERATURE_HISTORIC_X =   100;
static const int TEMPERATURE_HISTORIC_W =   (CONTENT_CENTER_X - TEMPERATURE_HISTORIC_X - PANEL_SPACING);

static const int PROGRESS_BAR_Y =           TOP_INNER_Y + HALF_BOX_H;
static const int PROGRESS_BAR_H =           23;
static const int FILENAME_Y =               PROGRESS_BAR_Y + PROGRESS_BAR_H + 10;
static const int STATUS_LABEL_Y =           FILENAME_Y + 26;
static const int STATUS_VALUE_Y =           STATUS_LABEL_Y + 18;

// Button bounding boxes
static const struct { int x, y, w, h; } BTN_RESTART = {
    SCREEN_W - COLUMN_HALF_BOX_W - PANEL_SPACING,  NAV_BAR_Y - BUTTON_SMALL_H - PANEL_SPACING, COLUMN_HALF_BOX_W, BUTTON_SMALL_H 
};

static bool s_initialized = false;

// ─────────────────────────────────────────────────────
// Last-drawn value cache – sentinel = impossible value so
// the first draw always renders.
// We compare at the same precision used for rendering:
//   temps  → int (%.0f)
//   progress → int‰ (%.1f → *10)
//   posZ   → int/100 (%.2f → *100)
//   time   → stored as seconds, redrawn each second change
// ─────────────────────────────────────────────────────
static struct {
    int           extruderTemp   = -9999;
    int           extruderTarget = -9999;
    bool          extruderAtTemp = false;
    int           bedTemp        = -9999;
    int           bedTarget      = -9999;
    bool          bedAtTemp      = false;
    int           fanPct         = -1;
    PrinterStatus status         = PrinterStatus::DISCONNECTED;
    char          filename[80]   = "\x01"; // sentinel
    int           progressPer10  = -1;    // progress*10 as int
    int           printDuration  = -1;
    int           remaining      = -1;
    long          posZ100        = -999999; // posZ*100 as long
    char          thumbFilename[80] = "\x01";
    bool          thumbShown        = false;
} s_prev;

static void drawStaticLayout() {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_SURFACE);
    tft.drawFastVLine(CONTENT_CENTER_X, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);
    tft.drawFastHLine(PANEL_SPACING, DIVIDER_CENTER_Y, CONTENT_CENTER_X - (PANEL_SPACING * 2), COLOR_DIVIDER);

    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    tft.drawString(L.LBL_HOTEND, PANEL_SPACING, HOTEND_LABEL_Y);
    tft.drawString(L.LBL_BED, PANEL_SPACING, BED_LABEL_Y);

    // ── Firmware Restart ─────────────────────────────
    drawButton(BTN_RESTART.x, BTN_RESTART.y, BTN_RESTART.w, BTN_RESTART.h, L.BTN_RESTART, COLOR_ERROR, COLOR_TEXT, 2);

    // ── Info ─────────────────────────────────────────
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(L.LBL_ELAPSED,   RIGHT_CONTENT_X, STATUS_LABEL_Y);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(L.LBL_REMAINING, RIGHT_CONTENT_CENTER_X, STATUS_LABEL_Y);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(L.LBL_HEIGHT, RIGHT_CONTENT_FINAL_X, STATUS_LABEL_Y);

}

static void drawDynamic(const PrinterState& state) {
    char buf[32];
    int TEMP_LIMIT_X = TEMPERATURE_HISTORIC_X - (PANEL_SPACING * 2);

    // ── Hotend temperature ───────────────────────────
    int   extI   = (int)state.extruderTemp;
    bool  extAt  = (state.extruderTemp >= state.extruderTarget - 2 && state.extruderTarget > 0);
    if (extI != s_prev.extruderTemp || extAt != s_prev.extruderAtTemp) {
        snprintf(buf, sizeof(buf), "%3d", extI);
        tft.fillRect(PANEL_SPACING, HOTEND_TEMP_Y - 1, TEMP_LIMIT_X, 49, COLOR_SURFACE);
        tft.setTextFont(6);
        tft.setTextColor(extAt ? COLOR_SUCCESS : COLOR_TEXT, COLOR_SURFACE);
        tft.setCursor(PANEL_SPACING, HOTEND_TEMP_Y);
        tft.print(buf);
        s_prev.extruderTemp   = extI;
        s_prev.extruderAtTemp = extAt;
    }

    int   extTgt = (int)state.extruderTarget;
    if (extTgt != s_prev.extruderTarget) {
        snprintf(buf, sizeof(buf), "/%3d C ", extTgt);
        tft.fillRect(PANEL_SPACING, HOTEND_TARGET_Y - 1, TEMP_LIMIT_X, 18, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.setCursor(PANEL_SPACING, HOTEND_TARGET_Y);
        tft.print(buf);
        s_prev.extruderTarget = extTgt;
    }

    // ── Hotend historic ──────────────────────────────
    g_hotendGraph.push((float)extI, (float)extTgt);
    g_hotendGraph.draw(TEMPERATURE_HISTORIC_X, TOP_INNER_Y, TEMPERATURE_HISTORIC_W, HALF_BOX_H, TGRAPH_HOT_LINE, TGRAPH_HOT_TGT);

    // ── Bed temperature ──────────────────────────────
    int  bedI   = (int)state.bedTemp;
    bool bedAt  = (state.bedTemp >= state.bedTarget - 2 && state.bedTarget > 0);
    if (bedI != s_prev.bedTemp || bedAt != s_prev.bedAtTemp) {
        snprintf(buf, sizeof(buf), "%3d", bedI);
        tft.fillRect(PANEL_SPACING, BED_TEMP_Y - 1, TEMP_LIMIT_X, 50, COLOR_SURFACE);
        tft.setTextFont(6);
        tft.setTextColor(bedAt ? COLOR_SUCCESS : COLOR_TEXT, COLOR_SURFACE);
        tft.setCursor(PANEL_SPACING, BED_TEMP_Y);
        tft.print(buf);
        s_prev.bedTemp   = bedI;
        s_prev.bedAtTemp = bedAt;
    }
    
    int  bedTgt = (int)state.bedTarget;
    if (bedTgt != s_prev.bedTarget) {
        snprintf(buf, sizeof(buf), "/%3d C ", bedTgt);
        tft.fillRect(PANEL_SPACING, BED_TARGET_Y - 1, TEMP_LIMIT_X, 18, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.setCursor(PANEL_SPACING, BED_TARGET_Y);
        tft.print(buf);
        s_prev.bedTarget = bedTgt;
    }

    // ── Bed historic ─────────────────────────────────
    g_bedGraph.push((float)bedI, (float)bedTgt);
    g_bedGraph.draw(TEMPERATURE_HISTORIC_X, BOTTOM_INNER_Y, TEMPERATURE_HISTORIC_W, HALF_BOX_H, TGRAPH_BED_LINE, TGRAPH_BED_TGT);

    // ── Progress bar + percentage ─────────────────────
    int progPer10 = (int)(state.progress * 1000.0f); // %.1f precision
    if (progPer10 != s_prev.progressPer10) {
        drawProgressBar(RIGHT_CONTENT_X, PROGRESS_BAR_Y, HALF_BOX_W, PROGRESS_BAR_H, state.progress,
            COLOR_ACCENT, COLOR_BG);
        snprintf(buf, sizeof(buf), "%5.1f%%", state.progress * 100.0f);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT);
        tft.drawString(buf, RIGHT_CONTENT_CENTER_X, PROGRESS_BAR_Y + 11);
        tft.drawString(buf, RIGHT_CONTENT_CENTER_X + 1, PROGRESS_BAR_Y + 11);
        s_prev.progressPer10 = progPer10;
    }

    // ── Filename ─────────────────────────────────────
    if (strncmp(state.filename, s_prev.filename, sizeof(s_prev.filename)) != 0) {
        char fname[28];
        truncateFilename(state.filename[0] ? state.filename : L.NO_FILE, fname, 26);
        tft.fillRect(RIGHT_CONTENT_X, FILENAME_Y-1, HALF_BOX_W, 18, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(state.filename[0] ? COLOR_TEXT : COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(fname, RIGHT_CONTENT_CENTER_X, FILENAME_Y);
        strlcpy(s_prev.filename, state.filename, sizeof(s_prev.filename));
    }

    // ── Elapsed time (updates every second) ──────────
    if (state.printDuration != s_prev.printDuration) {
        String elapsed = formatTime(state.printDuration);
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%-9s", elapsed.c_str());
        tft.setTextFont(2);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(tbuf, RIGHT_CONTENT_X, STATUS_VALUE_Y);
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
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(tbuf, RIGHT_CONTENT_CENTER_X, STATUS_VALUE_Y);
        s_prev.remaining = rem;
    }
    
    // ── Z position ───────────────────────────────────
    long posZ100 = (long)(state.posZ * 100.0f);
    if (posZ100 != s_prev.posZ100) {
        snprintf(buf, sizeof(buf), "%6.2f mm", state.posZ);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.setTextDatum(MR_DATUM);
        tft.drawString(buf, RIGHT_CONTENT_FINAL_X, STATUS_VALUE_Y);
        s_prev.posZ100 = posZ100;
    }
    
    // ── Fan ─────────────────────────────────────────
    int fanPct = (int)(state.fanSpeed * 100);
    if (fanPct != s_prev.fanPct) {
        snprintf(buf, sizeof(buf), "%s %3d%%", L.LBL_FAN, fanPct);
        tft.fillRect(FAN_X, FAN_Y - 1, COLUMN_HALF_BOX_W, 18, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.setCursor(FAN_X, FAN_Y);
        tft.print(buf);
        s_prev.fanPct = fanPct;
    }

    // ── Thumbnail (right panel, above filename) ───────
    bool printing = (state.status == PrinterStatus::PRINTING ||
                     state.status == PrinterStatus::PAUSED   ||
                     state.status == PrinterStatus::COMPLETE);
    if (printing) {
        if (strncmp(state.filename, s_prev.thumbFilename,
                    sizeof(s_prev.thumbFilename)) != 0) {
            Thumbnail::drawOrPlaceholder(
                state.thumbnailPath,
                RIGHT_CONTENT_X, TOP_INNER_Y, HALF_BOX_W, HALF_BOX_H);
            strlcpy(s_prev.thumbFilename, state.filename,
                    sizeof(s_prev.thumbFilename));
            s_prev.thumbShown = true;
        }
    } else {
        if (state.status != s_prev.status ||
                s_prev.thumbShown ||
                s_prev.thumbFilename[0] == '\x01') {
            Thumbnail::drawStandby(RIGHT_CONTENT_X, TOP_INNER_Y, HALF_BOX_W, HALF_BOX_H);
            s_prev.thumbFilename[0] = '\0';
            s_prev.thumbShown       = false;
        }
    }
}

// ─────────────────────────────────────────────────────
// Public
// ─────────────────────────────────────────────────────
namespace ScreenHome {

void resetInitialized() {
    s_initialized = false;
    // Reset cache so every field redraws on first pass
    s_prev.extruderTemp   = -9999;
    s_prev.extruderTarget = -9999;
    s_prev.extruderAtTemp = false;
    s_prev.bedTemp        = -9999;
    s_prev.bedTarget      = -9999;
    s_prev.bedAtTemp      = false;
    // g_hotendGraph and g_bedGraph are global – history is preserved across screen switches.
    s_prev.fanPct         = -1;
    s_prev.status         = PrinterStatus::DISCONNECTED;
    s_prev.filename[0]      = '\x01';
    s_prev.progressPer10    = -1;
    s_prev.printDuration    = -1;
    s_prev.remaining        = -1;
    s_prev.posZ100          = -999999;
    s_prev.thumbFilename[0] = '\x01';
    s_prev.thumbShown       = false;
}

void draw(const PrinterState& state) {
    if (!s_initialized) {
        drawStaticLayout();
        s_initialized = true;
    }
    drawDynamic(state);
}

ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client) {

    if (inRect(x, y, BTN_RESTART.x, BTN_RESTART.y, BTN_RESTART.w, BTN_RESTART.h)) {
        client.firmwareRestart();
        return ScreenID::HOME;
    }

    bool active = (state.status == PrinterStatus::PRINTING ||
                   state.status == PrinterStatus::PAUSED);

    if (!active &&
        inRect(x, y, RIGHT_CONTENT_X, TOP_INNER_Y, HALF_BOX_W, HALF_BOX_H)) {
        ScreenFiles::setPreviousScreen(ScreenID::HOME);
        return ScreenID::FILES;
    }

    return ScreenID::HOME;
}

}
