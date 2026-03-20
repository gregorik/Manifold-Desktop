#include "pch.h"
#include "ProcessManager.h"
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cwctype>

namespace Manifold::Core {

    namespace {
        bool StartsWithI(const std::wstring& text, const std::wstring& prefix) {
            if (text.size() < prefix.size()) return false;
            for (size_t i = 0; i < prefix.size(); ++i) {
                wchar_t a = static_cast<wchar_t>(towlower(text[i]));
                wchar_t b = static_cast<wchar_t>(towlower(prefix[i]));
                if (a != b) return false;
            }
            return true;
        }

        std::wstring BuildLaunchCommandLine(const std::wstring& commandLine) {
            if (commandLine.empty()) return commandLine;

            // If the caller already provided cmd.exe, don't wrap again.
            if (StartsWithI(commandLine, L"cmd.exe") || StartsWithI(commandLine, L"cmd ")) {
                return commandLine;
            }

            // Use cmd.exe so PATH resolution works for .cmd/.bat (e.g., npm-installed Gemini).
            // Pattern: cmd.exe /d /s /c ""<commandLine>""
            std::wstring wrapped = L"\"\"";
            wrapped += commandLine;
            wrapped += L"\"\"";
            return L"cmd.exe /d /s /c " + wrapped;
        }
    }

    ProcessManager::ProcessManager() {}

    ProcessManager::~ProcessManager() {
        Stop();
    }

