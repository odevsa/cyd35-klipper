// upng.cpp – minimal PNG decoder for ESP32 / Arduino
// No external dependencies; uses a self-contained DEFLATE inflate.
// Supports bit-depth 8: colour types 0 (Grey), 2 (RGB), 3 (Indexed/Palette),
//                       4 (Grey+Alpha), 6 (RGBA).
// No interlace support (Adam7).  Filter types 0-4 supported.
// ─────────────────────────────────────────────────────
#include "upng.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef DEBUG
#include <Arduino.h>
#endif

// ════════════════════════════════════════════════════════════════════════
//  DEFLATE inflate – canonical Huffman decoder (puff.c algorithm,
//  public domain by Mark Adler).  Correct for all code lengths 1-15.
// ════════════════════════════════════════════════════════════════════════

#define PUFF_MAXBITS  15
#define PUFF_MAXSYMS 320   // fits lit/len(288), dist(32), codelen(19)

// Canonical Huffman table – puff.c style.
// sorted[] holds symbols ordered by (code_length, symbol_value).
typedef struct {
    int      maxbits;
    int      count[PUFF_MAXBITS + 1];  // count[i] = #symbols of length i
    uint16_t sorted[PUFF_MAXSYMS];
} tinfl_huff_table;

typedef struct {
    const uint8_t *m_pIn_buf_cur, *m_pIn_buf_end;
    uint8_t       *m_pOut_buf_cur, *m_pOut_buf_end, *m_pOut_buf_start;
    uint32_t       m_bit_buf;
    int            m_num_bits;
    tinfl_huff_table m_tables[2];   // [0]=lit/len  [1]=dist
} tinfl_decompressor;

static void tinfl_init(tinfl_decompressor* r) { memset(r, 0, sizeof(*r)); }

// Bit-reader macros (same interface as before, used in ring decompress too).
#define TINFL_GET_BYTE(ptr, var) \
    do { if ((ptr)->m_pIn_buf_cur >= (ptr)->m_pIn_buf_end) return -1; \
         (var) = *(ptr)->m_pIn_buf_cur++; } while(0)

#define TINFL_NEED_BITS(ptr, n) \
    do { while ((ptr)->m_num_bits < (int)(n)) { \
        unsigned b; TINFL_GET_BYTE(ptr, b); \
        (ptr)->m_bit_buf |= (uint32_t)b << (ptr)->m_num_bits; \
        (ptr)->m_num_bits += 8; \
    } } while(0)

#define TINFL_PEEK_BITS(ptr, n)  ((ptr)->m_bit_buf & ((1u<<(n))-1u))
#define TINFL_SKIP_BITS(ptr, n)  do { (ptr)->m_bit_buf >>= (n); (ptr)->m_num_bits -= (n); } while(0)
#define TINFL_GET_BITS(ptr, var, n) do { TINFL_NEED_BITS(ptr,n); (var)=TINFL_PEEK_BITS(ptr,n); TINFL_SKIP_BITS(ptr,n); } while(0)

static int tinfl_build_table(tinfl_huff_table* h, const uint8_t* pCode_size, int num_syms) {
    int i, b;
    h->maxbits = 0;
    memset(h->count, 0, sizeof(h->count));
    for (i = 0; i < num_syms; i++) {
        if (pCode_size[i] > 0 && pCode_size[i] <= PUFF_MAXBITS) {
            h->count[pCode_size[i]]++;
            if (pCode_size[i] > h->maxbits) h->maxbits = pCode_size[i];
        }
    }
    // Build sorted[]: symbols sorted by (length, symbol_value) using counting sort.
    int idx[PUFF_MAXBITS + 2] = {};
    for (b = 1; b <= h->maxbits; b++) idx[b + 1] = idx[b] + h->count[b];
    int pos[PUFF_MAXBITS + 2];
    for (b = 0; b <= PUFF_MAXBITS + 1; b++) pos[b] = idx[b];
    for (i = 0; i < num_syms; i++) {
        b = pCode_size[i];
        if (b > 0 && b <= PUFF_MAXBITS) h->sorted[pos[b]++] = (uint16_t)i;
    }
    return 0;
}

