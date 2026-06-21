#include <overlay/overlay.hpp>

#include <atomic>

namespace overlay::detail {
extern Status dx11_attach(RenderFn render);
extern Status dx11_detach();
extern bool dx11_is_attached();
extern std::atomic<unsigned long> g_frame_count;
}

namespace overlay {

Status attach(RenderFn render) {
    return detail::dx11_attach(render);
}

Status detach() {
    return detail::dx11_detach();
}

bool is_attached() {
    return detail::dx11_is_attached();
}

unsigned long frame_count() {
    return detail::g_frame_count.load();
}

}