    void ProcessManager::InitializePipes() {
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = FALSE; // We don't inherit pipes; ConPTY handles I/O
        saAttr.lpSecurityDescriptor = NULL;

        // Create Pipe for Child STDOUT
        if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0))
            throw std::runtime_error("Stdout Pipe creation failed");

        // Create Pipe for Child STDIN
        if (!CreatePipe(&m_hChildStd_IN_Rd, &m_hChildStd_IN_Wr, &saAttr, 0))
            throw std::runtime_error("Stdin Pipe creation failed");
    }

    void ProcessManager::Start(const std::wstring& cmd) {
        Stop();

        InitializePipes();

        std::wstring launchCmd = BuildLaunchCommandLine(cmd);
        std::vector<wchar_t> cmdLine(launchCmd.begin(), launchCmd.end());
        cmdLine.push_back(L'\0'); // CreateProcessW requires a writable buffer

        // Create a pseudo console to give Gemini a real TTY (fixes no prompt/output).
        COORD consoleSize{ 120, 30 };
        HRESULT hr = CreatePseudoConsole(consoleSize, m_hChildStd_IN_Rd, m_hChildStd_OUT_Wr, 0, &m_hPseudoConsole);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create pseudo console");
        }

        // After CreatePseudoConsole, close the handles passed to it.
        m_hChildStd_IN_Rd.Close();
        m_hChildStd_OUT_Wr.Close();

        STARTUPINFOEXW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        ZeroMemory(&pi, sizeof(pi));

        SIZE_T attrListSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
        std::vector<uint8_t> attrListBuffer(attrListSize);
        si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrListBuffer.data());
        if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize)) {
            if (m_hPseudoConsole) {
                ClosePseudoConsole(m_hPseudoConsole);
                m_hPseudoConsole = nullptr;
            }
            throw std::runtime_error("Failed to initialize attribute list");
        }

        if (!UpdateProcThreadAttribute(
            si.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            m_hPseudoConsole,
            sizeof(HPCON),
            nullptr,
            nullptr)) {
            DeleteProcThreadAttributeList(si.lpAttributeList);
            if (m_hPseudoConsole) {
                ClosePseudoConsole(m_hPseudoConsole);
                m_hPseudoConsole = nullptr;
            }
            throw std::runtime_error("Failed to set pseudo console attribute");
        }

        // Create the Child Process
        BOOL bSuccess = CreateProcessW(
            NULL, 
            cmdLine.data(), // Command line (mutable buffer)
            NULL, NULL, 
            FALSE, // Handles are not inherited (ConPTY handles I/O)
            EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, 
            &si.StartupInfo, &pi);

        DeleteProcThreadAttributeList(si.lpAttributeList);

        if (!bSuccess) {
            if (m_hPseudoConsole) {
                ClosePseudoConsole(m_hPseudoConsole);
                m_hPseudoConsole = nullptr;
            }
            m_hChildStd_IN_Wr.Close();
            m_hChildStd_OUT_Rd.Close();
            throw std::runtime_error("Failed to launch Gemini process");
        }

        m_hProcess.Reset(pi.hProcess);
        CloseHandle(pi.hThread); // We don't need the primary thread handle

        m_running = true;
        m_readerThread = std::jthread(&ProcessManager::ReadLoop, this);
    }

    void ProcessManager::WriteInput(const std::wstring& input) {
        if (!m_running) return;

        std::wstring toSend = input;
        if (toSend.empty() || (toSend.back() != L'\n' && toSend.back() != L'\r')) {
            toSend += L"\n";
        }
        WriteRawInput(toSend);
    }

    void ProcessManager::WriteRawInput(const std::wstring& input) {
        if (!m_running) return;

        // Convert Wide input back to UTF-8 for the CLI
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.length(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.length(), &strTo[0], size_needed, NULL, NULL);

        DWORD dwWritten;
        WriteFile(m_hChildStd_IN_Wr, strTo.c_str(), (DWORD)strTo.size(), &dwWritten, NULL);
    }

    void ProcessManager::ReadLoop() {
        const int BUFSIZE = 4096;
        char chBuf[BUFSIZE];
        DWORD dwRead;

        while (m_running) {
            BOOL bSuccess = ReadFile(m_hChildStd_OUT_Rd, chBuf, BUFSIZE, &dwRead, NULL);
            if (!bSuccess || dwRead == 0) break;

            std::string rawChunk(chBuf, dwRead);
            std::wstring wideChunk = ConvertUtf8ToWide(rawChunk);

            std::lock_guard<std::mutex> lock(m_callbackMutex);
            if (m_callback) {
                m_callback(wideChunk);
            }
        }

        m_running = false;
    }

    std::wstring ProcessManager::ConvertUtf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    void ProcessManager::Stop() {
        if (!m_running && m_hProcess.h == INVALID_HANDLE_VALUE) {
            return;
        }

        m_running = false;

        // Clear callback under lock — waits for any in-flight invocation to finish
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            m_callback = nullptr;
        }

        m_hChildStd_IN_Wr.Close();

        if (m_hPseudoConsole) {
            ClosePseudoConsole(m_hPseudoConsole);
            m_hPseudoConsole = nullptr;
        }

        if (m_hProcess.h != INVALID_HANDLE_VALUE) {
            DWORD waitResult = WaitForSingleObject(m_hProcess, 2000);
            if (waitResult == WAIT_TIMEOUT) {
                TerminateProcess(m_hProcess, 0);
            }
            m_hProcess.Close();
        }

        m_hChildStd_OUT_Rd.Close();

        // Join the reader thread explicitly (jthread destructor would do this,
        // but we need it done before Stop() returns so callers can safely
        // destroy the ProcessManager/TerminalEmulator)
        if (m_readerThread.joinable()) {
            m_readerThread = std::jthread{}; // request_stop + join via move-assign
        }
    }

    void ProcessManager::SetOutputCallback(std::function<void(const std::wstring&)> callback) {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_callback = callback;
    }

    void ProcessManager::WriteRawBytes(const std::string& input) {
        if (!m_running) return;
        DWORD dwWritten;
        WriteFile(m_hChildStd_IN_Wr, input.c_str(), (DWORD)input.size(), &dwWritten, NULL);
    }

    void ProcessManager::Resize(SHORT cols, SHORT rows) {
        if (m_hPseudoConsole) {
            COORD size{ cols, rows };
            ResizePseudoConsole(m_hPseudoConsole, size);
        }
    }

    bool ProcessManager::IsRunning() const {
        if (m_hProcess.h == INVALID_HANDLE_VALUE) return false;
        DWORD exitCode = 0;
        if (!GetExitCodeProcess(m_hProcess, &exitCode)) return false;
        return exitCode == STILL_ACTIVE;
    }


}
