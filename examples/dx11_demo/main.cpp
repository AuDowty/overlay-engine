#include <overlay/overlay.hpp>
#include <imgui.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cstdio>
#include <cstring>

namespace {

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapchain = nullptr;
ID3D11RenderTargetView* g_backbuffer_rtv = nullptr;
HWND g_hwnd = nullptr;

LRESULT CALLBACK demo_wndproc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, w, l);
}

bool init_window() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = demo_wndproc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"overlay-demo";
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(0, L"overlay-demo", L"overlay-engine dx11 demo",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             960, 540, nullptr, nullptr, wc.hInstance, nullptr);
    return g_hwnd != nullptr;
}

bool init_d3d11() {
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = 0;
    scd.BufferDesc.Height = 0;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = g_hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &g_swapchain, &g_device, &fl, &g_context);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* bb = nullptr;
    g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&bb));
    g_device->CreateRenderTargetView(bb, nullptr, &g_backbuffer_rtv);
    bb->Release();
    return true;
}

void shutdown() {
    if (g_backbuffer_rtv) g_backbuffer_rtv->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    if (g_swapchain) g_swapchain->Release();
    if (g_hwnd) DestroyWindow(g_hwnd);
}

void render_fps_overlay() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("fps", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("overlay-engine demo");
    ImGui::Separator();
    ImGui::Text("FPS:  %.1f", io.Framerate);
    ImGui::Text("ms:   %.3f", 1000.0f / io.Framerate);
    ImGui::Text("frame: %lu", overlay::frame_count());
    ImGui::End();
}

}

int main(int argc, char** argv) {
    bool headless = false;
    int timeout_seconds = 0;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--smoke") == 0) {
            headless = true;
            timeout_seconds = 2;
        }
    }

    if (!init_window()) {
        std::fprintf(stderr, "init_window failed\n");
        return 1;
    }
    ShowWindow(g_hwnd, headless ? SW_HIDE : SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    if (!init_d3d11()) {
        std::fprintf(stderr, "init_d3d11 failed\n");
        return 2;
    }

    auto status = overlay::attach(render_fps_overlay);
    if (status != overlay::Status::Ok) {
        std::fprintf(stderr, "overlay::attach failed: %d\n", static_cast<int>(status));
        return 3;
    }
    std::fprintf(stderr, "overlay attached, entering render loop\n");
    std::fflush(stderr);

    DWORD start = GetTickCount();
    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        const float clear[4] = {0.08f, 0.10f, 0.14f, 1.0f};
        g_context->OMSetRenderTargets(1, &g_backbuffer_rtv, nullptr);
        g_context->ClearRenderTargetView(g_backbuffer_rtv, clear);
        g_swapchain->Present(headless ? 0 : 1, 0);

        if (headless && (GetTickCount() - start) >= static_cast<DWORD>(timeout_seconds * 1000)) {
            break;
        }
    }

    unsigned long frames = overlay::frame_count();
    std::fprintf(stderr, "overlay rendered %lu frames\n", frames);
    overlay::detach();
    shutdown();

    if (headless && frames == 0) {
        std::fprintf(stderr, "FAIL: hook never fired\n");
        return 4;
    }
    std::fprintf(stderr, "OK\n");
    return 0;
}
