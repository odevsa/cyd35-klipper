#pragma once
// ─────────────────────────────────────────────────────
// upng – minimal PNG decoder, no external dependencies.
// Supports: 8-bit Grayscale, RGB, RGBA (most Klipper thumbnails).
// Colour types 0, 2, 3 (palette), 4, 6.  Bit depth 8 only.
// Interlaced (Adam7) images are NOT supported (thumbnails never use it).
//
// Public domain / MIT – based on the original upng by Sean T. Barrett
// and later minimized for embedded use.
// ─────────────────────────────────────────────────────
#include <stdint.h>
#include <stddef.h>

typedef enum {
    UPNG_EOK          =  0,
    UPNG_ENOMEM       =  1,
    UPNG_ENOTFOUND    =  2,
    UPNG_ENOTPNG      =  3,
    UPNG_EMALFORMED   =  4,
    UPNG_EUNSUPPORTED =  5,
} upng_error_t;

typedef enum {
    UPNG_RGBA8  = 0,
    UPNG_RGB8   = 1,
    UPNG_GREY8  = 2,
    UPNG_GREYA8 = 3,
    UPNG_INDEXED= 4,
} upng_format_t;

typedef struct upng_t upng_t;

// Create/destroy
upng_t*      upng_new_from_bytes(const uint8_t* buf, size_t size);
void         upng_free(upng_t* upng);

// Decode (fills internal pixel buffer)
upng_error_t upng_decode(upng_t* upng);

// Accessors
unsigned     upng_get_width (const upng_t* upng);
unsigned     upng_get_height(const upng_t* upng);
upng_format_t upng_get_format(const upng_t* upng);
const uint8_t* upng_get_buffer(const upng_t* upng); // decoded pixels
size_t         upng_get_size  (const upng_t* upng); // buffer byte count
upng_error_t   upng_get_error (const upng_t* upng);

// ─────────────────────────────────────────────────────
// Streaming / low-memory render API.
//
// upng_decode_stream() inflates the PNG data, then calls row_cb() once per
// source row with the fully un-filtered pixel data for that row.
// The caller only needs to keep 2 × row_stride bytes in RAM (not the entire
// decoded image), making it possible to render a 400×300 RGBA image with
// only ~3.2 KB of line buffers instead of 480 KB.
//
//   row     – 0-based row index (0 = top)
//   pixels  – pointer to width*bpp bytes for this row (temporary – do not retain)
//   bpp     – bytes per pixel AFTER palette expansion (always 3 for INDEXED)
//   width   – image width in pixels
//   userdata – value passed through from upng_decode_stream()
// ─────────────────────────────────────────────────────
typedef void (*upng_row_cb)(unsigned row,
                            const uint8_t* pixels,
                            unsigned bpp,
                            unsigned width,
                            void* userdata);

upng_error_t upng_decode_stream(upng_t* upng, upng_row_cb cb, void* userdata);
