#include "thumbnail.h"
#include "upng.h"
#include "lang.h"
#include "ui_theme.h"
#include "config.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <ctype.h>

// Max bytes to download for a thumbnail (400x300 PNG is typically 15–80 KB).
#ifndef THUMBNAIL_MAX_BYTES
#define THUMBNAIL_MAX_BYTES  131072   // 128 KB ceiling
#endif

// ─────────────────────────────────────────────────────
// Standby icon (3D printer silhouette) – shown when not printing.
// ─────────────────────────────────────────────────────
void Thumbnail::drawStandby(int x, int y, int w, int h) {
    tft.fillRoundRect(x, y, w, h, PANEL_ROUNDED, COLOR_BG);
    tft.drawRoundRect(x, y, w, h, PANEL_ROUNDED, COLOR_DIVIDER);

    uint16_t col = COLOR_TEXT_DIM;
    int cx = x + w / 2;

    // Scale unit – 8 units fit the icon height; reserve 20 px bottom for label
    int avail = h - 20;
    int s = min(w * 3 / 4, avail) / 7;
    if (s < 4) s = 4;

    // Printer body (5s wide × 5s tall), centred, biased up to leave room for label
    int fw = s * 5;
    int fh = s * 5;
    int fx = cx - fw / 2;
    int fy = y + (avail - fh) / 2;

    // ── Left / right vertical rails (2 px wide each) ──────────────
    tft.fillRect(fx,          fy, 2, fh, col);
    tft.fillRect(fx + fw - 2, fy, 2, fh, col);

    // ── Top gantry beam (3 px tall, full width) ───────────────────
    tft.fillRect(fx, fy, fw, 3, col);

    // ── Bottom base (3 px tall, full width) ───────────────────────
    tft.fillRect(fx, fy + fh - 3, fw, 3, col);

    // ── Print bed (3 px, inset 2 px from rails, ~1 s from bottom) ─
    int bedY = fy + fh - s - 1;
    tft.fillRect(fx + 2, bedY, fw - 4, 3, col);

    // ── Carriage body (1.5s × 1s, centred, hanging from gantry) ──
    int headW = s + s / 2;
    int headH = s;
    int headX = cx - headW / 2;
    int headY = fy + 3;
    tft.drawRect(headX, headY, headW, headH, col);

    // ── Nozzle (narrow trapezoid below carriage) ───────────────────
    int nozBaseY = headY + headH;
    int nozTipY  = nozBaseY + s / 2;
    int nozTipX  = cx;
    int inset    = headW / 4;
    // base of nozzle
    tft.drawFastHLine(headX + inset, nozBaseY, headW - inset * 2, col);
    // left/right sides converging to tip
    tft.drawLine(headX + inset,           nozBaseY, nozTipX, nozTipY, col);
    tft.drawLine(headX + headW - inset,   nozBaseY, nozTipX, nozTipY, col);

    // ── Extruded filament dot at nozzle tip ────────────────────────
    tft.fillCircle(nozTipX, nozTipY + 2, 2, col);

    // ── Printed object on bed (3 stacked layers = small pyramid) ──
    int objW  = s + s / 2;
    int objX  = cx - objW / 2;
    int layH  = 4;
    int gap   = 2;
    int baseY = bedY - 1;
    for (int i = 0; i < 3; i++) {
        int lw = objW - i * (objW / 4);
        int lx = cx - lw / 2;
        int ly = baseY - i * (layH + gap) - layH;
        tft.fillRect(lx, ly, lw, layH, col);
    }

    // ── Label ─────────────────────────────────────────────────────
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString(L.LBL_CHOOSE_FILE, cx, y + h - 16);
}

