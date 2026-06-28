// capture_dump — standalone Windows screen-capture tap for the FG-quality test-field.
//
// WHY: the FG-quality scorer needs RGBA8 `.rgba` frames + a manifest of a captured target. The
// canonical use is to capture the OUTPUT window of an external, black-box frame-generation tool
// (e.g. LSFG) so its presented frames — including its interpolated ones — can be scored without
// any access to its code or metrics. This is the digital capture tap that feeds the scorer.
//
// WHAT: capture N frames of a window (by title substring) or a monitor, write each as an
// RGBA8 `.rgba` file `<dir>/cap_%06d.rgba` + a `manifest.txt` in the fg_quality_scorer format
// (`size W H` line + one line per frame: capture index, a high-res QPC timestamp in ms, the
// filename). Capture is the source's PRESENT cadence — every presented frame, NO de-duplication
// (so an external tool's interpolated frames are kept too). Then exit cleanly.
//
// CAPTURE BACKENDS:
//   --api wgc  (default)  Windows.Graphics.Capture — handles flip-model / borderless / overlay
//                         present paths. MSVC + C++/WinRT only.
//   --api dd              DXGI Desktop Duplication — captures a whole monitor output.
//
// CAPTURABILITY: WGC/DXGI cannot capture some exclusive-fullscreen / protected (WDA_MONITOR)
// present paths — the frame comes back ALL-BLACK or the call fails. This tool DETECTS that: if the
// first ~10 captured frames are all near-zero variance it prints a LOUD warning and still writes
// what it got — it does NOT silently dump black frames as if they were valid.
//
// Standalone (own CMake project). Windows-only. D3D11 + DXGI + (MSVC) WGC/WinRT. NO Vulkan.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3d11_4.h>   // ID3D11Multithread — shared immediate-context between callback + main
#include <dxgi1_2.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

// ─── WGC / WinRT (MSVC + Windows SDK only) ───────────────────────────────────
// The include set plus an inline IDirect3DDxgiInterfaceAccess declaration: the interop header does
// NOT surface it under the cppwinrt include order on recent Windows SDKs, giving C2065 without this.
#ifdef _MSC_VER
#define WINRT_LEAN_AND_MEAN
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
namespace wgc  = winrt::Windows::Graphics::Capture;
namespace wgdx = winrt::Windows::Graphics::DirectX;
namespace wd3d = winrt::Windows::Graphics::DirectX::Direct3D11;
struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1"))
IDirect3DDxgiInterfaceAccess : ::IUnknown {
    virtual HRESULT __stdcall GetInterface(GUID const& id, void** object) = 0;
};
#endif // _MSC_VER

// ─── small helpers ───────────────────────────────────────────────────────────
template<class T> static void rel(T*& p){ if(p){ p->Release(); p=nullptr; } }

// High-resolution QPC clock in milliseconds (the manifest timestamp source). QPC, not
// steady_clock, because the scorer manifest spec asks for "a high-res QPC timestamp in ms".
static double g_qpc_freq = 0.0;
static void qpc_init(){ LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpc_freq=(double)f.QuadPart; }
static double qpc_now_ms(){ LARGE_INTEGER c; QueryPerformanceCounter(&c); return 1000.0*(double)c.QuadPart/g_qpc_freq; }

// ─── config ──────────────────────────────────────────────────────────────────
enum CaptureApi { CA_WGC, CA_DD };
struct Config {
    CaptureApi  api = CA_WGC;          // default wgc (flip-model/borderless present paths)
    char        window_substr[256]={}; // --window: capture window by title substring
    char        class_substr[256]={};  // --class: capture window by window-CLASS substring (for title-less surfaces, e.g. LSFG's output)
    int         monitor = 0;           // --monitor: DXGI output index (DD captures a display)
    int         frames  = 120;         // --frames N
    std::string out     = "capture";   // --out <dir>
    bool        api_explicit = false;  // did the user pass --api?
};

