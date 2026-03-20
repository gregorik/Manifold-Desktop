#pragma once
#include <string>
#include <functional>

namespace Manifold::Core {

    // Callback: The UI provides this function to receive text chunks.
    // Note: This may be called from a background thread.
    using DataReceivedCallback = std::function<void(const std::wstring& content)>;

    class IProcessManager {
    public:
        virtual ~IProcessManager() = default;

        // Initialize and spawn the backend executable
        virtual void Start(const std::wstring& executablePath) = 0;

        // Clean shutdown or force terminate
        virtual void Stop() = 0;

        // Send commands (wraps stdin writing)
        virtual void WriteInput(const std::wstring& input) = 0;

        // Send raw keystrokes (no newline normalization)
        virtual void WriteRawInput(const std::wstring& input) = 0;

        // Register the listener for stdout/stderr
        virtual void SetOutputCallback(DataReceivedCallback cb) = 0;

        // State check
        virtual bool IsRunning() const = 0;
    };
}