// ─────────────────────────────────────────────────────
// "No preview" placeholder – no external library needed.
// ─────────────────────────────────────────────────────
void Thumbnail::drawPlaceholder(int x, int y, int w, int h) {
    tft.fillRect(x, y, w, h, COLOR_BG);
    tft.drawRect(x, y, w, h, COLOR_DIVIDER);

    int cx = x + w / 2;
    int cy = y + h / 2;
    int ir = min(w, h) / 5;
    int fx = cx - ir*2, fy = cy - ir - 4, fw = ir*4, fh = ir*2 + 8;

    // Draw a stylized "broken image" icon: a jagged mountain range with a sun/moon partly below the horizon.
    tft.drawRoundRect(fx, fy, fw, fh, 4, COLOR_TEXT_DIM);
    tft.drawLine(fx + 2,        fy + fh - 3, fx + fw / 3,   fy + 4,      COLOR_TEXT_DIM);
    tft.drawLine(fx + fw / 3,   fy + 4,      fx + fw * 2/3, fy + fh - 3, COLOR_TEXT_DIM);
    tft.drawLine(fx + fw * 2/3, fy + fh - 3, fx + fw - 8,  fy + fh/2,   COLOR_TEXT_DIM);
    tft.drawLine(fx + fw - 8,   fy + fh/2,   fx + fw - 3,  fy + fh - 3, COLOR_TEXT_DIM);
    tft.drawCircle(fx + fw - 9, fy + 8, 5, COLOR_TEXT_DIM);

    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    tft.drawString(L.NO_PREVIEW, cx, y + h - 16);
}

// ─────────────────────────────────────────────────────
// Download helper – tolerates missing Content-Length.
// Returns heap-allocated buffer (caller must free) or nullptr.
// ─────────────────────────────────────────────────────
static uint8_t* downloadBytes(const char* url, int* out_size) {
    *out_size = 0;
    HTTPClient http;
    http.setTimeout(10000);
    http.begin(url);
    int code = http.GET();
#ifdef DEBUG
    Serial.printf("[Thumb] download HTTP code: %d  content-length: %d\n",
        code, (int)http.getSize());
#endif
    if (code != HTTP_CODE_OK) { http.end(); return nullptr; }

    int contentLen  = (int)http.getSize();
    bool unknownLen = (contentLen <= 0);
    int  bufSize    = unknownLen ? THUMBNAIL_MAX_BYTES : contentLen;
    if (!unknownLen && contentLen > THUMBNAIL_MAX_BYTES) { http.end(); return nullptr; }

    uint8_t* buf = (uint8_t*)malloc((size_t)bufSize);
    if (!buf) { http.end(); return nullptr; }

    WiFiClient* stream = http.getStreamPtr();
    int got = 0;
    unsigned long deadline = millis() + 10000;
    while (got < bufSize && millis() < deadline) {
        int avail = stream->available();
        if (avail > 0) {
            int n = stream->readBytes(buf + got, min(avail, bufSize - got));
            if (n > 0) got += n;
        } else if (!stream->connected()) {
            break;
        }
    }
    http.end();

    if (got == 0) { free(buf); return nullptr; }
    // Reject truncated downloads – inflate would fail on partial data.
    if (!unknownLen && got < contentLen) {
#ifdef DEBUG
        Serial.printf("[Thumb] download incomplete: got %d of %d bytes\n", got, contentLen);
#endif
        free(buf); return nullptr;
    }
    *out_size = got;
    return buf;
}

// ─────────────────────────────────────────────────────
// URL-encode the path/query part of `url` but preserve the scheme and
// host portion and keep '/' characters unencoded. Returns a heap-allocated
// string which the caller must free, or nullptr on allocation failure.
// ─────────────────────────────────────────────────────
static char* url_encode_preserve_slash(const char* url) {
    if (!url) return nullptr;
    // Find the start of the path after scheme://host[:port]
    const char* p = strstr(url, "://");
    const char* path = url;
    if (p) {
        // Skip past ://
        p += 3;
        // Find first '/' after host
        const char* s = strchr(p, '/');
        if (s) path = s; else return nullptr; // no path to encode
    } else {
        // No scheme — treat whole string as path
        path = url;
    }

    size_t base_len = (size_t)(path - url);
    size_t out_cap = base_len + strlen(path) * 3 + 1;
    char* out = (char*)malloc(out_cap);
    if (!out) return nullptr;

    // Copy base (scheme+host)
    if (base_len > 0) memcpy(out, url, base_len);
    size_t o = base_len;

    // Encode path but keep '/'
    for (const char* q = path; *q; ++q) {
        unsigned char ch = (unsigned char)*q;
        if (ch == '/') {
            out[o++] = '/';
        } else if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out[o++] = ch;
        } else {
            // percent-encode
            if (o + 3 >= out_cap) {
                // should not happen, but be safe
                char* nxt = (char*)realloc(out, out_cap + 64);
                if (!nxt) { free(out); return nullptr; }
                out = nxt; out_cap += 64;
            }
            static const char* hex = "0123456789ABCDEF";
            out[o++] = '%';
            out[o++] = hex[(ch >> 4) & 0xF];
            out[o++] = hex[ch & 0xF];
        }
    }
    out[o] = '\0';
    return out;
}

