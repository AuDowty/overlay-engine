# overlay-engine

In-process Windows overlay engine. Hooks the graphics API present function, draws an ImGui frame on top, passes input through cleanly. Same approach as Steam overlay / RTSS / Discord overlay.

Currently supports DX11.

## How it works

1. Create a dummy swapchain, read `vtable[8]` to get `IDXGISwapChain::Present`
2. Hook that address with [microhook](https://github.com/AuDowty/microhook)
3. On first real `Present`: grab device + context, create a back-buffer RTV, init ImGui
4. Every subsequent `Present`: render an ImGui frame, then call the original

Input goes through ImGui's Win32 handler — the host app keeps getting input unless ImGui wants capture.

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

static void render() {
    ImGui::Begin("hello");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
}

overlay::attach(render);
// every Present() now draws your overlay
overlay::detach();
```

## Demo

```
.\build\Release\dx11_demo.exe          # interactive window with overlay
.\build\Release\dx11_demo.exe --smoke  # headless 2s run, verifies hook fired
```

## License

MIT
