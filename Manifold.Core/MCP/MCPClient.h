#pragma once
#include "MCPTypes.h"
#include "MCPTransport.h"
#include "../Providers/ProviderTypes.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace Manifold::Core::MCP {

struct McpServerConfig {
    std::string id;
    std::string name;
    std::string transportType; // "stdio" or "sse"
    std::string command;       // for stdio
    std::vector<std::string> args; // for stdio
    std::string url;           // for sse
};

struct ConnectedServer {
    McpServerConfig config;
    std::unique_ptr<MCPTransport> transport;
    std::vector<McpToolInfo> tools;
    std::vector<McpResource> resources;
};

class MCPClient {
public:
    MCPClient() = default;
    ~MCPClient();

    bool ConnectServer(const McpServerConfig& config);
    void DisconnectServer(const std::string& serverId);
    void DisconnectAll();

    std::vector<ToolDefinition> GetAllTools() const;
    nlohmann::json CallTool(const std::string& toolName, const nlohmann::json& args);

    const std::map<std::string, ConnectedServer>& GetServers() const { return m_servers; }

private:
    std::map<std::string, ConnectedServer> m_servers;

    // Map from tool name to server id for routing
    std::map<std::string, std::string> m_toolToServer;
};

} // namespace Manifold::Core::MCP
