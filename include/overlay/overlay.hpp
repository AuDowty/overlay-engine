#pragma once

namespace overlay {

enum class Status {
    Ok = 0,
    AlreadyAttached,
    NotAttached,
    DummyDeviceFailed,
    HookInstallFailed,
    UnsupportedBackend,
};

using RenderFn = void (*)();

Status attach(RenderFn render);
Status detach();
bool is_attached();

unsigned long frame_count();

}