// ─────────────────────────────────────────────────────
// Streaming render context – passed through upng_row_cb
// ─────────────────────────────────────────────────────
struct RenderCtx {
    int   ox, oy;       // top-left of drawn image on screen
    int   dw, dh;       // destination (scaled) dimensions
    unsigned iw, ih;    // source image dimensions
    unsigned bpp;       // source bytes per pixel (after palette expansion)
    static uint16_t lineBuf[480];
};
uint16_t RenderCtx::lineBuf[480];

static void streamRowCb(unsigned src_row, const uint8_t* pixels,
                        unsigned bpp, unsigned width, void* userdata) {
    RenderCtx* ctx = (RenderCtx*)userdata;
#ifdef DEBUG
    if (src_row == 0) {
        Serial.printf("[Thumb] cb row0: bpp=%u width=%u dw=%d dh=%d\n", bpp, width, ctx->dw, ctx->dh);
    }
#endif

    // Skip source rows that produce the same destination row as the previous one.
    // For a 400×300 source scaled to ≤210 rows, roughly every 2 source rows map
    // to the same dest row.  Avoids redundant pushImage calls.
    unsigned dst_row_start = (unsigned)((uint32_t)src_row       * (uint32_t)ctx->dh / ctx->ih);
    unsigned dst_row_end   = (unsigned)((uint32_t)(src_row + 1) * (uint32_t)ctx->dh / ctx->ih);
    if (dst_row_start >= dst_row_end) return;   // this source row adds nothing new
    if (dst_row_end > (unsigned)ctx->dh) dst_row_end = (unsigned)ctx->dh;

    // Build the scaled pixel line (same for all dst rows of this src row)
    for (int col = 0; col < ctx->dw; col++) {
        int src_col = (int)((size_t)col * ctx->iw / (size_t)ctx->dw);
        const uint8_t* px = pixels + src_col * bpp;
        uint8_t r, g, b;
        switch (bpp) {
            case 1: r = g = b = px[0];                       break;
            case 2: r = g = b = px[0];                       break;
            case 3: r = px[0]; g = px[1]; b = px[2];         break;
            case 4: r = px[0]; g = px[1]; b = px[2];         break;
            default: r = g = b = 0;                          break;
        }
        // TFT_eSPI pushImage with ESP32_DMA sends bytes in memory order (little-endian).
        // Byte-swap so the display receives the high byte (R bits) first.
        uint16_t c = ((uint16_t)(r & 0xF8) << 8) |
                     ((uint16_t)(g & 0xFC) << 3) |
                     (b >> 3);
        ctx->lineBuf[col] = __builtin_bswap16(c);
    }

    for (unsigned dr = dst_row_start; dr < dst_row_end; dr++) {
        tft.pushImage(ctx->ox, ctx->oy + (int)dr, ctx->dw, 1, ctx->lineBuf);
    }
}

