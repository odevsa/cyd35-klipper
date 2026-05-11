#include "screen_files.h"
#include "lang.h"
#include "ui_theme.h"
#include "thumbnail.h"

extern MoonrakerClient g_moonraker;

// ─────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────
static const int FM_HEADER_H  = 35;
static const int FM_SCROLL_W  = 40;
static const int FM_LIST_Y    = CONTENT_Y + FM_HEADER_H;
static const int FM_LIST_H    = NAV_BAR_Y - FM_LIST_Y;
static const int FM_ITEM_H    = 40;
static const int FM_VISIBLE   = FM_LIST_H / FM_ITEM_H;
static const int FM_LIST_W    = SCREEN_W  - FM_SCROLL_W;
static const int FM_SCROLL_X  = FM_LIST_W;
static const int SB_BTN_H     = 35;

// Header button bounding boxes
static const struct { int x, y, w, h; } BTN_HDR_CLOSE = {
    SCREEN_W - 38, CONTENT_Y + 3, 35, FM_HEADER_H - 6
};
static const struct { int x, y, w, h; } BTN_HDR_UP = {
    3, CONTENT_Y + 3, 35, FM_HEADER_H - 6
};

// ── Confirm dialog layout ────────────────────────────
static const int CONF_X       = PANEL_SPACING;
static const int CONF_Y       = CONTENT_Y + PANEL_SPACING;
static const int CONF_W       = SCREEN_W - (PANEL_SPACING * 2);
static const int CONF_H       = CONTENT_H - (PANEL_SPACING * 2);
static const int CONF_HDR_H   = 28;
static const int CONF_THUMB_X = CONF_X + PANEL_SPACING;
static const int CONF_THUMB_Y = CONF_Y + CONF_HDR_H + PANEL_SPACING;
static const int CONF_THUMB_W = 180;
static const int CONF_THUMB_H = CONF_H - CONF_HDR_H - (PANEL_SPACING * 2);
static const int CONF_INFO_X  = CONF_THUMB_X + CONF_THUMB_W + PANEL_SPACING;
static const int CONF_INFO_W  = CONF_X + CONF_W - CONF_INFO_X - PANEL_SPACING;
static const int CONF_BTN_H   = 34;
static const int CONF_BTN_Y   = CONF_Y + CONF_H - CONF_BTN_H - PANEL_SPACING;
static const int CONF_BTN_W   = (CONF_INFO_W - PANEL_SPACING) / 2;

static const struct { int x, y, w, h; } BTN_CONF_CLOSE = {
    CONF_X + CONF_W - 30, CONF_Y + 2, 27, CONF_HDR_H - 4
};
static const struct { int x, y, w, h; } BTN_CONF_PRINT = {
    CONF_INFO_X, CONF_BTN_Y, CONF_BTN_W, CONF_BTN_H
};
static const struct { int x, y, w, h; } BTN_CONF_CANCEL = {
    CONF_INFO_X + CONF_BTN_W + PANEL_SPACING, CONF_BTN_Y, CONF_BTN_W, CONF_BTN_H
};

// ─────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────
static bool     s_initialized  = false;
static bool     s_listDirty    = false;
static bool     s_confirmDirty = false;
static ScreenID s_prevScreen   = ScreenID::HOME;

static char      s_currentPath[128] = "";
static FileEntry s_entries[FM_MAX_ENTRIES];
static int       s_entryCount  = 0;
static int       s_scrollTop   = 0;   // index of topmost visible item

// ── Drag / touch tracking ────────────────────────────
static bool          s_touchHeld     = false;
static int           s_touchHeldY    = 0;
static unsigned long s_touchHeldMs   = 0;
static bool          s_isDragging    = false;
static float         s_dragAccum     = 0.0f;  // sub-item drag accumulator
static float         s_dragTotal     = 0.0f;  // total drag since press
static int           s_pressedIdx    = -1;    // item index pressed

#define FM_DRAG_THRESHOLD   8     // pixels before drag mode starts
#define FM_TOUCH_TIMEOUT_MS 500   // ms without new touch = gesture ended

// ── Confirm dialog ────────────────────────────────────
static bool         s_showConfirm  = false;
static char         s_selectedName[FM_NAME_LEN] = "";
static char         s_selectedPath[192]          = "";
static FileMetadata s_selMeta;

