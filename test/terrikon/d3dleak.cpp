// Creates and releases a D3D11 device again and again; prints private bytes
#include <windows.h>
#include <d3d11.h>
#include <psapi.h>
#include <cstdio>
static double mb() { PROCESS_MEMORY_COUNTERS_EX c{}; GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&c, sizeof c); return c.PrivateUsage / 1048576.0; }
static DWORD handles() { DWORD n = 0; GetProcessHandleCount(GetCurrentProcess(), &n); return n; }
int main(int argc, char **argv) {
    UINT flags = argc > 1 ? (UINT)atoi(argv[1]) : 0x800;
    for (int i = 0; i <= 20; i++) {
        ID3D11Device *d = nullptr; ID3D11DeviceContext *c = nullptr;
        D3D_FEATURE_LEVEL lvl;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0, D3D11_SDK_VERSION, &d, &lvl, &c);
        if (FAILED(hr)) { printf("fail %lx\n", hr); return 1; }
        c->Flush(); c->Release(); d->Release();
        if (i % 5 == 0) printf("round %2d: %.1f MB, %lu handles\n", i, mb(), handles());
    }
    return 0;
}