static void print_help(const char* a0){
    std::printf(
"capture_dump — standalone screen-capture tap → RGBA8 .rgba + manifest (PhyriadFG FG-quality test-field)\n\n"
"USAGE:\n"
"  %s --window \"<title substr>\" --frames N --out <dir> [--api wgc|dd]\n"
"  %s --monitor M --frames N --out <dir> [--api dd|wgc]\n\n"
"OPTIONS:\n"
"  --window SUBSTR   Capture the window whose title contains SUBSTR (implies --api wgc).\n"
"  --monitor M       Capture DXGI display output M (default 0; implies --api dd unless --api wgc).\n"
"  --frames N        Number of frames to capture (default 120).\n"
"  --out DIR         Output directory for cap_%%06d.rgba + manifest.txt (default 'capture').\n"
"  --api wgc|dd      Capture backend. wgc = Windows.Graphics.Capture (flip-model/borderless;\n"
"                    MSVC only). dd = DXGI Desktop Duplication (whole monitor). Default wgc.\n"
"  --help            This message.\n\n"
"OUTPUT (the fg_quality_scorer .rgba contract):\n"
"  <dir>/cap_%%06d.rgba   raw RGBA8, row-major, W*H*4 bytes, no header (B/R swapped on write).\n"
"  <dir>/manifest.txt     'size W H' + one line per frame: <index> <qpc_ms> <filename>.\n\n"
"CAPTURABILITY: if the first ~10 frames are all-black (near-zero variance) the target likely\n"
"  uses an uncapturable exclusive/protected present path — a LOUD warning is printed and the\n"
"  (black) frames are still written. Digital capture is not possible for such targets.\n",
        a0, a0);
}

static bool parse_args(int argc, char** argv, Config& c){
    for(int i=1;i<argc;++i){
        const char* a=argv[i];
        auto need=[&](const char* o)->const char*{ return (i+1<argc)?argv[++i]:(std::printf("[capture_dump] %s needs an argument\n",o),nullptr); };
        if      (!std::strcmp(a,"--help"))    { print_help(argv[0]); return false; }
        else if (!std::strcmp(a,"--window"))  { auto v=need(a); if(!v)return false; std::snprintf(c.window_substr,sizeof(c.window_substr),"%s",v); }
        else if (!std::strcmp(a,"--class"))   { auto v=need(a); if(!v)return false; std::snprintf(c.class_substr,sizeof(c.class_substr),"%s",v); }
        else if (!std::strcmp(a,"--monitor")) { auto v=need(a); if(!v)return false; c.monitor=std::atoi(v); }
        else if (!std::strcmp(a,"--frames"))  { auto v=need(a); if(!v)return false; c.frames=std::atoi(v); if(c.frames<1)c.frames=1; }
        else if (!std::strcmp(a,"--out"))     { auto v=need(a); if(!v)return false; c.out=v; }
        else if (!std::strcmp(a,"--api"))     { auto v=need(a); if(!v)return false;
            if      (!std::strcmp(v,"wgc")) { c.api=CA_WGC; c.api_explicit=true; }
            else if (!std::strcmp(v,"dd"))  { c.api=CA_DD;  c.api_explicit=true; }
            else { std::printf("[capture_dump] --api: unknown '%s' (wgc|dd)\n",v); return false; }
        }
        else { std::printf("[capture_dump] unknown arg '%s' (try --help)\n",a); return false; }
    }
    // --window implies WGC (a window can only be item-captured via WGC). Honour an explicit
    // --api dd with --window by erroring: DD captures an output, not a window.
    if(c.window_substr[0] || c.class_substr[0]){
        if(c.api_explicit && c.api==CA_DD){
            std::printf("[capture_dump] --window/--class requires --api wgc (DXGI DD captures a monitor, not a window)\n");
            return false;
        }
        c.api = CA_WGC;
    }
    return true;
}

// ─── D3D11 / DXGI device + output enumeration ────────────────────────────────
struct OutInfo { char name[40]={}; RECT coords{}; bool attached=false; HMONITOR hmon=nullptr; int hz=0; };
struct D3D {
    ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr;
    IDXGIOutputDuplication* dup=nullptr;
    char adapter[128]={}; uint32_t w=0,h=0; DXGI_FORMAT fmt=DXGI_FORMAT_UNKNOWN;
    HMONITOR cap_hmon=nullptr;
    std::vector<OutInfo> outputs;
};