// ─────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────

static void clampScroll() {
    int maxSc = s_entryCount - FM_VISIBLE;
    if (maxSc < 0) maxSc = 0;
    if (s_scrollTop < 0)     s_scrollTop = 0;
    if (s_scrollTop > maxSc) s_scrollTop = maxSc;
}

static void loadDirectory() {
    int count = 0;
    g_moonraker.listDirectory(s_currentPath, s_entries, FM_MAX_ENTRIES, count);
    s_entryCount = count;
    s_scrollTop  = 0;
    s_dragAccum  = 0.0f;
}

static const char* humanReadableFileSize(long bytes){
  static char sizeBuf[16];
  if (bytes >= 1048576) {
      float mb = bytes / 1048576.0f;
      snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", mb);
  } else {
      long kb = bytes / 1024L;
      snprintf(sizeBuf, sizeof(sizeBuf), "%ld KB", kb);
  }
  return sizeBuf;
}

// ─────────────────────────────────────────────────────
// Icon helpers
// ─────────────────────────────────────────────────────

static void drawFolderIcon(int cx, int cy, uint16_t col) {
    int bw = 20, bh = 14;
    int th = 2;
    int fx = cx - bw / 2, fy = cy - bh / 2;
    tft.fillRoundRect(fx, fy + th, bw, bh - th, 2, col);
    tft.fillRoundRect(fx, fy, bw / 2, bh / 2, 2, col);
}

static void drawFileIcon(int cx, int cy, uint16_t col) {
    int fw = 14, fh = 16, ear = 6;
    int fx = cx - fw / 2, fy = cy - fh / 2;
    tft.fillRect(fx, fy, fw - ear, fh, col);
    tft.fillRect(fx, fy + ear, fw, fh - ear, col);
    tft.drawLine(fx + fw - ear, fy, fx + fw - 1, fy + ear -1, col);
}

// ↑ with horizontal base = "go up one directory"
static void drawUpDirIcon(int cx, int cy, uint16_t col) {
    tft.fillTriangle(cx, cy - 7, cx - 5, cy - 2, cx + 5, cy - 2, col);
    tft.fillRect(cx - 2, cy - 2, 4, 5, col);
    tft.fillRect(cx - 5, cy + 3, 10, 2, col);
}

// ─────────────────────────────────────────────────────
// Scrollbar
// ─────────────────────────────────────────────────────

static void drawScrollbar() {
    int trackY = FM_LIST_Y + SB_BTN_H;
    int trackH = FM_LIST_H - (2 * SB_BTN_H);

    // Background strip
    tft.fillRect(FM_SCROLL_X, FM_LIST_Y, FM_SCROLL_W, FM_LIST_H, COLOR_NAV_BG);

    int ax = FM_SCROLL_X + FM_SCROLL_W / 2;

    // ▲ top button
    bool canUp = (s_scrollTop > 0);
    tft.fillRect(FM_SCROLL_X + 1, FM_LIST_Y, FM_SCROLL_W - 1, SB_BTN_H,
                 canUp ? COLOR_SURFACE : COLOR_NAV_BG);
    uint16_t uCol = canUp ? COLOR_TEXT_DIM : COLOR_TEXT_MUTED;
    int ay = FM_LIST_Y + SB_BTN_H / 2;
    tft.fillTriangle(ax, ay - 5, ax - 5, ay + 3, ax + 5, ay + 3, uCol);

    // ▼ bottom button
    int bBtnY = FM_LIST_Y + FM_LIST_H - SB_BTN_H;
    bool canDn = (s_scrollTop < s_entryCount - FM_VISIBLE);
    tft.fillRect(FM_SCROLL_X + 1, bBtnY, FM_SCROLL_W - 1, SB_BTN_H,
                 canDn ? COLOR_SURFACE : COLOR_NAV_BG);
    uint16_t dCol = canDn ? COLOR_TEXT_DIM : COLOR_TEXT_MUTED;
    int by = bBtnY + SB_BTN_H / 2;
    tft.fillTriangle(ax, by + 5, ax - 5, by - 3, ax + 5, by - 3, dCol);

    // Thumb
    if (s_entryCount > FM_VISIBLE && trackH > 0) {
        int thumbH = max(16, trackH * FM_VISIBLE / s_entryCount);
        int maxSc  = max(1, s_entryCount - FM_VISIBLE);
        int thumbY = trackY + (trackH - thumbH) * s_scrollTop / maxSc;
        tft.fillRect(FM_SCROLL_X + 3, thumbY + 2,
                     FM_SCROLL_W - 6, thumbH - 4, COLOR_TEXT_MUTED);
    }

    // Borders
    tft.drawRect(FM_SCROLL_X, FM_LIST_Y, FM_SCROLL_W, FM_LIST_H, COLOR_DIVIDER);
    tft.drawRect(FM_SCROLL_X, FM_LIST_Y + SB_BTN_H, FM_SCROLL_W, bBtnY - (FM_LIST_Y + SB_BTN_H), COLOR_DIVIDER);
}

