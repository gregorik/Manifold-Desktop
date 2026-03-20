#pragma once
#include <windows.h>

namespace Manifold::Core {
    struct ScopedHandle {
        HANDLE h = INVALID_HANDLE_VALUE;

        ScopedHandle();
        explicit ScopedHandle(HANDLE handle);
        ~ScopedHandle();

        // Disable copy
        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle& operator=(const ScopedHandle&) = delete;

        // Enable move
        ScopedHandle(ScopedHandle&& other) noexcept;
        ScopedHandle& operator=(ScopedHandle&& other) noexcept;

        // Methods
        void Reset(HANDLE handle = INVALID_HANDLE_VALUE) {
            Close();
            h = handle;
        }

        void Close();

        // Operators
        operator HANDLE() const;
        HANDLE* operator&();
    };
}