#pragma once
#include "MCPTransport.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>
#include <windows.h>

namespace Manifold::Core::MCP {

class StdioTransport : public MCPTransport {
public:
    StdioTransport(const std::string& command, const std::vector<std::string>& args = {});
    ~StdioTransport() override;

    bool Connect() override;
    void Disconnect() override;
    bool IsConnected() const override;
    JsonRpcResponse SendRequest(const JsonRpcRequest& req) override;

private:
    void ReaderLoop();

    std::string m_command;
    std::vector<std::string> m_args;

    HANDLE m_processHandle = INVALID_HANDLE_VALUE;
    HANDLE m_stdinWrite = INVALID_HANDLE_VALUE;
    HANDLE m_stdoutRead = INVALID_HANDLE_VALUE;

    std::jthread m_readerThread;
    std::atomic<bool> m_connected{ false };
    std::atomic<int> m_nextId{ 1 };

    std::mutex m_pendingMutex;
    std::condition_variable m_pendingCv;
    struct PendingRequest {
        bool ready = false;
        JsonRpcResponse response;
    };
    std::map<int, std::shared_ptr<PendingRequest>> m_pending;
};

} // namespace Manifold::Core::MCP
