#include <overlay/overlay.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace overlay::detail {

WNDPROC g_original_wndproc = nullptr;

namespace {

LRESULT CALLBACK overlay_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui::GetCurrentContext()) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
            return TRUE;
        }
        ImGuiIO& io = ImGui::GetIO();
        bool block_mouse = io.WantCaptureMouse && (
            msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
            msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_MBUTTONDOWN ||
            msg == WM_MBUTTONUP || msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL);
        bool block_keyboard = io.WantCaptureKeyboard && (
            msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR ||
            msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP);
        if (block_mouse || block_keyboard) {
            return TRUE;
        }
    }
    return CallWindowProcW(g_original_wndproc, hwnd, msg, wparam, lparam);
}

}

LRESULT install_wndproc_hook(HWND target, WNDPROC* out_original) {
    LONG_PTR prev = SetWindowLongPtrW(target, GWLP_WNDPROC,
                                      reinterpret_cast<LONG_PTR>(overlay_wndproc));
    *out_original = reinterpret_cast<WNDPROC>(prev);
    return prev;
}

void uninstall_wndproc_hook(HWND target, WNDPROC original) {
    SetWindowLongPtrW(target, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
}

}