// ─────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────
bool Thumbnail::drawOrPlaceholder(const char* url, int x, int y, int maxW, int maxH) {
    // Fill background and draw border FIRST so the frame is visible
    // while the image streams in row-by-row.
    tft.fillRoundRect(x, y, maxW, maxH, PANEL_ROUNDED, THUMBNAIL_BG_COLOR);
    tft.drawRoundRect(x, y, maxW, maxH, PANEL_ROUNDED, COLOR_DIVIDER);
    
    if (!url || url[0] == '\0' || WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
        Serial.printf("[Thumb] skip: url=%s wifi=%d\n",
            url ? (url[0] ? url : "(empty)") : "(null)",
            (int)WiFi.status());
#endif
        drawPlaceholder(x, y, maxW, maxH);
        return false;
    }

#ifdef DEBUG
    Serial.printf("[Thumb] downloading: %s\n", url);
#endif

    int dataLen = 0;
    char* enc = url_encode_preserve_slash(url);
    uint8_t* buf = downloadBytes(enc ? enc : url, &dataLen);
    if (enc) free(enc);
    if (!buf) {
#ifdef DEBUG
        Serial.println("[Thumb] download failed");
#endif
        drawPlaceholder(x, y, maxW, maxH);
        return false;
    }

#ifdef DEBUG
    Serial.printf("[Thumb] got %d bytes, parsing header...\n", dataLen);
#endif

    // Read width/height directly from the PNG IHDR bytes – no decode needed.
    // PNG layout: 8-byte signature | 4-byte len | 4-byte "IHDR" | 4-byte W | 4-byte H | ...
    // Width  = bytes [16..19], Height = bytes [20..23], big-endian.
    if (dataLen < 24) {
#ifdef DEBUG
        Serial.println("[Thumb] file too small to be PNG");
#endif
        free(buf);
        drawPlaceholder(x, y, maxW, maxH);
        return false;
    }
    static const uint8_t PNG_SIG[8] = {137,80,78,71,13,10,26,10};
    if (memcmp(buf, PNG_SIG, 8) != 0) {
#ifdef DEBUG
        Serial.println("[Thumb] not a PNG");
#endif
        free(buf);
        drawPlaceholder(x, y, maxW, maxH);
        return false;
    }
    unsigned iw = ((unsigned)buf[16]<<24)|((unsigned)buf[17]<<16)|
                  ((unsigned)buf[18]<<8 )| (unsigned)buf[19];
    unsigned ih = ((unsigned)buf[20]<<24)|((unsigned)buf[21]<<16)|
                  ((unsigned)buf[22]<<8 )| (unsigned)buf[23];

#ifdef DEBUG
    Serial.printf("[Thumb] image %ux%u\n", iw, ih);
#endif

    if (iw == 0 || ih == 0) {
#ifdef DEBUG
        Serial.println("[Thumb] bad image dimensions");
#endif
        free(buf);
        drawPlaceholder(x, y, maxW, maxH);
        return false;
    }

    upng_t* png = upng_new_from_bytes(buf, (size_t)dataLen);

    if (!png) {
        free(buf);
        drawPlaceholder(x, y, maxW, maxH);
        return false;
    }

    // Compute proportional scale-to-fit destination size
    int dw, dh;
    if ((int)iw <= maxW && (int)ih <= maxH) {
        dw = (int)iw;
        dh = (int)ih;
    } else if ((int)iw * maxH >= (int)ih * maxW) {
        dw = maxW;
        dh = (int)((uint32_t)ih * (uint32_t)maxW / iw);
    } else {
        dh = maxH;
        dw = (int)((uint32_t)iw * (uint32_t)maxH / ih);
    }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;

    static RenderCtx ctx;
    ctx.ox  = x + 1 + (maxW - dw) / 2;
    ctx.oy  = y + 1 + (maxH - dh) / 2;
    ctx.dw  = dw - 2;
    ctx.dh  = dh - 2;
    ctx.iw  = iw;
    ctx.ih  = ih;
    ctx.bpp = 0;

    upng_error_t stream_err = upng_decode_stream(png, streamRowCb, &ctx);
    upng_free(png);
    free(buf);  // safe to free only after upng_free

    if (stream_err != UPNG_EOK) {
#ifdef DEBUG
        Serial.printf("[Thumb] stream error %d\n", (int)stream_err);
#endif
        drawPlaceholder(x, y, maxW, maxH);
        return false;
    }

#ifdef DEBUG
    Serial.printf("[Thumb] rendered %dx%d -> %dx%d\n", iw, ih, dw, dh);
#endif
    return true;
}
