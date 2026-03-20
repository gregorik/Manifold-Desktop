#include "pch.h"
#include "StdioTransport.h"
#include "../StringUtils.h"

namespace Manifold::Core::MCP {

StdioTransport::StdioTransport(const std::string& command, const std::vector<std::string>& args)
    : m_command(command), m_args(args) {}

StdioTransport::~StdioTransport() {
    Disconnect();
}

bool StdioTransport::Connect() {
    if (m_connected) return true;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

    HANDLE stdinRead = INVALID_HANDLE_VALUE, stdinWrite = INVALID_HANDLE_VALUE;
    HANDLE stdoutRead = INVALID_HANDLE_VALUE, stdoutWrite = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0)) return false;
    if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0)) {
        CloseHandle(stdinRead); CloseHandle(stdinWrite);
        return false;
    }

    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);

    // Build command line
    std::string cmdLine = m_command;
    for (auto& arg : m_args) {
        cmdLine += " " + arg;
    }
    auto wCmdLine = Utf8ToWide(cmdLine);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stdoutWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(nullptr, wCmdLine.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);

    if (!ok) {
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        return false;
    }

    CloseHandle(pi.hThread);
    m_processHandle = pi.hProcess;
    m_stdinWrite = stdinWrite;
    m_stdoutRead = stdoutRead;
    m_connected = true;

    m_readerThread = std::jthread([this](std::stop_token) { ReaderLoop(); });
    return true;
}

void StdioTransport::Disconnect() {
    m_connected = false;

    if (m_readerThread.joinable()) {
        m_readerThread.request_stop();
    }

    if (m_stdinWrite != INVALID_HANDLE_VALUE) {
        CloseHandle(m_stdinWrite);
        m_stdinWrite = INVALID_HANDLE_VALUE;
    }
    if (m_stdoutRead != INVALID_HANDLE_VALUE) {
        CloseHandle(m_stdoutRead);
        m_stdoutRead = INVALID_HANDLE_VALUE;
    }

    if (m_processHandle != INVALID_HANDLE_VALUE) {
        TerminateProcess(m_processHandle, 0);
        WaitForSingleObject(m_processHandle, 3000);
        CloseHandle(m_processHandle);
        m_processHandle = INVALID_HANDLE_VALUE;
    }

    if (m_readerThread.joinable()) {
        m_readerThread = std::jthread{};
    }

    // Wake any waiting senders
    {
        std::lock_guard lock(m_pendingMutex);
        for (auto& [id, p] : m_pending) {
            p->ready = true;
            p->response.error = JsonRpcError{ -1, "Transport disconnected" };
        }
    }
    m_pendingCv.notify_all();
}

bool StdioTransport::IsConnected() const {
    return m_connected;
}

JsonRpcResponse StdioTransport::SendRequest(const JsonRpcRequest& req) {
    if (!m_connected) {
        JsonRpcResponse err;
        err.error = JsonRpcError{ -1, "Not connected" };
        return err;
    }

    int reqId = m_nextId++;
    auto pending = std::make_shared<PendingRequest>();

    {
        std::lock_guard lock(m_pendingMutex);
        m_pending[reqId] = pending;
    }

    // Build request with integer id
    auto jReq = req.ToJson();
    jReq["id"] = reqId;
    auto line = jReq.dump() + "\n";

    DWORD written;
    WriteFile(m_stdinWrite, line.c_str(), (DWORD)line.size(), &written, nullptr);

    // Wait for response
    {
        std::unique_lock lock(m_pendingMutex);
        m_pendingCv.wait_for(lock, std::chrono::seconds(30), [&]{ return pending->ready; });
    }

    {
        std::lock_guard lock(m_pendingMutex);
        m_pending.erase(reqId);
    }

    if (!pending->ready) {
        JsonRpcResponse err;
        err.error = JsonRpcError{ -1, "Request timed out" };
        return err;
    }

    return pending->response;
}

void StdioTransport::ReaderLoop() {
    std::string buffer;
    char buf[4096];

    while (m_connected) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(m_stdoutRead, buf, sizeof(buf), &bytesRead, nullptr);
        if (!ok || bytesRead == 0) break;

        buffer.append(buf, bytesRead);

        // Process complete lines
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            auto line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);

            if (line.empty() || line[0] != '{') continue;

            auto j = nlohmann::json::parse(line, nullptr, false);
            if (j.is_discarded()) continue;

            auto resp = JsonRpcResponse::FromJson(j);

            if (resp.id.is_number_integer()) {
                int id = resp.id.get<int>();
                std::lock_guard lock(m_pendingMutex);
                auto it = m_pending.find(id);
                if (it != m_pending.end()) {
                    it->second->response = std::move(resp);
                    it->second->ready = true;
                    m_pendingCv.notify_all();
                }
            }
        }
    }
    m_connected = false;
}

} // namespace Manifold::Core::MCP
