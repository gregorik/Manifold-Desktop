#pragma once
#include "MCPTypes.h"

namespace Manifold::Core::MCP {

class MCPTransport {
public:
    virtual ~MCPTransport() = default;
    virtual bool Connect() = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual JsonRpcResponse SendRequest(const JsonRpcRequest& req) = 0;
};

} // namespace Manifold::Core::MCP
