#pragma once
// ─────────────────────────────────────────────────────
// TempGraph – lightweight rolling temperature history chart.
// Stores up to TEMP_GRAPH_SAMPLES samples, drawn as a line
// chart with the target temperature shown as a dashed rule.
//
// Usage:
//   TempGraph g;
//   g.push(current, target);   // call once per poll
//   g.draw(x, y, w, h);        // render into the bounding box
//   g.reset();                  // clear history (screen switch)
// ─────────────────────────────────────────────────────

#include <stdint.h>
#include "config.h"
#include "ui_theme.h"

// Hotend graph colours
#define TGRAPH_HOT_LINE   0xF800  // red-ish
#define TGRAPH_HOT_TGT    0xFB20  // amber dashed

// Bed graph colours
#define TGRAPH_BED_LINE   0xFB20  // orange-ish
#define TGRAPH_BED_TGT    0xFD60  // amber dashed

struct TempGraph {
    float  samples[TEMP_GRAPH_SAMPLES];
    int    head   = 0;
    int    count  = 0;
    float  target = 0.0f;

    void reset() { head = 0; count = 0; target = 0.0f; }

    void push(float current, float tgt) {
        samples[head] = current;
        head = (head + 1) % TEMP_GRAPH_SAMPLES;
        if (count < TEMP_GRAPH_SAMPLES) count++;
        target = tgt;
    }

    // Draw the chart inside the bounding box (x,y,w,h).
    // lineCol = colour for the temperature trace.
    // tgtCol  = colour for the target dashed rule.
    void draw(int x, int y, int w, int h,
              uint16_t lineCol, uint16_t tgtCol) const {

        // Background + border
        tft.fillRoundRect(x, y, w, h, PANEL_ROUNDED, COLOR_BG);
        tft.drawRoundRect(x, y, w, h, PANEL_ROUNDED, COLOR_DIVIDER);

        if (count < 2) return;

        const float tempRange = TEMP_GRAPH_MAX_TEMP - TEMP_GRAPH_MIN_TEMP;

        // Inner drawing area (1 px inset for the border)
        int ix = x + 1, iy = y + 1, iw = w - 2, ih = h - 2;
        if (iw < 2 || ih < 2) return;

        // Helper lambda: temperature → screen Y
        auto tempToY = [&](float t) -> int {
            int py = iy + ih - 1 - (int)((t - TEMP_GRAPH_MIN_TEMP) / tempRange * (float)(ih - 1));
            if (py < iy)        py = iy;
            if (py > iy + ih - 1) py = iy + ih - 1;
            return py;
        };

        // ── Target dashed horizontal line ────────────────
        if (target > 0.0f) {
            int ty = tempToY(target);
            for (int px = ix; px < ix + iw; px += 4) {
                tft.drawPixel(px, ty, tgtCol);
                if (px + 1 < ix + iw) tft.drawPixel(px + 1, ty, tgtCol);
            }
        }

        // ── Temperature trace ────────────────────────────
        // Iterate samples oldest→newest
        int first = (count < TEMP_GRAPH_SAMPLES)
                    ? 0
                    : head;   // head is the next write slot = oldest when full

        int prevX = -1, prevY = -1;
        for (int i = 0; i < count; i++) {
            int si = (first + i) % TEMP_GRAPH_SAMPLES;
            // Map sample index to X within the inner area
            int px = ix + i * (iw - 1) / (count - 1);
            int py = tempToY(samples[si]);
            if (prevX >= 0) {
                tft.drawLine(prevX, prevY, px, py, lineCol);
            }
            prevX = px; prevY = py;
        }
    }
};

// ─────────────────────────────────────────────────────
// Global instances – defined once in cyd35-klipper.ino,
// shared by all screens so history survives tab switches.
// ─────────────────────────────────────────────────────
extern TempGraph g_hotendGraph;
extern TempGraph g_bedGraph;
