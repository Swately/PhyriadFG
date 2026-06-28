// PhyriadFG instrument layer. Bodies of the diagnostic frame-dump helpers declared in
// instrument/instrument.hpp.
#include "instrument/instrument.hpp"
#include <cstdio>     // std::fopen / std::fwrite / std::fclose
#include <cstring>    // std::memcpy (dump_bmp header build)
#include <vector>     // std::vector<uint8_t> row (dump_bmp)

// Diagnostic frame dump — 32bpp top-down BMP from an RGBA host buffer. Written synchronously in P
// (pacing is irrelevant in dump runs).
void dump_bmp(const char* path,const uint8_t* rgba,uint32_t w,uint32_t h){
    FILE* f=std::fopen(path,"wb"); if(!f) return;
    const uint32_t img=w*h*4u, fsz=54u+img, off=54u, ihsz=40u;
    uint8_t fh[14]={'B','M'}; std::memcpy(fh+2,&fsz,4); std::memcpy(fh+10,&off,4);
    uint8_t ih[40]={}; std::memcpy(ih,&ihsz,4);
    const int32_t iw=(int32_t)w, ihh=-(int32_t)h;   // negative height = top-down rows
    std::memcpy(ih+4,&iw,4); std::memcpy(ih+8,&ihh,4);
    const uint16_t planes=1,bpp=32; std::memcpy(ih+12,&planes,2); std::memcpy(ih+14,&bpp,2);
    std::memcpy(ih+20,&img,4);
    std::fwrite(fh,1,14,f); std::fwrite(ih,1,40,f);
    std::vector<uint8_t> row((size_t)w*4u);
    for(uint32_t y=0;y<h;++y){ const uint8_t* s=rgba+(size_t)y*w*4u;
        for(uint32_t x=0;x<w;++x){ row[x*4+0]=s[x*4+2]; row[x*4+1]=s[x*4+1]; row[x*4+2]=s[x*4+0]; row[x*4+3]=255; }
        std::fwrite(row.data(),1,row.size(),f); }
    std::fclose(f);
}
// --qdump: raw RGBA8 dump — W*H*4 bytes, row-major, NO header. The source host buffer holds the warp
// images' bytes in RGBA8 order (wapPrevA/wapCurA/wapOutA are VK_FORMAT_R8G8B8A8_UNORM), so this writes
// them verbatim — NO channel swap. This is the inverse of dump_bmp above, which swaps R↔B (row[0]=s[2])
// because the .bmp format stores BGRA; the fg_quality_scorer's read_file expects RGBA8, so the .rgba
// must NOT be swapped.
void dump_rgba(const char* path,const uint8_t* rgba,uint32_t w,uint32_t h){
    FILE* f=std::fopen(path,"wb"); if(!f) return;
    std::fwrite(rgba,1,(size_t)w*h*4u,f);
    std::fclose(f);
}
