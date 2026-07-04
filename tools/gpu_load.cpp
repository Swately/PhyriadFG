// gpu_load.cpp — synthetic GPU saturation bench (0.4.0 arc, operator-requested 2026-07-03).
// Loads a CHOSEN adapter to a TARGET duty % with a closed-loop compute spinner, so the FG's
// collapse-under-saturation curve (freshage/lat/multiplier vs load) is reproducible without
// burning game sessions. Same tool class as ball_zoo: a bench witness, not product code.
//
//   gpu_load.exe --list                 enumerate adapters (DXGI order)
//   gpu_load.exe --gpu 0 --load 99      pin adapter 0 at ~99% duty until Ctrl+C
//   gpu_load.exe --gpu 0 --load 60 --heavy 4   heavier per-dispatch work (jitter coarseness knob)
//
// Duty control: each cycle submits a batch sized ~kSliceMs of GPU work, waits for completion
// (event query), then sleeps busy*(100-load)/load — a duty-cycle governor that converges on the
// target regardless of the GPU's speed. --heavy scales the per-dispatch cost: coarser slices
// model games that submit LONG command buffers (the preemption-granularity worst case).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")

static const char* kCS = R"(
RWStructuredBuffer<float> buf : register(u0);
cbuffer C : register(b0) { uint iters; uint pad0; uint pad1; uint pad2; }
[numthreads(256,1,1)]
void main(uint3 id : SV_DispatchThreadID){
    float v = buf[id.x];
    [loop] for(uint i=0;i<iters;++i){ v = v*1.000001f + 0.000001f; v = v - (v>1e6f ? v : 0.0f); }
    buf[id.x] = v;
}
)";