// want_dup=false (WGC path): create the D3D11 device only, take dims from the output rect / window
// client rect. want_dup=true (DD path): also DuplicateOutput on output `ci`.
static bool d3d_init(D3D& d,int ci,bool want_dup){
    D3D_FEATURE_LEVEL fl;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&d.dev,&fl,&d.ctx))) return false;
    // Enable D3D11 multithread protection so the WGC FrameArrived callback (CopyResource) and the
    // main loop (Map/Unmap) can share the immediate context safely.
#ifdef _MSC_VER
    { ID3D11Multithread* mt=nullptr;
      if(SUCCEEDED(d.ctx->QueryInterface(__uuidof(ID3D11Multithread),(void**)&mt))&&mt){ mt->SetMultithreadProtected(TRUE); mt->Release(); } }
#endif
    IDXGIDevice* dxgi=nullptr; d.dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);
    IDXGIAdapter* ad=nullptr; dxgi->GetAdapter(&ad); DXGI_ADAPTER_DESC adesc{}; ad->GetDesc(&adesc);
    WideCharToMultiByte(CP_ACP,0,adesc.Description,-1,d.adapter,sizeof(d.adapter),nullptr,nullptr);
    for(UINT i=0;;++i){ IDXGIOutput* o=nullptr; if(ad->EnumOutputs(i,&o)!=S_OK)break; DXGI_OUTPUT_DESC od{}; o->GetDesc(&od);
        OutInfo info; WideCharToMultiByte(CP_ACP,0,od.DeviceName,-1,info.name,sizeof(info.name),nullptr,nullptr);
        info.coords=od.DesktopCoordinates; info.attached=(od.AttachedToDesktop!=0); info.hmon=od.Monitor;
        { DEVMODEA dm{}; dm.dmSize=sizeof(dm); if(EnumDisplaySettingsExA(info.name,ENUM_CURRENT_SETTINGS,&dm,0)) info.hz=(int)dm.dmDisplayFrequency; }
        d.outputs.push_back(info);
        if((int)i==ci){
            d.cap_hmon=od.Monitor;
            if(want_dup){
                IDXGIOutput1* o1=nullptr; o->QueryInterface(__uuidof(IDXGIOutput1),(void**)&o1);
                if(o1&&o1->DuplicateOutput(d.dev,&d.dup)==S_OK){ DXGI_OUTDUPL_DESC dd{}; d.dup->GetDesc(&dd); d.w=dd.ModeDesc.Width; d.h=dd.ModeDesc.Height; d.fmt=dd.ModeDesc.Format; }
                rel(o1);
            } else {
                // WGC path: dims from the output rect; WGC always delivers BGRA8.
                d.w=(uint32_t)(od.DesktopCoordinates.right-od.DesktopCoordinates.left);
                d.h=(uint32_t)(od.DesktopCoordinates.bottom-od.DesktopCoordinates.top);
                d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM;
            }
        }
        rel(o); }
    rel(ad); rel(dxgi);
    return want_dup ? (d.dup!=nullptr) : (d.dev!=nullptr);
}
static void d3d_shutdown(D3D& d){ rel(d.dup); rel(d.ctx); rel(d.dev); }

// A CPU-readable staging texture of the capture format (D3D11_USAGE_STAGING + CPU_READ).
static ID3D11Texture2D* d3d_staging(D3D& d,uint32_t w,uint32_t h){
    D3D11_TEXTURE2D_DESC td{}; td.Width=w; td.Height=h; td.MipLevels=1; td.ArraySize=1; td.Format=d.fmt;
    td.SampleDesc.Count=1; td.Usage=D3D11_USAGE_STAGING; td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* t=nullptr; d.dev->CreateTexture2D(&td,nullptr,&t); return t;
}