// Decode one symbol using puff.c algorithm – correct for ALL code lengths.
// Reads bits one at a time (LSB-first from stream → builds code MSB-first).
static int tinfl_decode_symbol(tinfl_decompressor* r, tinfl_huff_table* h) {
    int code = 0, first = 0, index = 0, cnt;
    for (int b = 1; b <= h->maxbits; b++) {
        if (r->m_num_bits < 1) {
            if (r->m_pIn_buf_cur >= r->m_pIn_buf_end) return -1;
            r->m_bit_buf |= (uint32_t)(*r->m_pIn_buf_cur++) << r->m_num_bits;
            r->m_num_bits += 8;
        }
        code |= (int)(r->m_bit_buf & 1);
        r->m_bit_buf >>= 1; r->m_num_bits--;
        cnt = h->count[b];
        if (code - cnt < first)
            return (int)h->sorted[index + code - first];
        index += cnt;
        first = (first + cnt) << 1;
        code  <<= 1;
    }
    return -1;
}

static const uint8_t  s_length_extra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t s_length_base[29] ={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t  s_dist_extra[30]  ={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static const uint32_t s_dist_base[30]   ={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};

// Returns number of bytes written to out, or -1 on error.
// r is static to avoid stack overflow on ESP32 (default loop stack 8 KB).
static int tinfl_decompress(const uint8_t* in, int in_size, uint8_t* out, int out_max) {
    static tinfl_decompressor r; tinfl_init(&r);
    r.m_pIn_buf_cur  = in; r.m_pIn_buf_end  = in + in_size;
    r.m_pOut_buf_cur = out; r.m_pOut_buf_end = out + out_max; r.m_pOut_buf_start = out;

    // Skip zlib header (2 bytes: CMF + FLG)
    if (in_size < 2) return -1;
    r.m_pIn_buf_cur += 2;

    int final_block = 0;
    while (!final_block) {
        TINFL_NEED_BITS(&r, 3);
        int bfinal = TINFL_PEEK_BITS(&r, 1); TINFL_SKIP_BITS(&r, 1);
        int btype  = TINFL_PEEK_BITS(&r, 2); TINFL_SKIP_BITS(&r, 2);
        final_block = bfinal;

        if (btype == 0) { // stored
            r.m_num_bits = 0; r.m_bit_buf = 0;
            uint8_t hdr[4];
            for(int i=0;i<4;i++) TINFL_GET_BYTE(&r, hdr[i]);
            int len=hdr[0]|(hdr[1]<<8);
            int nlen=hdr[2]|(hdr[3]<<8);
            if ((len^nlen)!=0xFFFF) return -1;
            while(len--) {
                if(r.m_pIn_buf_cur>=r.m_pIn_buf_end) return -1;
                if(r.m_pOut_buf_cur>=r.m_pOut_buf_end) return -1;
                *r.m_pOut_buf_cur++ = *r.m_pIn_buf_cur++;
            }
        } else if (btype == 1 || btype == 2) { // fixed or dynamic Huffman
            static const uint8_t s_fixed_lit[288]={
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8};
            static const uint8_t s_fixed_dist[32]={
                5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};

            uint8_t lit_sizes[288];
            uint8_t dist_sizes[32];
            int num_lit=288, num_dist=32;

            if (btype == 2) {
                uint32_t hlit, hdist, hclen;
                TINFL_GET_BITS(&r, hlit,  5); hlit+=257;
                TINFL_GET_BITS(&r, hdist, 5); hdist+=1;
                TINFL_GET_BITS(&r, hclen, 4); hclen+=4;

                static const uint8_t code_len_order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                uint8_t code_len_sizes[19]={};
                for(uint32_t i=0;i<hclen;i++) {
                    uint32_t v; TINFL_GET_BITS(&r, v, 3);
                    code_len_sizes[code_len_order[i]]=(uint8_t)v;
                }
                static tinfl_huff_table code_len_tbl;
                tinfl_build_table(&code_len_tbl, code_len_sizes, 19);

                static uint8_t combined[288 + 32];
                memset(combined, 0, sizeof(combined));
                uint32_t total=hlit+hdist, prev=0;
                for(uint32_t i=0;i<total;) {
                    int sym=tinfl_decode_symbol(&r, &code_len_tbl);
                    if(sym<0) return -1;
                    if(sym<=15) { combined[i++]=(uint8_t)sym; prev=sym; }
                    else if(sym==16) {
                        uint32_t rep; TINFL_GET_BITS(&r, rep, 2); rep+=3;
                        while(rep--) combined[i++]=(uint8_t)prev;
                    } else if(sym==17) {
                        uint32_t rep; TINFL_GET_BITS(&r, rep, 3); rep+=3;
                        while(rep--) combined[i++]=0; prev=0;
                    } else {
                        uint32_t rep; TINFL_GET_BITS(&r, rep, 7); rep+=11;
                        while(rep--) combined[i++]=0; prev=0;
                    }
                }
                memcpy(lit_sizes,  combined,       hlit);
                memcpy(dist_sizes, combined+hlit,  hdist);
                num_lit=hlit; num_dist=hdist;
                tinfl_build_table(&r.m_tables[0], lit_sizes,  num_lit);
                tinfl_build_table(&r.m_tables[1], dist_sizes, num_dist);
            } else {
                tinfl_build_table(&r.m_tables[0], s_fixed_lit,  288);
                tinfl_build_table(&r.m_tables[1], s_fixed_dist, 32);
            }

            for(;;) {
                int sym=tinfl_decode_symbol(&r, &r.m_tables[0]);
                if(sym<0) return -1;
                if(sym==256) break;
                if(sym<256) {
                    if(r.m_pOut_buf_cur>=r.m_pOut_buf_end) return -1;
                    *r.m_pOut_buf_cur++ = (uint8_t)sym;
                } else {
                    int len_idx=sym-257;
                    if(len_idx>=29) return -1;
                    uint32_t match_len = s_length_base[len_idx];
                    if(s_length_extra[len_idx]) { uint32_t ex; TINFL_GET_BITS(&r, ex, s_length_extra[len_idx]); match_len+=ex; }
                    int dist_sym=tinfl_decode_symbol(&r, &r.m_tables[1]);
                    if(dist_sym<0||dist_sym>=30) return -1;
                    uint32_t match_dist = s_dist_base[dist_sym];
                    if(s_dist_extra[dist_sym]) { uint32_t ex; TINFL_GET_BITS(&r, ex, s_dist_extra[dist_sym]); match_dist+=ex; }
                    uint8_t* pSrc = r.m_pOut_buf_cur - match_dist;
                    if(pSrc < r.m_pOut_buf_start) return -1;
                    while(match_len--) {
                        if(r.m_pOut_buf_cur>=r.m_pOut_buf_end) return -1;
                        *r.m_pOut_buf_cur++ = *pSrc++;
                    }
                }
            }
        } else return -1; // btype == 3: reserved error
    }
    return (int)(r.m_pOut_buf_cur - out);
}

// ════════════════════════════════════════════════════════════════════════
//  Ring-buffer streaming inflate
//  Uses a static 32 KB circular DEFLATE window for back-references.
//  Row data is accumulated directly in ctx->cur (no ring re-read needed).
// ════════════════════════════════════════════════════════════════════════

#define RING_SIZE 32768u
#define RING_MASK (RING_SIZE - 1u)

static uint8_t  s_ring[RING_SIZE];  // static – BSS, DEFLATE back-ref window only
static uint32_t s_ring_wpos;        // absolute write cursor

#define RING_READ_AT(p) (s_ring[(p) & RING_MASK])

// Context passed through ring inflate to the row callback.
struct ring_ctx_t {
    upng_row_cb  cb;
    void*        userdata;
    unsigned     width, height;
    uint8_t      bpp_raw, bpp_out, color_type;
    uint8_t*     cur;        // stride bytes – filled directly as inflate runs
    uint8_t*     prev;       // stride bytes – previous reconstructed row
    uint8_t*     pal_row;    // width*3 for palette images, else nullptr
    uint8_t      palette[256][3];
    bool         has_palette;
    size_t       stride;     // 1 + width*bpp_raw
    size_t       row_pos;    // bytes filled in cur for current row
    unsigned     row_idx;
};

static int paeth(int a, int b, int c);  // forward declaration

// cur is fully filled; apply PNG filter in-place, call cb, swap cur↔prev.
// Returns false on bad filter byte.
static bool ring_flush_row(ring_ctx_t* ctx) {
    // cur[0] is the filter byte; cur[1..stride-1] are raw pixel bytes.
    uint8_t  filter = ctx->cur[0];
    uint8_t* row    = ctx->cur  + 1;
    uint8_t* prv    = ctx->prev + 1;
    size_t   npix   = (size_t)ctx->width * ctx->bpp_raw;

    switch (filter) {
        case 0: break;
        case 1:
            for (size_t x = ctx->bpp_raw; x < npix; x++)
                row[x] = (uint8_t)(row[x] + row[x - ctx->bpp_raw]);
            break;
        case 2:
            for (size_t x = 0; x < npix; x++)
                row[x] = (uint8_t)(row[x] + prv[x]);
            break;
        case 3:
            for (size_t x = 0; x < npix; x++) {
                uint8_t a = (x >= ctx->bpp_raw) ? row[x - ctx->bpp_raw] : 0;
                row[x] = (uint8_t)(row[x] + ((a + prv[x]) >> 1));
            }
            break;
        case 4:
            for (size_t x = 0; x < npix; x++) {
                uint8_t a = (x >= ctx->bpp_raw) ? row[x - ctx->bpp_raw] : 0;
                uint8_t b = prv[x];
                uint8_t c = (x >= ctx->bpp_raw) ? prv[x - ctx->bpp_raw] : 0;
                row[x] = (uint8_t)(row[x] + paeth(a, b, c));
            }
            break;
        default:
#ifdef DEBUG
            Serial.printf("[upng] bad filter=%d row=%u\n", filter, ctx->row_idx);
#endif
            return false;
    }

    const uint8_t* out_row = row;
    if (ctx->color_type == 3 && ctx->has_palette) {
        for (unsigned x = 0; x < ctx->width; x++) {
            ctx->pal_row[x*3]   = ctx->palette[row[x]][0];
            ctx->pal_row[x*3+1] = ctx->palette[row[x]][1];
            ctx->pal_row[x*3+2] = ctx->palette[row[x]][2];
        }
        out_row = ctx->pal_row;
    }
    ctx->cb(ctx->row_idx, out_row, ctx->bpp_out, ctx->width, ctx->userdata);
    ctx->row_idx++;
    ctx->row_pos = 0;
    // Swap cur↔prev for next row
    uint8_t* tmp = ctx->cur; ctx->cur = ctx->prev; ctx->prev = tmp;
    return true;
}

// Emit one byte: write to DEFLATE ring window AND to cur row buffer.
// When cur is full, flush the completed row.
// Returns false on filter error.
static bool ring_emit_and_flush(uint8_t b, ring_ctx_t* ctx) {
    // Always update the DEFLATE back-reference window.
    s_ring[s_ring_wpos & RING_MASK] = b;
    s_ring_wpos++;

    // Fill the current PNG row buffer directly (no ring re-read).
    if (ctx->row_idx < ctx->height) {
        ctx->cur[ctx->row_pos++] = b;
        if (ctx->row_pos == ctx->stride) {
            if (!ring_flush_row(ctx)) return false;
            // row_pos reset to 0 inside ring_flush_row
        }
    }
    return true;
}

// Ring-buffer streaming DEFLATE decompress.
// Same algorithm as tinfl_decompress but output goes to s_ring.
// Returns 0 on success, -1 on error.
static int tinfl_ring_decompress(const uint8_t* in, int in_size, ring_ctx_t* ctx) {
    static tinfl_decompressor r;
    tinfl_init(&r);
    r.m_pIn_buf_cur = in;
    r.m_pIn_buf_end = in + in_size;

    s_ring_wpos    = 0;
    ctx->row_pos   = 0;
    ctx->row_idx   = 0;
    memset(ctx->prev, 0, ctx->stride);   // previous row zeroed for row 0
    memset(s_ring, 0, sizeof(s_ring));   // clear stale back-ref data

    if (in_size < 2) return -1;
    r.m_pIn_buf_cur += 2; // skip zlib CMF+FLG

    int final_block = 0;
    while (!final_block) {
        TINFL_NEED_BITS(&r, 3);
        int bfinal = TINFL_PEEK_BITS(&r, 1); TINFL_SKIP_BITS(&r, 1);
        int btype  = TINFL_PEEK_BITS(&r, 2); TINFL_SKIP_BITS(&r, 2);
        final_block = bfinal;

        if (btype == 0) { // stored block
            r.m_num_bits = 0; r.m_bit_buf = 0;
            uint8_t hdr[4];
            for (int i = 0; i < 4; i++) TINFL_GET_BYTE(&r, hdr[i]);
            int blen  = hdr[0] | (hdr[1] << 8);
            int bnlen = hdr[2] | (hdr[3] << 8);
            if ((blen ^ bnlen) != 0xFFFF) { 
#ifdef DEBUG
                Serial.println("[upng] stored blk len/nlen mismatch");
#endif
                return -1; 
            }
            while (blen--) {
                if (r.m_pIn_buf_cur >= r.m_pIn_buf_end) { 
#ifdef DEBUG
                    Serial.println("[upng] stored blk eof");
#endif
                    return -1; 
                }
                if (!ring_emit_and_flush(*r.m_pIn_buf_cur++, ctx)) { 
#ifdef DEBUG
                    Serial.println("[upng] stored blk filter err");
#endif
                    return -1; 
                }
            }
        } else if (btype == 1 || btype == 2) {
            static const uint8_t s_fixed_lit[288]={
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
                8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
                7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8};
            static const uint8_t s_fixed_dist[32]={
                5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};

            uint8_t lit_sizes[288];
            uint8_t dist_sizes[32];
            int num_lit = 288, num_dist = 32;

            if (btype == 2) {
                uint32_t hlit, hdist, hclen;
                TINFL_GET_BITS(&r, hlit,  5); hlit  += 257;
                TINFL_GET_BITS(&r, hdist, 5); hdist += 1;
                TINFL_GET_BITS(&r, hclen, 4); hclen += 4;

                static const uint8_t cl_order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                uint8_t cl_sizes[19] = {};
                for (uint32_t i = 0; i < hclen; i++) {
                    uint32_t v; TINFL_GET_BITS(&r, v, 3);
                    cl_sizes[cl_order[i]] = (uint8_t)v;
                }
                static tinfl_huff_table cl_tbl;
                tinfl_build_table(&cl_tbl, cl_sizes, 19);

                static uint8_t combined[288 + 32];
                memset(combined, 0, sizeof(combined));
                uint32_t total = hlit + hdist, cprev = 0;
                for (uint32_t i = 0; i < total; ) {
                    int sym = tinfl_decode_symbol(&r, &cl_tbl);
                    if (sym < 0) { 
#ifdef DEBUG
                        Serial.printf("[upng] cl_tbl decode fail i=%u total=%u\n", i, total);
#endif
                        return -1; 
                    }
                    if (sym <= 15) { combined[i++] = (uint8_t)sym; cprev = sym; }
                    else if (sym == 16) { uint32_t rep; TINFL_GET_BITS(&r, rep, 2); rep += 3; while (rep-- && i < total) combined[i++] = (uint8_t)cprev; }
                    else if (sym == 17) { uint32_t rep; TINFL_GET_BITS(&r, rep, 3); rep += 3; while (rep-- && i < total) combined[i++] = 0; cprev = 0; }
                    else               { uint32_t rep; TINFL_GET_BITS(&r, rep, 7); rep += 11; while (rep-- && i < total) combined[i++] = 0; cprev = 0; }
                }
                memcpy(lit_sizes,  combined,       hlit);
                memcpy(dist_sizes, combined + hlit, hdist);
                num_lit = hlit; num_dist = hdist;
                tinfl_build_table(&r.m_tables[0], lit_sizes,  num_lit);
                tinfl_build_table(&r.m_tables[1], dist_sizes, num_dist);
            } else {
                tinfl_build_table(&r.m_tables[0], s_fixed_lit,  288);
                tinfl_build_table(&r.m_tables[1], s_fixed_dist, 32);
            }

            for (;;) {
                int sym = tinfl_decode_symbol(&r, &r.m_tables[0]);
                if (sym < 0) { 
#ifdef DEBUG
                    Serial.printf("[upng] lit sym fail wpos=%u row=%u\n", s_ring_wpos, ctx->row_idx);
#endif
                    return -1; 
                }
                if (sym == 256) break;
                if (sym < 256) {
                    if (!ring_emit_and_flush((uint8_t)sym, ctx)) { 
#ifdef DEBUG
                        Serial.printf("[upng] filter fail sym=%d\n", sym);
#endif
                        return -1; 
                    }
                } else {
                    int len_idx = sym - 257;
                    if (len_idx >= 29) { 
#ifdef DEBUG
                        Serial.printf("[upng] bad len_idx=%d\n", len_idx);
#endif
                        return -1; 
                    }
                    uint32_t match_len = s_length_base[len_idx];
                    if (s_length_extra[len_idx]) { uint32_t ex; TINFL_GET_BITS(&r, ex, s_length_extra[len_idx]); match_len += ex; }
                    int dist_sym = tinfl_decode_symbol(&r, &r.m_tables[1]);
                    if (dist_sym < 0 || dist_sym >= 30) { 
#ifdef DEBUG
                        Serial.printf("[upng] bad dist_sym=%d\n", dist_sym);
#endif
                        return -1; 
                    }
                    uint32_t match_dist = s_dist_base[dist_sym];
                    if (s_dist_extra[dist_sym]) { uint32_t ex; TINFL_GET_BITS(&r, ex, s_dist_extra[dist_sym]); match_dist += ex; }
                    if (match_dist > s_ring_wpos) { 
#ifdef DEBUG
                        Serial.printf("[upng] bad backref dist=%u wpos=%u\n", match_dist, s_ring_wpos);
#endif
                        return -1; 
                    }
                    uint32_t src_pos = s_ring_wpos - match_dist;
                    while (match_len--) {
                        if (!ring_emit_and_flush(RING_READ_AT(src_pos++), ctx)) { 
#ifdef DEBUG
                            Serial.println("[upng] backref filter fail");
#endif
                            return -1; 
                        }
                    }
                }
            }
        } else { 
#ifdef DEBUG
            Serial.printf("[upng] bad btype=%d\n", btype);
#endif
            return -1; 
        }
    }
    return 0;
}

// ════════════════════════════════════════════════════════════════════════
//  PNG parser
// ════════════════════════════════════════════════════════════════════════

struct upng_t {
    unsigned     width, height;
    upng_format_t format;
    uint8_t      color_type, bit_depth;
    uint8_t*     buffer;   // decoded pixel data
    size_t       size;
    uint8_t*     idat;     // concatenated IDAT data
    size_t       idat_size, idat_cap;
    uint8_t      palette[256][3]; // for indexed colour
    bool         has_palette;
    upng_error_t error;

    const uint8_t* src;
    size_t         src_size;
};

static uint32_t png_read_u32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}

static uint32_t png_crc_table[256];
static bool     png_crc_init = false;

static void init_crc_table() {
    if (png_crc_init) return;
    for(int n=0;n<256;n++){
        uint32_t c=n;
        for(int k=0;k<8;k++) c=(c&1)?(0xEDB88320^(c>>1)):(c>>1);
        png_crc_table[n]=c;
    }
    png_crc_init=true;
}

static uint32_t png_crc(const uint8_t* buf, size_t len) {
    init_crc_table();
    uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<len;i++) c=png_crc_table[(c^buf[i])&0xFF]^(c>>8);
    return c^0xFFFFFFFFu;
}

