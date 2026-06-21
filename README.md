# overlay-engine

In-process Windows overlay engine. Hooks the graphics-API present/swap-buffers function in a running app, draws an ImGui frame on top, passes input through cleanly. Same approach as Steam overlay, RTSS, Discord overlay.

**Backends:** DX11 ✓ · DX12 (planned) · OpenGL (planned) · Vulkan (planned).

## Intended use

Built for legitimate Windows graphics tooling — same category as RivaTuner Statistics Server, MSI Afterburner, Steam overlay, Discord's in-game overlay:

- **Performance HUDs** — FPS counters, frame-time graphs, GPU/CPU stats
- **Debug visualization** — render coordinate frames, bounding boxes, telemetry on top of your own engine during development
- **Game-modding ecosystems** — mod menus, debug consoles in Minecraft/Skyrim-style modding
- **Accessibility tools** — closed captions, contrast overlays for color-blind users
- **Streaming/recording overlays** — stat displays, alerts

**Don't use this against software you don't own or aren't authorized to instrument.**

## How it works (DX11)

1. Spin up a hidden window + dummy `D3D11CreateDeviceAndSwapChain`
2. Read `vtable[8]` of the dummy swapchain — that's `IDXGISwapChain::Present`. Same function across the whole process because COM vtables are per-interface, not per-instance.
3. Hook that address with [`microhook`](https://github.com/AuDowty/microhook) (your own published inline hooker)
4. First time the real app's `Present` fires, extract the device + context from the swapchain, create a back-buffer RTV, init ImGui DX11+Win32, subclass the window proc for input
5. Every subsequent `Present`: build an ImGui frame, render into the swap chain's RTV, then call the original Present

Input passthrough: forwards messages to ImGui's Win32 handler; swallows mouse/keyboard input only when ImGui's `WantCaptureMouse` / `WantCaptureKeyboard` is set (so the game keeps getting input when no menu is open).

## Build

```
cmake -B build -A x64
cmake --build build --config Release
```

FetchContent pulls [microhook](https://github.com/AuDowty/microhook) and [Dear ImGui](https://github.com/ocornut/imgui) automatically.

## Use

```cpp
#include <overlay/overlay.hpp>
#include <imgui.h>

static void render_my_overlay() {
    ImGui::Begin("hello");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
}

// In your existing DX11 app, or a DLL injected into one:
overlay::attach(render_my_overlay);
// ... run as normal; every Present() now draws your overlay on top ...
overlay::detach();
```

## Demo

```
.\build\Release\dx11_demo.exe          # interactive window with overlay
.\build\Release\dx11_demo.exe --smoke  # hidden, 2-second run, asserts hook fired
```

The smoke run creates a real DX11 swapchain, attaches the overlay, calls `Present` for two seconds, then verifies `overlay::frame_count() > 0`. Used for CI / regression testing.

## Roadmap

- **W2:** OpenGL backend (`wglSwapBuffers` hook)
- **W3:** DX12 backend (different vtable, more lifecycle bookkeeping)
- **W4:** Vulkan backend (`vkQueuePresentKHR`), polish, GIF demos

## License

MIT.
