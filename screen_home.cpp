#include "screen_home.h"
#include "ui_theme.h"

static const int DIVIDER_X  = 234;
static const int PANEL_PAD  =   8;
static const int LBL_HOTEND_Y = 38;
static const int TMP_HOTEND_Y = 54;
static const int TGT_HOTEND_Y = 103;
static const int SEP_Y        = 143;
static const int LBL_BED_Y    = 158;
static const int TMP_BED_Y    = 174;
static const int TGT_BED_Y    = 223;
static const int FAN_Y        = 243;
static const int RP_X = 240;
static const int RP_W = 235;
static const int RP_XC = RP_X + RP_W / 2;
static const int RP_XF = 470;
static const int RB_Y = 30;
static const int RB_W = 110;
static const int RF_X = RP_X + RB_W + 15;
static const int RB_H = 26;

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
} s_prev;

static void drawStaticLayout() {
    tft.fillRect(0, CONTENT_Y, DIVIDER_X, CONTENT_H, COLOR_SURFACE);
    tft.fillRect(DIVIDER_X + 1, CONTENT_Y,
                 SCREEN_W - DIVIDER_X - 1, CONTENT_H, COLOR_SURFACE);
    tft.drawFastVLine(DIVIDER_X, CONTENT_Y, CONTENT_H, COLOR_DIVIDER);

    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    tft.drawString("HOTEND", PANEL_PAD, LBL_HOTEND_Y);
    tft.drawFastHLine(4, SEP_Y, DIVIDER_X - 8, COLOR_DIVIDER);
    tft.drawString("BED", PANEL_PAD, LBL_BED_Y);

    // ── Firmware Restart ─────────────────────────────
    tft.fillRoundRect(RF_X, RB_Y, RB_W, RB_H, 5, COLOR_ERROR);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_ERROR);
    tft.drawString("RESTART", RF_X + RB_W / 2, RB_Y + 13);
    
    // ── Info ─────────────────────────────────────────
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("ELAPSED",   RP_X, 235);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("REMAINING", RP_XC, 235);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("HEIGHT", RP_XF, 235);

}