upng_t* upng_new_from_bytes(const uint8_t* buf, size_t size) {
    upng_t* u=(upng_t*)calloc(1,sizeof(upng_t));
    if(!u) return nullptr;
    u->src=buf; u->src_size=size; u->error=UPNG_EOK;
    return u;
}

void upng_free(upng_t* u) {
    if(!u) return;
    free(u->buffer);
    free(u->idat);
    free(u);
}

unsigned      upng_get_width (const upng_t* u) { return u->width; }
unsigned      upng_get_height(const upng_t* u) { return u->height; }
upng_format_t upng_get_format(const upng_t* u) { return u->format; }
const uint8_t* upng_get_buffer(const upng_t* u) { return u->buffer; }
size_t         upng_get_size  (const upng_t* u) { return u->size; }
upng_error_t   upng_get_error (const upng_t* u) { return u->error; }

static int paeth(int a,int b,int c){
    int p=a+b-c, pa=abs(p-a), pb=abs(p-b), pc=abs(p-c);
    return (pa<=pb&&pa<=pc)?a:(pb<=pc?b:c);
}

upng_error_t upng_decode(upng_t* u) {
    if(!u) return UPNG_ENOTFOUND;
    const uint8_t* p=u->src; size_t rem=u->src_size;

    // PNG signature
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    if(rem<8||memcmp(p,sig,8)!=0){ u->error=UPNG_ENOTPNG; return u->error; }
    p+=8; rem-=8;

    uint8_t bpp=0; // bytes per pixel after decode
    u->idat_size=0; u->idat_cap=0; u->idat=nullptr;

    while(rem>=12) {
        uint32_t len=png_read_u32(p);   p+=4; rem-=4;
        char type[5]; memcpy(type,p,4); type[4]=0; p+=4; rem-=4;
        if(rem < len+4) { u->error=UPNG_EMALFORMED; return u->error; }

        if(strcmp(type,"IHDR")==0) {
            if(len!=13){ u->error=UPNG_EMALFORMED; return u->error; }
            u->width     = png_read_u32(p);
            u->height    = png_read_u32(p+4);
            u->bit_depth = p[8];
            u->color_type= p[9];
            if(u->bit_depth!=8){ u->error=UPNG_EUNSUPPORTED; return u->error; }
            if(p[10]!=0||p[11]!=0||p[12]!=0){ u->error=UPNG_EUNSUPPORTED; return u->error; }
            switch(u->color_type){
                case 0: u->format=UPNG_GREY8;  bpp=1; break;
                case 2: u->format=UPNG_RGB8;   bpp=3; break;
                case 3: u->format=UPNG_INDEXED; bpp=1; break;
                case 4: u->format=UPNG_GREYA8; bpp=2; break;
                case 6: u->format=UPNG_RGBA8;  bpp=4; break;
                default: u->error=UPNG_EUNSUPPORTED; return u->error;
            }
        } else if(strcmp(type,"PLTE")==0) {
            u->has_palette=true;
            for(uint32_t i=0;i<len/3&&i<256;i++){
                u->palette[i][0]=p[i*3];
                u->palette[i][1]=p[i*3+1];
                u->palette[i][2]=p[i*3+2];
            }
        } else if(strcmp(type,"IDAT")==0) {
            if(u->idat_size+len > u->idat_cap) {
                size_t newcap=u->idat_cap+len+4096;
                uint8_t* nb=(uint8_t*)realloc(u->idat,newcap);
                if(!nb){ u->error=UPNG_ENOMEM; return u->error; }
                u->idat=nb; u->idat_cap=newcap;
            }
            memcpy(u->idat+u->idat_size,p,len);
            u->idat_size+=len;
        } else if(strcmp(type,"IEND")==0) {
            break;
        }
        p+=len; rem-=len;
        p+=4;   rem-=4; // skip CRC
    }

    if(!u->width||!u->height||!bpp||!u->idat_size){
        u->error=UPNG_EMALFORMED; return u->error;
    }

    // Inflate
    // Raw pixel rows: each row has 1 filter byte + width*bpp bytes
    size_t raw_size = (size_t)u->height * (1 + (size_t)u->width * bpp);
    uint8_t* raw=(uint8_t*)malloc(raw_size);
    if(!raw){ u->error=UPNG_ENOMEM; return u->error; }

    int got=tinfl_decompress(u->idat,(int)u->idat_size,raw,(int)raw_size);
    free(u->idat); u->idat=nullptr; u->idat_cap=0; u->idat_size=0;
    if(got<0||(size_t)got<raw_size){ free(raw); u->error=UPNG_EMALFORMED; return u->error; }

    // Reconstruct filtered rows
    // Output buffer: width * height * bpp (no filter bytes)
    size_t out_bpp_effective = (u->color_type==3) ? 3 : bpp; // palette → RGB
    u->size = (size_t)u->width * u->height * out_bpp_effective;
    u->buffer=(uint8_t*)malloc(u->size);
    if(!u->buffer){ free(raw); u->error=UPNG_ENOMEM; return u->error; }

    size_t stride = 1 + (size_t)u->width * bpp; // raw row stride (with filter byte)
    size_t out_stride = (size_t)u->width * out_bpp_effective;

    for(unsigned y=0;y<u->height;y++){
        uint8_t* recon      = raw    + y * stride;
        uint8_t* prev_recon = (y>0) ? (raw + (y-1)*stride) : nullptr;
        uint8_t  filter     = recon[0];
        uint8_t* row        = recon+1;
        uint8_t* prev_row   = prev_recon ? prev_recon+1 : nullptr;

        // Apply filter in-place
        switch(filter){
            case 0: break; // None
            case 1: // Sub
                for(size_t x=bpp;x<(size_t)u->width*bpp;x++)
                    row[x]=(uint8_t)(row[x]+row[x-bpp]);
                break;
            case 2: // Up
                if(prev_row)
                    for(size_t x=0;x<(size_t)u->width*bpp;x++)
                        row[x]=(uint8_t)(row[x]+prev_row[x]);
                break;
            case 3: // Average
                for(size_t x=0;x<(size_t)u->width*bpp;x++){
                    uint8_t a=(x>=(size_t)bpp)?row[x-bpp]:0;
                    uint8_t b=prev_row?prev_row[x]:0;
                    row[x]=(uint8_t)(row[x]+((a+b)>>1));
                }
                break;
            case 4: // Paeth
                for(size_t x=0;x<(size_t)u->width*bpp;x++){
                    uint8_t a=(x>=(size_t)bpp)?row[x-bpp]:0;
                    uint8_t b=prev_row?prev_row[x]:0;
                    uint8_t c=(prev_row&&x>=(size_t)bpp)?prev_row[x-bpp]:0;
                    row[x]=(uint8_t)(row[x]+paeth(a,b,c));
                }
                break;
            default: free(raw); u->error=UPNG_EMALFORMED; return u->error;
        }

        // Copy to output (expand palette if needed)
        uint8_t* dst = u->buffer + y * out_stride;
        if(u->color_type==3 && u->has_palette){
            for(unsigned x=0;x<u->width;x++){
                uint8_t idx=row[x];
                dst[x*3]   = u->palette[idx][0];
                dst[x*3+1] = u->palette[idx][1];
                dst[x*3+2] = u->palette[idx][2];
            }
        } else {
            memcpy(dst, row, u->width*bpp);
        }
    }

    free(raw);
    u->error=UPNG_EOK;
    return UPNG_EOK;
}

