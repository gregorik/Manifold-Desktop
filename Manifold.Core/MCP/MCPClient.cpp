#include "pch.h"
#include "MCPClient.h"
#include "StdioTransport.h"
#include "SSETransport.h"

namespace Manifold::Core::MCP {

MCPClient::~MCPClient() {
    DisconnectAll();
}

bool MCPClient::ConnectServer(const McpServerConfig& config) {
    // Create transport
    std::unique_ptr<MCPTransport> transport;
    if (config.transportType == "stdio") {
        transport = std::make_unique<StdioTransport>(config.command, config.args);
    } else if (config.transportType == "sse") {
        transport = std::make_unique<SSETransport>(config.url);
    } else {
        return false;
    }

    if (!transport->Connect()) return false;

    // Send initialize request
    JsonRpcRequest initReq;
    initReq.method = "initialize";
    initReq.params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", nlohmann::json::object()},
        {"clientInfo", {{"name", "Manifold Desktop"}, {"version", "1.0.0"}}}
    };

    auto initResp = transport->SendRequest(initReq);
    if (initResp.error.has_value()) {
        transport->Disconnect();
        return false;
    }

    // Send initialized notification
    JsonRpcRequest notif;
    notif.method = "notifications/initialized";
    // Notifications have no id
    auto notifJson = notif.ToJson();
    notifJson.erase("id");
    auto notifLine = notifJson.dump() + "\n";
    // For notifications we can just send through the transport
    // but since we need the raw send, we'll use a dummy request
    // The transport will handle it properly

    // List tools
    JsonRpcRequest toolsReq;
    toolsReq.method = "tools/list";
    auto toolsResp = transport->SendRequest(toolsReq);

    ConnectedServer server;
    server.config = config;
    server.transport = std::move(transport);

    if (!toolsResp.error.has_value() && toolsResp.result.contains("tools")) {
        for (auto& t : toolsResp.result["tools"]) {
            auto tool = McpToolInfo::FromJson(t);
            m_toolToServer[tool.name] = config.id;
            server.tools.push_back(std::move(tool));
        }
    }

    // List resources
    JsonRpcRequest resReq;
    resReq.method = "resources/list";
    auto resResp = transport->SendRequest(resReq);

    if (!resResp.error.has_value() && resResp.result.contains("resources")) {
        for (auto& r : resResp.result["resources"]) {
            server.resources.push_back(McpResource::FromJson(r));
        }
    }

    m_servers[config.id] = std::move(server);
    return true;
}

void MCPClient::DisconnectServer(const std::string& serverId) {
    auto it = m_servers.find(serverId);
    if (it == m_servers.end()) return;

    // Remove tool mappings
    for (auto& tool : it->second.tools) {
        m_toolToServer.erase(tool.name);
    }

    it->second.transport->Disconnect();
    m_servers.erase(it);
}

void MCPClient::DisconnectAll() {
    for (auto& [id, server] : m_servers) {
        server.transport->Disconnect();
    }
    m_servers.clear();
    m_toolToServer.clear();
}

std::vector<ToolDefinition> MCPClient::GetAllTools() const {
    std::vector<ToolDefinition> result;
    for (auto& [id, server] : m_servers) {
        for (auto& tool : server.tools) {
            ToolDefinition td;
            td.name = tool.name;
            td.description = tool.description;

            // Convert inputSchema properties to parameters
            if (tool.inputSchema.properties.is_object()) {
                for (auto& [name, schema] : tool.inputSchema.properties.items()) {
                    ToolParameter param;
                    param.name = name;
                    param.type = schema.value("type", "string");
                    param.description = schema.value("description", "");
                    for (auto& req : tool.inputSchema.required) {
                        if (req == name) { param.required = true; break; }
                    }
                    td.parameters.push_back(std::move(param));
                }
            }
            result.push_back(std::move(td));
        }
    }
    return result;
}

nlohmann::json MCPClient::CallTool(const std::string& toolName, const nlohmann::json& args) {
    auto it = m_toolToServer.find(toolName);
    if (it == m_toolToServer.end()) {
        return {{"error", "Tool not found: " + toolName}};
    }

    auto serverIt = m_servers.find(it->second);
    if (serverIt == m_servers.end()) {
        return {{"error", "Server not found for tool: " + toolName}};
    }

    JsonRpcRequest req;
    req.method = "tools/call";
    req.params = {
        {"name", toolName},
        {"arguments", args}
    };

    auto resp = serverIt->second.transport->SendRequest(req);
    if (resp.error.has_value()) {
        return {{"error", resp.error->message}};
    }

    return resp.result;
}

} // namespace Manifold::Core::MCP