#ifdef _MSC_VER
// Find the first visible window whose title contains substr.
struct WndFind { const char* substr; bool byClass; HWND found; };
static BOOL CALLBACK enum_wnd_cb(HWND h,LPARAM lp){
    WndFind* f=reinterpret_cast<WndFind*>(lp);
    if(!IsWindowVisible(h)) return TRUE;
    char buf[256]={};
    if(f->byClass) GetClassNameA(h,buf,sizeof(buf));
    else           GetWindowTextA(h,buf,sizeof(buf));
    if(buf[0]&&std::strstr(buf,f->substr)){ f->found=h; return FALSE; }
    return TRUE;
}
static HWND find_window_by_substr(const char* substr){
    WndFind f{substr,false,nullptr}; EnumWindows(enum_wnd_cb,reinterpret_cast<LPARAM>(&f)); return f.found;
}
static HWND find_window_by_class(const char* substr){
    WndFind f{substr,true,nullptr}; EnumWindows(enum_wnd_cb,reinterpret_cast<LPARAM>(&f)); return f.found;
}
#endif // _MSC_VER

// ─── BGRA8 → RGBA8 .rgba writer ──────────────────────────────────────────────
// Mapped BGRA8 rows (capture is DXGI_FORMAT_B8G8R8A8_UNORM) → RGBA8 on disk (the scorer's loader
// expects RGBA8, row-major, W*H*4, no header). Swap B/R per pixel, force A=255. Handles
// RowPitch != W*4 (DXGI staging rows are pitch-aligned). Returns false on file error.
//
// While converting, accumulate a cheap luma sum/sumsq over a subsampled grid so the caller can
// flag all-black/near-zero-variance frames (the capturability test) without a second pass.
static bool write_rgba_from_bgra(const char* path,const uint8_t* src,size_t row_pitch,
                                 uint32_t w,uint32_t h,double& out_mean,double& out_var){
    FILE* f=std::fopen(path,"wb"); if(!f){ out_mean=out_var=0.0; return false; }
    std::vector<uint8_t> row((size_t)w*4u);
    // Subsampled luma stats: every 16th pixel of every 8th row — enough to detect a black frame,
    // negligible cost. Uses the green channel as a luma proxy (cheap, B/R-swap-invariant).
    double sum=0.0, sumsq=0.0; uint64_t cnt=0;
    for(uint32_t y=0;y<h;++y){
        const uint8_t* s=src+(size_t)y*row_pitch;
        for(uint32_t x=0;x<w;++x){
            const uint8_t b=s[x*4+0], g=s[x*4+1], r=s[x*4+2];
            row[x*4+0]=r; row[x*4+1]=g; row[x*4+2]=b; row[x*4+3]=255;  // BGRA → RGBA, opaque
        }
        if((y&7u)==0){ for(uint32_t x=0;x<w;x+=16){ const double v=(double)s[x*4+1]; sum+=v; sumsq+=v*v; ++cnt; } }
        if(std::fwrite(row.data(),1,row.size(),f)!=row.size()){ std::fclose(f); out_mean=out_var=0.0; return false; }
    }
    std::fclose(f);
    if(cnt){ out_mean=sum/(double)cnt; out_var=sumsq/(double)cnt - out_mean*out_mean; }
    else   { out_mean=out_var=0.0; }
    return true;
}

// One captured frame's bookkeeping for the manifest + the summary stats.
struct CapRec { double t_ms; double mean; double var; bool ok; };

// All-black detection threshold. A frame is "near-zero variance / all-black" when its subsampled
// luma mean AND variance are both below these. Chosen conservatively: a genuine (even very dark)
// game frame has SOME spatial variation → variance well above kBlackVar; a failed protected/
// exclusive capture returns a uniform 0 (mean≈0, var≈0) plane. mean guards a uniform non-zero
// plane (rare, but e.g. a solid clear). Both must hold to flag — avoids false-positiving a dark scene.
static constexpr double kBlackMean = 2.0;    // luma 0..255
static constexpr double kBlackVar  = 1.0;    // luma variance 0..(255^2)
static constexpr int    kBlackProbeN = 10;   // inspect the first N frames for the capturability test

