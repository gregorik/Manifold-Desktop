#include "pch.h"
#include "ScopedHandle.h"
#include <memory>

namespace Manifold::Core {
    ScopedHandle::ScopedHandle() : h(INVALID_HANDLE_VALUE) {}
    ScopedHandle::ScopedHandle(HANDLE handle) : h(handle) {}
    ScopedHandle::~ScopedHandle() {
        Close();
    }
    ScopedHandle::ScopedHandle(ScopedHandle&& other) noexcept : h(other.h) {
        other.h = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& ScopedHandle::operator=(ScopedHandle&& other) noexcept {
        if (this != std::addressof(other)) {
            Close();
            h = other.h;
            other.h = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    // Reset() is already defined inline in the header - DO NOT add it here

    void ScopedHandle::Close() {
        if (h != INVALID_HANDLE_VALUE && h != NULL) {
            ::CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
    }
    ScopedHandle::operator HANDLE() const {
        return h;
    }
    HANDLE* ScopedHandle::operator&() {
        return &h;
    }
}