// ─────────────────────────────────────────────────────
// Header
// ─────────────────────────────────────────────────────

static void drawHeader(bool inSubDir) {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, FM_HEADER_H, COLOR_NAV_BG);

    const char* rawTitle = s_currentPath[0] ? s_currentPath : L.LBL_FILES;
    int titleX = inSubDir ? BTN_HDR_UP.x + BTN_HDR_UP.w + 6 : 8;
    int titleW = BTN_HDR_CLOSE.x - titleX - 4;
    char dispTitle[34];
    truncateFilename(rawTitle, dispTitle, 32);
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_NAV_BG);
    tft.drawString(dispTitle, titleX + titleW / 2, CONTENT_Y + FM_HEADER_H / 2);

    // Up Dir
    if (inSubDir) {
        tft.fillRoundRect(BTN_HDR_UP.x, BTN_HDR_UP.y,
                          BTN_HDR_UP.w, BTN_HDR_UP.h, 3, COLOR_ACCENT);
        drawUpDirIcon(BTN_HDR_UP.x + BTN_HDR_UP.w / 2,
                      BTN_HDR_UP.y + BTN_HDR_UP.h / 2,
                      COLOR_SURFACE);
    }

    // Close [X]
    drawButton(BTN_HDR_CLOSE.x, BTN_HDR_CLOSE.y, BTN_HDR_CLOSE.w, BTN_HDR_CLOSE.h, "X", COLOR_ERROR, COLOR_TEXT, 2);

    tft.drawRect(0, CONTENT_Y, SCREEN_W, FM_HEADER_H, COLOR_DIVIDER);
}

// ─────────────────────────────────────────────────────
// File list
// ─────────────────────────────────────────────────────

static void drawFileList() {
    for (int row = 0; row < FM_VISIBLE; row++) {
        int idx  = s_scrollTop + row;
        int rowY = FM_LIST_Y + row * FM_ITEM_H;

        if (idx < s_entryCount) {
            const FileEntry& e = s_entries[idx];
            uint16_t bg  = COLOR_BG;
            uint16_t txt = COLOR_TEXT;
            uint16_t ico = e.isDir ? COLOR_ACCENT : COLOR_TEXT_DIM;

            if (e.name[0] == '.'){
              ico = darkenRGB565(ico, 0.35f);
              txt = darkenRGB565(txt, 0.35f);
            }

            tft.fillRect(0, rowY, FM_LIST_W, FM_ITEM_H - 1, bg);
            tft.drawFastHLine(0, rowY + FM_ITEM_H - 1, FM_LIST_W, COLOR_DIVIDER);

            int iconCX = 22, iconCY = rowY + FM_ITEM_H / 2;
            if (e.isDir) drawFolderIcon(iconCX, iconCY, ico);
            else         drawFileIcon  (iconCX, iconCY, ico);

            char fname[32];
            truncateFilename(e.name, fname, 30);
            tft.setTextFont(2);
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(txt, bg);
            tft.drawString(fname, 44, rowY + FM_ITEM_H / 2);

            if (e.isDir) {
                tft.setTextDatum(MR_DATUM);
                tft.setTextColor(COLOR_TEXT_DIM, bg);
                tft.drawString(">", FM_LIST_W - 6, rowY + FM_ITEM_H / 2);
            } else if (e.sizeBytes > 0) {
                tft.setTextDatum(MR_DATUM);
                tft.setTextColor(COLOR_TEXT_DIM, bg);
                tft.drawString(humanReadableFileSize(e.sizeBytes), FM_LIST_W - 6, rowY + FM_ITEM_H / 2);
            }
        } else {
            tft.fillRect(0, rowY, FM_LIST_W, FM_ITEM_H, COLOR_BG);
        }
    }

    tft.drawRect(0, FM_LIST_Y, SCREEN_W, FM_LIST_H, COLOR_DIVIDER);

    drawScrollbar();
}