int main(int argc,char** argv){
    qpc_init();
    Config cfg; if(!parse_args(argc,argv,cfg)) return 0;

#ifndef _MSC_VER
    if(cfg.api==CA_WGC){
        std::printf("[capture_dump] --api wgc requires the MSVC + C++/WinRT build; this binary was built\n"
                    "               without WGC. Use --api dd --monitor M, or build with MSVC.\n");
        return 1;
    }
#endif

    // ── D3D11 + capture target ────────────────────────────────────────────────
    const bool want_dd = (cfg.api==CA_DD);
    D3D d{};
    if(!d3d_init(d,cfg.monitor,want_dd)){
        std::printf("[capture_dump] capture init failed (api=%s, monitor=%d)\n", want_dd?"dd":"wgc", cfg.monitor);
        d3d_shutdown(d); return 1;
    }

#ifdef _MSC_VER
    HWND target_hwnd=nullptr;
    if(cfg.api==CA_WGC && (cfg.window_substr[0] || cfg.class_substr[0])){
        target_hwnd = cfg.class_substr[0] ? find_window_by_class(cfg.class_substr)
                                          : find_window_by_substr(cfg.window_substr);
        if(!target_hwnd){ std::printf("[capture_dump] no visible window matching %s '%s'\n",
                                      cfg.class_substr[0]?"class":"title",
                                      cfg.class_substr[0]?cfg.class_substr:cfg.window_substr); d3d_shutdown(d); return 1; }
        RECT cr{}; GetClientRect(target_hwnd,&cr);
        if(cr.right>cr.left && cr.bottom>cr.top){ d.w=(uint32_t)(cr.right-cr.left); d.h=(uint32_t)(cr.bottom-cr.top); }
        d.fmt=DXGI_FORMAT_B8G8R8A8_UNORM;   // WGC always delivers BGRA8
    }
#endif

    if(d.w==0||d.h==0){ std::printf("[capture_dump] target has zero size (w=%u h=%u)\n",d.w,d.h); d3d_shutdown(d); return 1; }

    // Only BGRA8 / RGBA8 (8-bit SDR) targets are written as straight RGBA8. The scorer expects
    // RGBA8; HDR (FP16) / 10-bit outputs would need tone-mapping the tool deliberately does not do.
    const bool fmt_ok = (d.fmt==DXGI_FORMAT_B8G8R8A8_UNORM || d.fmt==DXGI_FORMAT_R8G8B8A8_UNORM);
    if(!fmt_ok){
        std::printf("[capture_dump] WARNING: capture format DXGI=%d is not 8-bit BGRA/RGBA; the .rgba\n"
                    "              output would be wrong (HDR/10-bit needs tone-map — not supported).\n",(int)d.fmt);
        d3d_shutdown(d); return 1;
    }
    const bool src_is_bgra = (d.fmt==DXGI_FORMAT_B8G8R8A8_UNORM);

    const uint32_t W=d.w, H=d.h;
    const uint32_t bpp=4;
    const size_t   tight_row=(size_t)W*bpp;

    // ── prepare output dir + manifest ─────────────────────────────────────────
    CreateDirectoryA(cfg.out.c_str(),nullptr);   // ok if it already exists
    std::string manifest_path = cfg.out + "\\manifest.txt";
    FILE* mf=std::fopen(manifest_path.c_str(),"wb");
    if(!mf){ std::printf("[capture_dump] cannot open manifest '%s' for writing\n",manifest_path.c_str()); d3d_shutdown(d); return 1; }
    std::fprintf(mf,"# capture_dump — PhyriadFG FG-quality test-field tap\n");
    std::fprintf(mf,"# api=%s target=%s adapter=%s\n", want_dd?"dd":"wgc",
        cfg.window_substr[0]?cfg.window_substr:"<monitor>", d.adapter);
    std::fprintf(mf,"size %u %u\n", W, H);

    // Target/banner.
    if(cfg.window_substr[0]) std::printf("[capture_dump] target window: '%s'  hwnd=%p\n",cfg.window_substr,(void*)
#ifdef _MSC_VER
        target_hwnd
#else
        nullptr
#endif
    );
    else                     std::printf("[capture_dump] target monitor: %d\n",cfg.monitor);
    std::printf("[capture_dump] api=%s  resolution=%ux%u  format=%s  adapter=%s\n",
        want_dd?"dd":"wgc", W, H, src_is_bgra?"BGRA8":"RGBA8", d.adapter);
    std::printf("[capture_dump] capturing %d frames → %s\\cap_%%06d.rgba (+ manifest.txt)\n", cfg.frames, cfg.out.c_str());

    std::vector<CapRec> recs; recs.reserve((size_t)cfg.frames);

    // A small lambda: convert one mapped BGRA/RGBA plane → RGBA8 file + record stats.
    // The B/R swap is a no-op when the source is already RGBA8 (DD on an R8G8B8A8 output);
    // we still route through write_rgba_from_bgra but the swap would mis-order — so for the
    // RGBA8 source case we write straight-through. Handled by `src_is_bgra`.
    auto write_one=[&](const uint8_t* mapped,size_t row_pitch,uint32_t idx,double t_ms)->void{
        char fname[64]; std::snprintf(fname,sizeof(fname),"cap_%06u.rgba",idx);
        std::string fpath = cfg.out + "\\" + fname;
        double mean=0.0,var=0.0; bool ok=false;
        if(src_is_bgra){
            ok=write_rgba_from_bgra(fpath.c_str(),mapped,row_pitch,W,H,mean,var);
        } else {
            // Source already RGBA8 — straight copy row-by-row (still honour row_pitch), stats on G.
            FILE* f=std::fopen(fpath.c_str(),"wb");
            if(f){ double sum=0.0,sumsq=0.0; uint64_t cnt=0;
                for(uint32_t y=0;y<H;++y){ const uint8_t* s=mapped+(size_t)y*row_pitch;
                    std::fwrite(s,1,tight_row,f);
                    if((y&7u)==0) for(uint32_t x=0;x<W;x+=16){ const double v=(double)s[x*4+1]; sum+=v; sumsq+=v*v; ++cnt; } }
                std::fclose(f);
                if(cnt){ mean=sum/(double)cnt; var=sumsq/(double)cnt-mean*mean; }
                ok=true;
            }
        }
        std::fprintf(mf,"%u %.6f %s\n", idx, t_ms, fname);
        recs.push_back(CapRec{t_ms,mean,var,ok});
    };

    int captured=0;

    if(cfg.api==CA_DD){
        // ── DXGI Desktop Duplication path ──────────────────────────────────────
        ID3D11Texture2D* stage=d3d_staging(d,W,H);
        if(!stage){ std::printf("[capture_dump] staging texture alloc failed\n"); std::fclose(mf); d3d_shutdown(d); return 1; }
        int empty_streak=0;
        while(captured<cfg.frames){
            DXGI_OUTDUPL_FRAME_INFO fi{}; IDXGIResource* res=nullptr;
            HRESULT hr=d.dup->AcquireNextFrame(1000,&fi,&res);
            if(FAILED(hr)){
                if(hr==DXGI_ERROR_WAIT_TIMEOUT){ if(++empty_streak>5){ std::printf("[capture_dump] DD: no new frames (output idle) — stopping early at %d\n",captured); break; } continue; }
                std::printf("[capture_dump] DD: AcquireNextFrame failed hr=0x%08lX — stopping\n",(unsigned long)hr); break;
            }
            empty_streak=0;
            ID3D11Texture2D* cap=nullptr; res->QueryInterface(__uuidof(ID3D11Texture2D),(void**)&cap); rel(res);
            if(!cap){ d.dup->ReleaseFrame(); continue; }
            d.ctx->CopyResource(stage,cap); d.ctx->Flush(); rel(cap);
            D3D11_MAPPED_SUBRESOURCE mr{};
            if(d.ctx->Map(stage,0,D3D11_MAP_READ,0,&mr)==S_OK){
                const double t_ms=qpc_now_ms();
                write_one((const uint8_t*)mr.pData, mr.RowPitch, (uint32_t)captured, t_ms);
                d.ctx->Unmap(stage,0);
                ++captured;
                if(captured%30==0) std::printf("[capture_dump] captured %d/%d frames\n",captured,cfg.frames);
            }
            d.dup->ReleaseFrame();
        }
        rel(stage);
    }
#ifdef _MSC_VER
    else { // CA_WGC
        // ── Windows.Graphics.Capture path ─────────────────────────────────────
        winrt::init_apartment();   // WinRT MTA — required before any WinRT call; matches CreateFreeThreaded

        winrt::com_ptr<IDXGIDevice> dxgi_dev;
        d.dev->QueryInterface(dxgi_dev.put());
        winrt::com_ptr<IInspectable> d3d_insp;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi_dev.get(),d3d_insp.put()));
        auto d3d_winrt=d3d_insp.as<wd3d::IDirect3DDevice>();

        auto interop=winrt::get_activation_factory<wgc::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
        wgc::GraphicsCaptureItem item{nullptr};
        if(cfg.window_substr[0]){
            winrt::check_hresult(interop->CreateForWindow(target_hwnd,
                winrt::guid_of<wgc::GraphicsCaptureItem>(),winrt::put_abi(item)));
        } else {
            winrt::check_hresult(interop->CreateForMonitor(d.cap_hmon,
                winrt::guid_of<wgc::GraphicsCaptureItem>(),winrt::put_abi(item)));
        }

        // Free-threaded staging ring (the callback CopyResource's into it; main maps it). 4 deep —
        // absorbs callback/main scheduling jitter without WGC-level drops at the source cadence.
        enum : uint32_t { RING_N=4u };
        struct WgcCtx {
            wgc::Direct3D11CaptureFramePool pool{nullptr};
            wgc::GraphicsCaptureSession     session{nullptr};
            ID3D11DeviceContext*            ctx=nullptr;
            ID3D11Texture2D*                ring[RING_N]{};
            std::atomic<uint32_t>           ring_write{0};
            std::atomic<uint32_t>           ring_read{0};
            std::atomic<bool>               running{true};
        } wctx;
        wctx.ctx=d.ctx;
        for(uint32_t i=0;i<RING_N;++i){ wctx.ring[i]=d3d_staging(d,W,H); if(!wctx.ring[i]){ std::printf("[capture_dump] WGC staging alloc failed\n"); std::fclose(mf); d3d_shutdown(d); return 1; } }

        winrt::Windows::Graphics::SizeInt32 pool_sz{(int32_t)W,(int32_t)H};
        wctx.pool=wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(
            d3d_winrt, wgdx::DirectXPixelFormat::B8G8R8A8UIntNormalized, RING_N+2, pool_sz);

        // FrameArrived: CopyResource the new BGRA8 surface to the next ring slot. NO de-dup —
        // every presented frame (incl. an external tool's interpolated ones) is forwarded.
        WgcCtx* raw=&wctx;
        wctx.pool.FrameArrived([raw](auto& p,auto&){
            if(!raw->running.load()) return;
            auto frame=p.TryGetNextFrame(); if(!frame) return;
            auto surface=frame.Surface();
            auto acc=surface.try_as<IDirect3DDxgiInterfaceAccess>(); if(!acc) return;
            winrt::com_ptr<ID3D11Texture2D> tex;
            acc->GetInterface(IID_PPV_ARGS(tex.put())); if(!tex) return;
            const uint32_t w=raw->ring_write.load();
            const uint32_t r=raw->ring_read.load();
            if(w-r>=RING_N) return;                 // ring full — main is behind; skip this slot
            raw->ctx->CopyResource(raw->ring[w%RING_N],tex.get());
            ++raw->ring_write;
        });

        wctx.session=wctx.pool.CreateCaptureSession(item);
        try { wctx.session.IsBorderRequired(false); } catch(...) {}        // Win11 22621+
        try { wctx.session.IsCursorCaptureEnabled(false); } catch(...) {}  // don't bake the cursor in
        // NOTE: we deliberately do NOT set MinUpdateInterval — the test-field WANTS every presented
        // frame at the source's full present cadence (240Hz → ~240 distinct frames/s, incl.
        // interpolated). The default (no throttle) delivers the source's present rate.
        wctx.session.StartCapture();

        // ── drain the ring → write ────────────────────────────────────────────
        const double t_start=qpc_now_ms();
        const double t_deadline=t_start + 20000.0 + (double)cfg.frames*50.0; // generous safety stop
        while(captured<cfg.frames){
            const uint32_t w=wctx.ring_write.load();
            const uint32_t r=wctx.ring_read.load();
            if(w==r){
                if(qpc_now_ms()>t_deadline){ std::printf("[capture_dump] WGC: timed out waiting for frames (got %d/%d) — target may be idle or uncapturable\n",captured,cfg.frames); break; }
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue;
            }
            // Consume the OLDEST unread slot (in present order) so timestamps stay monotone.
            const uint32_t slot=r%RING_N;
            D3D11_MAPPED_SUBRESOURCE mr{};
            HRESULT mhr=d.ctx->Map(wctx.ring[slot],0,D3D11_MAP_READ,0,&mr);  // blocking Map — the copy is done
            if(mhr!=S_OK){ wctx.ring_read.store(r+1); continue; }
            const double t_ms=qpc_now_ms();
            write_one((const uint8_t*)mr.pData, mr.RowPitch, (uint32_t)captured, t_ms);
            d.ctx->Unmap(wctx.ring[slot],0);
            wctx.ring_read.store(r+1);
            ++captured;
            if(captured%30==0) std::printf("[capture_dump] captured %d/%d frames\n",captured,cfg.frames);
        }

        // Stop the session BEFORE the ring textures are freed (the callback touches them).
        wctx.running.store(false);
        try { wctx.session.Close(); } catch(...) {}
        try { wctx.pool.Close(); } catch(...) {}
        for(uint32_t i=0;i<RING_N;++i) rel(wctx.ring[i]);
    }
