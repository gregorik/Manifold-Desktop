#include "pch.h"
#include "SSETransport.h"
#include "../Providers/HttpClient.h"
#include <sstream>

namespace Manifold::Core::MCP {

SSETransport::SSETransport(const std::string& url) : m_baseUrl(url) {
    // SSE endpoint is typically at /sse, POST at /message
    if (m_baseUrl.back() == '/') m_baseUrl.pop_back();
    m_sseEndpoint = m_baseUrl + "/sse";
    m_postEndpoint = m_baseUrl + "/message";
}

SSETransport::~SSETransport() {
    Disconnect();
}

bool SSETransport::Connect() {
    if (m_connected) return true;
    m_connected = true;

    // Start SSE listener thread
    m_sseThread = std::jthread([this](std::stop_token stopToken) { SSELoop(); });

    // Give the SSE connection a moment to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return m_connected;
}

void SSETransport::Disconnect() {
    m_connected = false;
    if (m_sseThread.joinable()) {
        m_sseThread.request_stop();
        m_sseThread = std::jthread{};
    }

    std::lock_guard lock(m_pendingMutex);
    for (auto& [id, p] : m_pending) {
        p->ready = true;
        p->response.error = JsonRpcError{ -1, "Transport disconnected" };
    }
    m_pendingCv.notify_all();
}

bool SSETransport::IsConnected() const {
    return m_connected;
}

JsonRpcResponse SSETransport::SendRequest(const JsonRpcRequest& req) {
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

    auto jReq = req.ToJson();
    jReq["id"] = reqId;

    // POST the request
    try {
        HttpClient::Post(m_postEndpoint, jReq.dump(), {{"Content-Type", "application/json"}});
    } catch (...) {
        std::lock_guard lock(m_pendingMutex);
        m_pending.erase(reqId);
        JsonRpcResponse err;
        err.error = JsonRpcError{ -1, "HTTP POST failed" };
        return err;
    }

    // Wait for response via SSE
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

void SSETransport::SSELoop() {
    try {
        HttpClient::StreamPost(m_sseEndpoint, "",
            {{"Accept", "text/event-stream"}},
            [this](const std::string& chunk) -> bool {
                if (!m_connected) return false;

                // Parse SSE data lines
                std::istringstream stream(chunk);
                std::string line;
                while (std::getline(stream, line)) {
                    if (line.size() < 6 || line.substr(0, 6) != "data: ") continue;
                    auto data = line.substr(6);
                    if (data.empty()) continue;

                    auto j = nlohmann::json::parse(data, nullptr, false);
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
                return true;
            });
    } catch (...) {
        m_connected = false;
    }
}

} // namespace Manifold::Core::MCP
