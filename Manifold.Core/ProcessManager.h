#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include "IProcessManager.h"
#include "ScopedHandle.h"

namespace Manifold::Core {

    class ProcessManager : public IProcessManager {
    public:
        ProcessManager();
        ~ProcessManager() override;

        // --- IProcessManager Implementation ---
        
        // Spawns gemini.exe (hidden)
        void Start(const std::wstring& executablePath) override;
        
        // Clean shutdown
        void Stop() override;
        
        // Sends keyboard input/commands to the CLI
        void WriteInput(const std::wstring& input) override;

        // Sends raw keystrokes (no newline normalization)
        void WriteRawInput(const std::wstring& input) override;

        // Write raw UTF-8 bytes directly (no wstring conversion round-trip)
        void WriteRawBytes(const std::string& input);

        // Resize the pseudo console
        void Resize(SHORT cols, SHORT rows);
        
        // Hook for the UI to listen to stdout
        void SetOutputCallback(DataReceivedCallback cb) override;

        // Check if the background process is still active
        bool IsRunning() const override;

    private:
        // -- Win32 Pipes & Handles --
        ScopedHandle m_hChildStd_IN_Rd;  // Read side of Input Pipe (Child uses this)
        ScopedHandle m_hChildStd_IN_Wr;  // Write side of Input Pipe (We use this)
        ScopedHandle m_hChildStd_OUT_Rd; // Read side of Output Pipe (We use this)
        ScopedHandle m_hChildStd_OUT_Wr; // Write side of Output Pipe (Child uses this)
        ScopedHandle m_hProcess;         // Handle to the child process
        HPCON m_hPseudoConsole{ nullptr }; // Pseudo console for TTY-like behavior

        // -- Threading & State --
        std::jthread m_readerThread;     // Auto-joining thread (C++20)
        std::atomic<bool> m_running{ false };
        DataReceivedCallback m_callback;
        std::mutex m_callbackMutex;

        // -- Internal Helpers --
        void ReadLoop();
        void InitializePipes();
        std::wstring ConvertUtf8ToWide(const std::string& str);
    };
}