#endif // _MSC_VER

    std::fclose(mf);

    // ── capturability test + summary ──────────────────────────────────────────
    // All-black detection over the first kBlackProbeN frames (the first-run capturability test):
    // a protected/exclusive present path returns uniform-black or fails → mean≈0 AND var≈0.
    int probe = (captured<kBlackProbeN)? captured : kBlackProbeN;
    int black=0;
    for(int i=0;i<probe;++i) if(recs[(size_t)i].mean<kBlackMean && recs[(size_t)i].var<kBlackVar) ++black;
    const bool all_black = (probe>0 && black==probe);

    // Mean inter-frame ms from the QPC timestamps = the observed capture cadence.
    double mean_dt=0.0; int n_dt=0;
    for(size_t i=1;i<recs.size();++i){ const double dt=recs[i].t_ms-recs[i-1].t_ms; if(dt>0.0){ mean_dt+=dt; ++n_dt; } }
    if(n_dt) mean_dt/=(double)n_dt;

    std::printf("\n[capture_dump] ── summary ─────────────────────────────────────────\n");
    std::printf("[capture_dump] frames written : %d (requested %d)\n", captured, cfg.frames);
    std::printf("[capture_dump] resolution     : %ux%u  api=%s\n", W, H, want_dd?"dd":"wgc");
    if(n_dt) std::printf("[capture_dump] mean inter-frame: %.3f ms  (~%.1f fps observed capture cadence)\n", mean_dt, mean_dt>0.0?1000.0/mean_dt:0.0);
    else     std::printf("[capture_dump] mean inter-frame: n/a (need >=2 frames)\n");
    std::printf("[capture_dump] output         : %s\\cap_000000.rgba ... + manifest.txt\n", cfg.out.c_str());

    if(all_black){
        std::printf("\n");
        std::printf("[capture_dump] ****************************************************************\n");
        std::printf("[capture_dump] WARNING: frames are all-black — target may use an uncapturable\n");
        std::printf("[capture_dump]          exclusive/protected present path; digital capture not\n");
        std::printf("[capture_dump]          possible for this target (camera fallback needed)\n");
        std::printf("[capture_dump] ****************************************************************\n");
        std::printf("[capture_dump] (the %d frames WERE written — they are black; do NOT score them as valid)\n", captured);
    }

    d3d_shutdown(d);
    // Exit code: 0 normal, 2 = captured but all-black (lets a script detect the uncapturable case),
    // 3 = nothing captured.
    if(captured==0) return 3;
    return all_black ? 2 : 0;
}
// Made with my soul - Swately <3