// ─────────────────────────────────────────────────────
// Confirm / metadata dialog
// ─────────────────────────────────────────────────────

static void drawInfoLine(int x, int& y, const char* label, const char* value,
                         uint16_t valCol = COLOR_TEXT) {
    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.drawString(label, x, y);
    
    tft.setTextFont(2);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(valCol, COLOR_SURFACE);
    tft.drawString(value, CONF_X + CONF_W - PANEL_SPACING, y);
    y += 17;
}

static void drawConfirmDialog() {
    tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, 0x0841);

    // Dialog panel
    tft.fillRoundRect(CONF_X, CONF_Y, CONF_W, CONF_H, PANEL_ROUNDED, COLOR_SURFACE);
    
    // Header bar
    tft.fillRoundRect(CONF_X, CONF_Y, CONF_W, CONF_HDR_H, PANEL_ROUNDED, COLOR_NAV_BG);
    tft.fillRect(CONF_X, CONF_Y + CONF_HDR_H / 2, CONF_W, CONF_HDR_H / 2 + 1, COLOR_NAV_BG);
    
    // Border
    tft.drawRoundRect(CONF_X, CONF_Y, CONF_W, CONF_H, PANEL_ROUNDED, COLOR_DIVIDER);

    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_NAV_BG);
    tft.drawString(L.MSG_PRINT_CONFIRM, CONF_X + 8, CONF_Y + CONF_HDR_H / 2);

    // Header close [X]
    drawButton(BTN_CONF_CLOSE.x, BTN_CONF_CLOSE.y, BTN_CONF_CLOSE.w, BTN_CONF_CLOSE.h, "X", COLOR_ERROR, COLOR_TEXT, 2);

    // Thumbnail
    if (s_selMeta.valid && s_selMeta.thumbUrl[0]) {
        Thumbnail::drawOrPlaceholder(s_selMeta.thumbUrl,
            CONF_THUMB_X, CONF_THUMB_Y, CONF_THUMB_W, CONF_THUMB_H);
    } else {
        Thumbnail::drawPlaceholder(CONF_THUMB_X, CONF_THUMB_Y,
                                   CONF_THUMB_W, CONF_THUMB_H);
    }

    // Filename
    char fname[38];
    truncateFilename(s_selectedName, fname, 36);
    tft.setTextFont(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_TEXT, COLOR_SURFACE);
    tft.drawString(fname, CONF_INFO_X, CONF_THUMB_Y);
    tft.setTextColor(COLOR_TEXT);
    tft.drawString(fname, CONF_INFO_X + 1, CONF_THUMB_Y);

    // Folder Location
    char fpath[42];
    truncateFilename(sanitizeDirPath(s_selectedPath), fpath, 40);
    tft.setTextFont(1);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_SURFACE);
    tft.drawString(fpath, CONF_INFO_X, CONF_THUMB_Y + 18);

    // Metadata rows
    int infoY = CONF_THUMB_Y + 40;
    char buf[36];

    if (s_selMeta.valid) {
        if (s_selMeta.sizeBytes > 0) {
            drawInfoLine(CONF_INFO_X, infoY, L.LBL_FILE_SIZE, humanReadableFileSize(s_selMeta.sizeBytes), COLOR_TEXT);
        }
        if (s_selMeta.estimatedTime > 0) {
            String t = formatTime(s_selMeta.estimatedTime);
            drawInfoLine(CONF_INFO_X, infoY, L.LBL_PRINT_TIME, t.c_str(), COLOR_TEXT);
        }
        if (s_selMeta.material[0]) {
            drawInfoLine(CONF_INFO_X, infoY, L.LBL_MATERIAL,
                         s_selMeta.material, COLOR_ACCENT);
        }
        if (s_selMeta.nozzleTemp > 0) {
            snprintf(buf, sizeof(buf), "%d C", s_selMeta.nozzleTemp);
            drawInfoLine(CONF_INFO_X, infoY, L.LBL_HOTEND, buf, COLOR_WARNING);
        }
        if (s_selMeta.bedTemp > 0) {
            snprintf(buf, sizeof(buf), "%d C", s_selMeta.bedTemp);
            drawInfoLine(CONF_INFO_X, infoY, L.LBL_BED, buf, COLOR_WARNING);
        }
        if (s_selMeta.filamentGrams > 0.5f) {
            snprintf(buf, sizeof(buf), "%.1fg / %.0fmm",
                     s_selMeta.filamentGrams, s_selMeta.filamentMM);
            drawInfoLine(CONF_INFO_X, infoY, L.LBL_FILAMENT, buf, COLOR_TEXT);
        } else if (s_selMeta.filamentMM > 0.5f) {
            snprintf(buf, sizeof(buf), "%.0f mm", s_selMeta.filamentMM);
            drawInfoLine(CONF_INFO_X, infoY, L.LBL_FILAMENT, buf, COLOR_TEXT);
        }
    }

    // Action buttons
    drawButton(BTN_CONF_PRINT.x,  BTN_CONF_PRINT.y,
               BTN_CONF_PRINT.w,  BTN_CONF_PRINT.h,
               L.BTN_PRINT,  COLOR_SUCCESS, COLOR_TEXT, 2);
    drawButton(BTN_CONF_CANCEL.x, BTN_CONF_CANCEL.y,
               BTN_CONF_CANCEL.w, BTN_CONF_CANCEL.h,
               L.BTN_CANCEL, COLOR_ERROR,   COLOR_TEXT, 2);
}

