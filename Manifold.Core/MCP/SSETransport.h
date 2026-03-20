#pragma once
#include "MCPTransport.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <atomic>

namespace Manifold::Core::MCP {

class SSETransport : public MCPTransport {
public:
    explicit SSETransport(const std::string& url);
    ~SSETransport() override;

    bool Connect() override;
    void Disconnect() override;
    bool IsConnected() const override;
    JsonRpcResponse SendRequest(const JsonRpcRequest& req) override;

private:
    void SSELoop();

    std::string m_baseUrl;
    std::string m_sseEndpoint;
    std::string m_postEndpoint;

    std::jthread m_sseThread;
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
