#pragma once
// ─────────────────────────────────────────────────────
// Thumbnail rendering – standalone, no external libraries.
// PNG decoding is built-in via upng (bundled in project).
// Supports: RGB, RGBA, Greyscale, Indexed PNG, bit-depth 8.
// ─────────────────────────────────────────────────────

namespace Thumbnail {
    // Draw the thumbnail at (x,y) fitting within maxW×maxH pixels.
    // If download/decode succeeds returns true; otherwise draws the
    // placeholder icon and returns false.
    // url must be a full HTTP URL (from PrinterState::thumbnailPath).
    bool drawOrPlaceholder(const char* url, int x, int y, int maxW, int maxH);

    // Draw the "no preview" placeholder icon unconditionally.
    void drawPlaceholder(int x, int y, int w, int h);

    // Draw the standby (idle / not printing) icon unconditionally.
    void drawStandby(int x, int y, int w, int h);
}
