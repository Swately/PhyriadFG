#pragma once
// PhyriadFG instrument layer (STEP 4b of the layered separation — PURE RELOCATION from
// src/core/main.cpp; no logic change). The diagnostic frame-dump helpers: dump_bmp (32bpp
// top-down BMP, R<->B swapped for the .bmp BGRA storage) and dump_rgba (raw RGBA8, no header,
// no swap — the fg_quality_scorer's read_file contract). Both are written from the present /
// F threads in dump runs (pacing irrelevant); the bodies live in instrument.cpp.
#include <cstdint>

// STAGE-39b: diagnostic frame dump — 32bpp top-down BMP from an RGBA host buffer.
// frames/ is gitignored; written synchronously in P (pacing is irrelevant in dump runs).
void dump_bmp(const char* path,const uint8_t* rgba,uint32_t w,uint32_t h);
// §11.4 layer 1 (--qdump): raw RGBA8 dump — W*H*4 bytes, row-major, NO header (NO channel swap).
void dump_rgba(const char* path,const uint8_t* rgba,uint32_t w,uint32_t h);