// ─────────────────────────────────────────────────────
// Public interface
// ─────────────────────────────────────────────────────
namespace ScreenFiles {

void setPreviousScreen(ScreenID prev) {
    s_prevScreen = prev;
}

void resetInitialized() {
    s_initialized    = false;
    s_listDirty      = false;
    s_confirmDirty   = false;
    s_showConfirm    = false;
    s_currentPath[0] = '\0';
    s_scrollTop      = 0;
    s_entryCount     = 0;
    s_touchHeld      = false;
    s_isDragging     = false;
    s_pressedIdx     = -1;
    s_dragAccum      = 0.0f;
    s_dragTotal      = 0.0f;
}

void draw(const PrinterState& state) {
    (void)state;

    if (!s_initialized) {
        tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_BG);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
        tft.drawString(L.LBL_LOADING, SCREEN_W / 2, CONTENT_Y + CONTENT_H / 2);

        loadDirectory();

        tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_BG);
        drawHeader(s_currentPath[0] != '\0');
        drawFileList();
        s_initialized  = true;
        s_listDirty    = false;
        s_confirmDirty = false;
    }

    if (s_listDirty) {
        s_listDirty = false;
        drawFileList();
    }

    if (s_confirmDirty) {
        s_confirmDirty = false;
        drawConfirmDialog();
    }
}