static double now_ms(){ static LARGE_INTEGER f={}; if(!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart; }

int main(int argc, char** argv){
    int gpu=-1, load=99, heavy=1; bool list=false;
    for(int i=1;i<argc;++i){
        if(!strcmp(argv[i],"--list")) list=true;
        else if(!strcmp(argv[i],"--gpu")   && i+1<argc) gpu=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--load")  && i+1<argc) load=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--heavy") && i+1<argc) heavy=atoi(argv[++i]);
    }
    if(load<1) load=1; if(load>100) load=100; if(heavy<1) heavy=1; if(heavy>64) heavy=64;

    IDXGIFactory1* fac=nullptr;
    if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&fac))){ printf("[gpu-load] DXGI factory failed\n"); return 1; }
    IDXGIAdapter1* ad=nullptr; IDXGIAdapter1* pick=nullptr;
    for(UINT i=0; fac->EnumAdapters1(i,&ad)==S_OK; ++i){
        DXGI_ADAPTER_DESC1 d; ad->GetDesc1(&d);
        if(list) wprintf(L"[gpu-load] adapter %u: %s%s\n", i, d.Description,
                         (d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)?L" (software)":L"");
        if((int)i==gpu) pick=ad; else ad->Release();
    }
    if(list){ if(pick) pick->Release(); fac->Release(); return 0; }
    if(!pick){ printf("[gpu-load] --gpu N required (see --list)\n"); fac->Release(); return 1; }

    ID3D11Device* dev=nullptr; ID3D11DeviceContext* ctx=nullptr;
    D3D_FEATURE_LEVEL fl;
    if(FAILED(D3D11CreateDevice(pick,D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,
                                D3D11_SDK_VERSION,&dev,&fl,&ctx))){ printf("[gpu-load] device failed\n"); return 1; }
    DXGI_ADAPTER_DESC1 d; pick->GetDesc1(&d);
    wprintf(L"[gpu-load] ARMED on %s — target %d%% duty, heavy %d\n", d.Description, load, heavy);

    ID3DBlob* blob=nullptr; ID3DBlob* err=nullptr;
    if(FAILED(D3DCompile(kCS,strlen(kCS),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&blob,&err))){
        printf("[gpu-load] shader: %s\n", err?(char*)err->GetBufferPointer():"?"); return 1; }
    ID3D11ComputeShader* cs=nullptr; dev->CreateComputeShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&cs);

    const UINT kElems=256*1024;
    D3D11_BUFFER_DESC bd{}; bd.ByteWidth=kElems*4; bd.Usage=D3D11_USAGE_DEFAULT;
    bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS; bd.StructureByteStride=4;
    bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    ID3D11Buffer* buf=nullptr; dev->CreateBuffer(&bd,nullptr,&buf);
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud{}; ud.Format=DXGI_FORMAT_UNKNOWN;
    ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER; ud.Buffer.NumElements=kElems;
    ID3D11UnorderedAccessView* uav=nullptr; dev->CreateUnorderedAccessView(buf,&ud,&uav);
    // Per-thread work must DOMINATE the WDDM submit/sync round-trip (~1-3ms class), or the duty
    // loop saturates the round-trip pipeline while the SMs idle (measured: self-duty 99% with
    // nvidia-smi at 8% when iters was 2000). 50K iters ≈ ms-class real execution per dispatch.
    struct { UINT iters,p0,p1,p2; } cb{ 50000u*(UINT)heavy,0,0,0 };
    D3D11_BUFFER_DESC cbd{}; cbd.ByteWidth=16; cbd.Usage=D3D11_USAGE_DEFAULT; cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cbi{&cb,0,0};
    ID3D11Buffer* cbuf=nullptr; dev->CreateBuffer(&cbd,&cbi,&cbuf);
    // In-flight ring: the queue must NEVER drain (a drain-per-cycle loop measures the ~13ms WDDM
    // round trip while the SMs idle — measured: self-duty 99% with nvidia-smi at 8-14%). Up to
    // kInflight batches ride the queue; pacing waits only on the OLDEST completion.
    const int kInflight=3;
    D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT,0};
    ID3D11Query* q[kInflight]={};
    double t_submit[kInflight]={};
    for(int i=0;i<kInflight;++i) dev->CreateQuery(&qd,&q[i]);

    ctx->CSSetShader(cs,nullptr,0); ctx->CSSetUnorderedAccessViews(0,1,&uav,nullptr);
    ctx->CSSetConstantBuffers(0,1,&cbuf);
    timeBeginPeriod(1);

    UINT64 head=0, tail=0;   // submitted / completed batch counters (ring index = n % kInflight)
    UINT64 done=0; double t_stat=now_ms(); double gpu_ms_acc=0; double last_done_t=now_ms();
    for(;;){
        while(head-tail<(UINT64)kInflight){
            const int slot=(int)(head%(UINT64)kInflight);
            ctx->Dispatch(kElems/256,1,1);
            ctx->End(q[slot]); ctx->Flush();
            t_submit[slot]=now_ms(); ++head;
        }
        const int oldest=(int)(tail%(UINT64)kInflight);
        while(ctx->GetData(q[oldest],nullptr,0,0)==S_FALSE) SwitchToThread();
        const double t_done=now_ms();
        const double gpu_ms=t_done-last_done_t;   // inter-completion gap ≈ per-batch GPU occupancy when fed
        last_done_t=t_done; ++tail; ++done; gpu_ms_acc+=gpu_ms;
        // Duty pacing: sleep between completions so busy/(busy+sleep) → load%.
        const double sleep_ms = gpu_ms*(100.0-load)/(double)load;
        if(sleep_ms>=1.0){ Sleep((DWORD)sleep_ms); last_done_t+=sleep_ms; }
        const double wall=now_ms()-t_stat;
        if(wall>=1000.0){
            printf("[gpu-load] batches/s=%llu batch_ms=%.2f (target %d%%)\n",
                   (unsigned long long)done, done? gpu_ms_acc/(double)done : 0.0, load);
            fflush(stdout); done=0; gpu_ms_acc=0; t_stat=now_ms();
        }
    }
}
