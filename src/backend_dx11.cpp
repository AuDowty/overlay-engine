#include <overlay/overlay.hpp>
#include <microhook/microhook.hpp>

#include <atomic>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

namespace overlay::detail {

extern LRESULT install_wndproc_hook(HWND target, WNDPROC* out_original);
extern void uninstall_wndproc_hook(HWND target, WNDPROC original);
extern WNDPROC g_original_wndproc;

std::atomic<unsigned long> g_frame_count{0};

namespace {

typedef HRESULT(__stdcall PresentFn)(IDXGISwapChain*, UINT, UINT);

microhook::Hook g_present_hook{};
RenderFn g_render = nullptr;
std::mutex g_init_mutex;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
HWND g_hwnd = nullptr;
bool g_imgui_initialized = false;

void release_rtv() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

bool init_device_from_swapchain(IDXGISwapChain* sc) {
    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_device)))) {
        return false;
    }
    g_device->GetImmediateContext(&g_context);
    DXGI_SWAP_CHAIN_DESC desc{};
    sc->GetDesc(&desc);
    g_hwnd = desc.OutputWindow;
    return true;
}

bool create_rtv(IDXGISwapChain* sc) {
    ID3D11Texture2D* bb = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&bb)))) {
        return false;
    }
    HRESULT hr = g_device->CreateRenderTargetView(bb, nullptr, &g_rtv);
    bb->Release();
    return SUCCEEDED(hr);
}

HRESULT __stdcall hooked_present(IDXGISwapChain* swapchain, UINT sync_interval, UINT flags) {
    auto* call_original = microhook::trampoline_as<PresentFn>(g_present_hook);

    if (!g_imgui_initialized) {
        std::lock_guard lock(g_init_mutex);
        if (!g_imgui_initialized) {
            if (!init_device_from_swapchain(swapchain)) {
                return call_original(swapchain, sync_interval, flags);
            }
            if (!create_rtv(swapchain)) {
                return call_original(swapchain, sync_interval, flags);
            }
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(g_hwnd);
            ImGui_ImplDX11_Init(g_device, g_context);
            install_wndproc_hook(g_hwnd, &g_original_wndproc);
            g_imgui_initialized = true;
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (g_render) {
        g_render();
    }
    ImGui::Render();
    g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_frame_count.fetch_add(1, std::memory_order_relaxed);

    return call_original(swapchain, sync_interval, flags);
}

void* extract_present_address() {
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "overlay-engine-dummy";
    if (!RegisterClassExA(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return nullptr;
        }
    }
    HWND dummy = CreateWindowExA(0, "overlay-engine-dummy", "", WS_OVERLAPPEDWINDOW,
                                 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy) return nullptr;

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = dummy;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swapchain = nullptr;
    ID3D11Device* device = nullptr;
    D3D_FEATURE_LEVEL fl{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &swapchain, &device, &fl, nullptr);
    if (FAILED(hr)) {
        DestroyWindow(dummy);
        return nullptr;
    }

    void** vtable = *reinterpret_cast<void***>(swapchain);
    void* present = vtable[8];

    swapchain->Release();
    device->Release();
    DestroyWindow(dummy);

    return present;
}

}

Status dx11_attach(RenderFn render) {
    if (g_present_hook.installed) return Status::AlreadyAttached;
    g_render = render;

    void* present_addr = extract_present_address();
    if (!present_addr) return Status::DummyDeviceFailed;

    auto status = microhook::install(
        present_addr,
        reinterpret_cast<void*>(&hooked_present),
        g_present_hook);
    if (status != microhook::Status::Ok) {
        return Status::HookInstallFailed;
    }
    return Status::Ok;
}

Status dx11_detach() {
    if (!g_present_hook.installed) return Status::NotAttached;

    microhook::uninstall(g_present_hook);

    if (g_imgui_initialized) {
        if (g_hwnd && g_original_wndproc) {
            uninstall_wndproc_hook(g_hwnd, g_original_wndproc);
            g_original_wndproc = nullptr;
        }
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imgui_initialized = false;
    }
    release_rtv();
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
    g_render = nullptr;
    g_hwnd = nullptr;
    return Status::Ok;
}

bool dx11_is_attached() {
    return g_present_hook.installed;
}

}