ScreenID handleTouch(int x, int y, PrinterState& state, MoonrakerClient& client) {
    (void)state; (void)client;
    unsigned long now = millis();

    // ── Confirm dialog: only button presses handled immediately ──────
    if (s_showConfirm) {
        if (inRect(x, y, BTN_CONF_CLOSE.x, BTN_CONF_CLOSE.y,
                   BTN_CONF_CLOSE.w, BTN_CONF_CLOSE.h) ||
            inRect(x, y, BTN_CONF_CANCEL.x, BTN_CONF_CANCEL.y,
                   BTN_CONF_CANCEL.w, BTN_CONF_CANCEL.h)) {
            s_showConfirm = false;
            s_initialized = false;
        } else if (inRect(x, y, BTN_CONF_PRINT.x, BTN_CONF_PRINT.y,
                          BTN_CONF_PRINT.w, BTN_CONF_PRINT.h)) {
            client.startPrint(s_selectedPath);
            s_showConfirm = false;
            return s_prevScreen;
        }
        return ScreenID::FILES;
    }

    // ── Header: immediate actions on press ───────────────────────────
    if (inRect(x, y, BTN_HDR_CLOSE.x, BTN_HDR_CLOSE.y,
               BTN_HDR_CLOSE.w, BTN_HDR_CLOSE.h)) {
        return s_prevScreen;
    }
    if (s_currentPath[0] &&
        inRect(x, y, BTN_HDR_UP.x, BTN_HDR_UP.y,
               BTN_HDR_UP.w, BTN_HDR_UP.h)) {
        char* slash = strrchr(s_currentPath, '/');
        if (slash) *slash = '\0';
        else        s_currentPath[0] = '\0';
        s_initialized = false;
        s_touchHeld   = false;
        return ScreenID::FILES;
    }

    // ── Scrollbar arrow buttons: immediate on press ──────────────────
    if (x >= FM_SCROLL_X) {
        if (y >= FM_LIST_Y && y < FM_LIST_Y + SB_BTN_H && s_scrollTop > 0) {
            s_scrollTop--;
            clampScroll();
            s_listDirty = true;
        } else if (y >= FM_LIST_Y + FM_LIST_H - SB_BTN_H &&
                   y <  FM_LIST_Y + FM_LIST_H &&
                   s_scrollTop < s_entryCount - FM_VISIBLE) {
            s_scrollTop++;
            clampScroll();
            s_listDirty = true;
        }
        return ScreenID::FILES;
    }

    // ── List area: drag tracking ─────────────────────────────────────
    if (y >= FM_LIST_Y && y < FM_LIST_Y + FM_LIST_H) {
        bool sameGesture = s_touchHeld &&
                           (now - s_touchHeldMs) < FM_TOUCH_TIMEOUT_MS;

        if (!sameGesture) {
            // New press
            s_touchHeld   = true;
            s_touchHeldY  = y;
            s_touchHeldMs = now;
            s_isDragging  = false;
            s_dragAccum   = 0.0f;
            s_dragTotal   = 0.0f;
            s_pressedIdx  = s_scrollTop + (y - FM_LIST_Y) / FM_ITEM_H;
        } else {
            // Continuation – accumulate drag
            float delta = (float)(s_touchHeldY - y);
            s_dragTotal += fabsf(delta);
            if (s_dragTotal >= FM_DRAG_THRESHOLD) s_isDragging = true;

            if (s_isDragging) {
                s_dragAccum += delta;
                int itemDelta = (int)(s_dragAccum / FM_ITEM_H);
                if (itemDelta != 0) {
                    s_scrollTop += itemDelta;
                    s_dragAccum -= itemDelta * FM_ITEM_H;
                    clampScroll();
                    s_listDirty = true;
                }
            }
            s_touchHeldY  = y;
            s_touchHeldMs = now;
        }
    }

    return ScreenID::FILES;
}

ScreenID handleRelease(PrinterState& state, MoonrakerClient& client) {
    (void)state;

    if (!s_touchHeld) return ScreenID::FILES;

    bool wasDragging = s_isDragging;
    int  idx         = s_pressedIdx;
    s_touchHeld  = false;
    s_isDragging = false;
    s_pressedIdx = -1;
    s_dragAccum  = 0.0f;
    s_dragTotal  = 0.0f;

    if (s_showConfirm) return ScreenID::FILES;

    // Only open on release if it was a tap (not a drag)
    if (!wasDragging && idx >= 0 && idx < s_entryCount) {
        const FileEntry& e = s_entries[idx];
        if (e.isDir) {
            // Navigate into directory
            if (s_currentPath[0]) {
                size_t len = strlen(s_currentPath);
                if (len + 1 + strlen(e.name) < sizeof(s_currentPath)) {
                    s_currentPath[len] = '/';
                    strlcpy(s_currentPath + len + 1, e.name,
                            sizeof(s_currentPath) - len - 1);
                }
            } else {
                strlcpy(s_currentPath, e.name, sizeof(s_currentPath));
            }
            s_initialized = false;
        } else {
            // Select file → fetch metadata → show confirm
            strlcpy(s_selectedName, e.name, sizeof(s_selectedName));
            if (s_currentPath[0]) {
                snprintf(s_selectedPath, sizeof(s_selectedPath),
                         "%s/%s", s_currentPath, e.name);
            } else {
                strlcpy(s_selectedPath, e.name, sizeof(s_selectedPath));
            }
            s_selMeta = FileMetadata();
            client.getFileInfo(s_selectedPath, s_selMeta);
            s_showConfirm  = true;
            s_confirmDirty = true;
        }
    }

    return ScreenID::FILES;
}

}