static void drawDynamic(const PrinterState& state) {
    char buf[32];

    // ── Hotend temperature ───────────────────────────
    int   extI   = (int)state.extruderTemp;
    int   extTgt = (int)state.extruderTarget;
    bool  extAt  = (state.extruderTemp >= state.extruderTarget - 2 &&
                    state.extruderTarget > 0);
    if (extI != s_prev.extruderTemp || extAt != s_prev.extruderAtTemp) {
        snprintf(buf, sizeof(buf), "%3d", extI);
        tft.setTextFont(6);
        tft.setTextColor(extAt ? COLOR_SUCCESS : COLOR_TEXT, COLOR_SURFACE);
        tft.setCursor(PANEL_PAD, TMP_HOTEND_Y);
        tft.print(buf);
        s_prev.extruderTemp   = extI;
        s_prev.extruderAtTemp = extAt;
    }
    if (extTgt != s_prev.extruderTarget) {
        snprintf(buf, sizeof(buf), "/%3d C ", extTgt);
        tft.fillRect(PANEL_PAD, TGT_HOTEND_Y - 1, DIVIDER_X - PANEL_PAD - 4, 20, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.setCursor(PANEL_PAD, TGT_HOTEND_Y);
        tft.print(buf);
        s_prev.extruderTarget = extTgt;
    }

    // ── Bed temperature ──────────────────────────────
    int  bedI   = (int)state.bedTemp;
    int  bedTgt = (int)state.bedTarget;
    bool bedAt  = (state.bedTemp >= state.bedTarget - 2 &&
                   state.bedTarget > 0);
    if (bedI != s_prev.bedTemp || bedAt != s_prev.bedAtTemp) {
        snprintf(buf, sizeof(buf), "%3d", bedI);
        tft.setTextFont(6);
        tft.setTextColor(bedAt ? COLOR_SUCCESS : COLOR_TEXT, COLOR_SURFACE);
        tft.setCursor(PANEL_PAD, TMP_BED_Y);
        tft.print(buf);
        s_prev.bedTemp   = bedI;
        s_prev.bedAtTemp = bedAt;
    }
    if (bedTgt != s_prev.bedTarget) {
        snprintf(buf, sizeof(buf), "/%3d C ", bedTgt);
        tft.fillRect(PANEL_PAD, TGT_BED_Y - 1, DIVIDER_X - PANEL_PAD - 4, 20, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.setCursor(PANEL_PAD, TGT_BED_Y);
        tft.print(buf);
        s_prev.bedTarget = bedTgt;
    }

    // ── Fan ─────────────────────────────────────────
    int fanPct = (int)(state.fanSpeed * 100);
    if (fanPct != s_prev.fanPct) {
        snprintf(buf, sizeof(buf), "FAN %3d%%", fanPct);
        tft.fillRect(PANEL_PAD, FAN_Y - 1, DIVIDER_X - PANEL_PAD - 4, 20, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.setCursor(PANEL_PAD, FAN_Y);
        tft.print(buf);
        s_prev.fanPct = fanPct;
    }

    // ── Status badge ────────────────────────────────
    if (state.status != s_prev.status) {
        uint16_t badgeColor = statusColor(state.status);
        tft.fillRoundRect(RP_X, RB_Y, RB_W, RB_H, 5, badgeColor);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_BG, badgeColor);
        tft.drawString(statusLabel(state.status),
                       RP_X + RB_W / 2, RB_Y + 13);
        s_prev.status = state.status;
    }

    // ── Filename ─────────────────────────────────────
    if (strncmp(state.filename, s_prev.filename, sizeof(s_prev.filename)) != 0) {
        char fname[28];
        truncateFilename(state.filename[0] ? state.filename : "No File", fname, 26);
        tft.fillRect(RP_X, 182, RP_W, 16, COLOR_SURFACE);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(state.filename[0] ? COLOR_TEXT : COLOR_TEXT_DIM, COLOR_SURFACE);
        tft.drawString(fname, RP_XC, 190);
        strlcpy(s_prev.filename, state.filename, sizeof(s_prev.filename));
    }

    // ── Progress bar + percentage ─────────────────────
    int progPer10 = (int)(state.progress * 1000.0f); // %.1f precision
    if (progPer10 != s_prev.progressPer10) {
        drawProgressBar(RP_X, 200, RP_W, 21, state.progress,
                        COLOR_ACCENT, COLOR_BG);
        snprintf(buf, sizeof(buf), "%5.1f%%", state.progress * 100.0f);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT);
        tft.drawString(buf, RP_XC, 210);
        s_prev.progressPer10 = progPer10;
    }

    // ── Elapsed time (updates every second) ──────────
    if (state.printDuration != s_prev.printDuration) {
        String elapsed = formatTime(state.printDuration);
        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%-9s", elapsed.c_str());
        tft.setTextFont(2);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.drawString(tbuf, RP_X, 250);
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
        tft.drawString(tbuf, RP_XC, 250);
        s_prev.remaining = rem;
    }

    // ── Z position ───────────────────────────────────
    long posZ100 = (long)(state.posZ * 100.0f);
    if (posZ100 != s_prev.posZ100) {
        snprintf(buf, sizeof(buf), "%6.2f mm", state.posZ);
        tft.setTextFont(2);
        tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
        tft.setTextDatum(MR_DATUM);
        tft.drawString(buf, RP_XF, 250);
        s_prev.posZ100 = posZ100;
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
    s_prev.fanPct         = -1;
    s_prev.status         = PrinterStatus::DISCONNECTED;
    s_prev.filename[0]    = '\x01';
    s_prev.progressPer10  = -1;
    s_prev.printDuration  = -1;
    s_prev.remaining      = -1;
    s_prev.posZ100        = -999999;
}

void draw(const PrinterState& state) {
    if (!s_initialized) {
        drawStaticLayout();
        s_initialized = true;
    }
    drawDynamic(state);
}

ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client) {
    (void)x; (void)y; (void)state; (void)client;

    // reset firmware button
    if(x >= RF_X && x <= RF_X + RB_W && y >= RB_Y && y <= RB_Y + RB_H) {
        client.firmwareRestart();
    }

    return ScreenID::HOME;
}

} // namespace ScreenHome