// ─────────────────────────────────────────────────────
// Streaming decode using ring-buffer inflate.
// Peak heap: IDAT (~8KB) + 2 row buffers (~3KB) = ~11KB.
// s_ring[32768] is static (BSS) – no heap for DEFLATE window.
// ─────────────────────────────────────────────────────
upng_error_t upng_decode_stream(upng_t* u, upng_row_cb cb, void* userdata) {
    if (!u || !cb) return UPNG_ENOTFOUND;

    const uint8_t* p = u->src; size_t rem = u->src_size;

    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    if (rem < 8 || memcmp(p, sig, 8) != 0) { u->error = UPNG_ENOTPNG; return u->error; }
    p += 8; rem -= 8;

    uint8_t color_type = 0, bit_depth = 0, bpp_raw = 0;
    unsigned width = 0, height = 0;
    uint8_t palette[256][3] = {};
    bool has_palette = false;

    uint8_t* idat    = nullptr;
    size_t   idat_sz = 0, idat_cap = 0;

    while (rem >= 12) {
        uint32_t len = png_read_u32(p); p += 4; rem -= 4;
        char type[5]; memcpy(type, p, 4); type[4] = 0; p += 4; rem -= 4;
        if (rem < len + 4) { free(idat); u->error = UPNG_EMALFORMED; return u->error; }

        if (strcmp(type, "IHDR") == 0) {
            width      = png_read_u32(p);
            height     = png_read_u32(p + 4);
            bit_depth  = p[8];
            color_type = p[9];
            if (bit_depth != 8 || p[10] != 0 || p[11] != 0 || p[12] != 0) {
                free(idat); u->error = UPNG_EUNSUPPORTED; return u->error;
            }
            switch (color_type) {
                case 0: bpp_raw = 1; break;
                case 2: bpp_raw = 3; break;
                case 3: bpp_raw = 1; break;
                case 4: bpp_raw = 2; break;
                case 6: bpp_raw = 4; break;
                default: free(idat); u->error = UPNG_EUNSUPPORTED; return u->error;
            }
        } else if (strcmp(type, "PLTE") == 0) {
            has_palette = true;
            for (uint32_t i = 0; i < len / 3 && i < 256; i++) {
                palette[i][0] = p[i*3];
                palette[i][1] = p[i*3+1];
                palette[i][2] = p[i*3+2];
            }
        } else if (strcmp(type, "IDAT") == 0) {
            if (idat_sz + len > idat_cap) {
                size_t newcap = idat_cap + len + 4096;
                uint8_t* nb = (uint8_t*)realloc(idat, newcap);
                if (!nb) { free(idat); u->error = UPNG_ENOMEM; return u->error; }
                idat = nb; idat_cap = newcap;
            }
            memcpy(idat + idat_sz, p, len);
            idat_sz += len;
        } else if (strcmp(type, "IEND") == 0) {
            break;
        }
        p += len; rem -= len;
        p += 4;   rem -= 4;
    }

    if (!width || !height || !bpp_raw || !idat_sz) {
        free(idat); u->error = UPNG_EMALFORMED; return u->error;
    }

    // Allocate only 2 row buffers + optional palette scratch.
    size_t stride  = 1 + (size_t)width * bpp_raw;
    uint8_t bpp_out = (color_type == 3) ? 3 : bpp_raw;

    uint8_t* cur     = (uint8_t*)malloc(stride);
    uint8_t* prev    = (uint8_t*)calloc(stride, 1);
    uint8_t* pal_row = (color_type == 3) ? (uint8_t*)malloc((size_t)width * 3) : nullptr;

    if (!cur || !prev || (color_type == 3 && !pal_row)) {
        free(idat); free(cur); free(prev); free(pal_row);
        u->error = UPNG_ENOMEM; return u->error;
    }

    // Set up ring context
    static ring_ctx_t ctx;
    ctx.cb          = cb;
    ctx.userdata    = userdata;
    ctx.width       = width;
    ctx.height      = height;
    ctx.bpp_raw     = bpp_raw;
    ctx.bpp_out     = bpp_out;
    ctx.color_type  = color_type;
    ctx.cur         = cur;
    ctx.prev        = prev;
    ctx.pal_row     = pal_row;
    ctx.has_palette = has_palette;
    ctx.stride      = stride;
    memcpy(ctx.palette, palette, sizeof(palette));

    int ret = tinfl_ring_decompress(idat, (int)idat_sz, &ctx);

    free(idat);
    free(cur);
    free(prev);
    free(pal_row);

    u->error = (ret == 0) ? UPNG_EOK : UPNG_EMALFORMED;
    return u->error;
}
