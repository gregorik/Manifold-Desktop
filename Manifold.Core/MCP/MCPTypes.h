#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../json.hpp"

namespace Manifold::Core::MCP {

struct JsonRpcRequest {
    std::string jsonrpc = "2.0";
    nlohmann::json id;
    std::string method;
    nlohmann::json params = nlohmann::json::object();

    nlohmann::json ToJson() const {
        nlohmann::json j = {
            {"jsonrpc", jsonrpc},
            {"method", method},
            {"params", params}
        };
        if (!id.is_null()) j["id"] = id;
        return j;
    }
};

struct JsonRpcError {
    int code = 0;
    std::string message;
    nlohmann::json data;
};

struct JsonRpcResponse {
    std::string jsonrpc = "2.0";
    nlohmann::json id;
    nlohmann::json result;
    std::optional<JsonRpcError> error;

    static JsonRpcResponse FromJson(const nlohmann::json& j) {
        JsonRpcResponse resp;
        resp.jsonrpc = j.value("jsonrpc", "2.0");
        if (j.contains("id")) resp.id = j["id"];
        if (j.contains("result")) resp.result = j["result"];
        if (j.contains("error")) {
            JsonRpcError err;
            auto& ej = j["error"];
            err.code = ej.value("code", 0);
            err.message = ej.value("message", "");
            if (ej.contains("data")) err.data = ej["data"];
            resp.error = err;
        }
        return resp;
    }
};

struct McpToolInputSchema {
    std::string type = "object";
    nlohmann::json properties = nlohmann::json::object();
    std::vector<std::string> required;
};

struct McpToolInfo {
    std::string name;
    std::string description;
    McpToolInputSchema inputSchema;

    static McpToolInfo FromJson(const nlohmann::json& j) {
        McpToolInfo t;
        t.name = j.value("name", "");
        t.description = j.value("description", "");
        if (j.contains("inputSchema")) {
            auto& s = j["inputSchema"];
            t.inputSchema.type = s.value("type", "object");
            if (s.contains("properties")) t.inputSchema.properties = s["properties"];
            if (s.contains("required")) {
                for (auto& r : s["required"]) t.inputSchema.required.push_back(r.get<std::string>());
            }
        }
        return t;
    }
};

struct McpResource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mimeType;

    static McpResource FromJson(const nlohmann::json& j) {
        McpResource r;
        r.uri = j.value("uri", "");
        r.name = j.value("name", "");
        r.description = j.value("description", "");
        r.mimeType = j.value("mimeType", "");
        return r;
    }
};

struct McpResourceContent {
    std::string uri;
    std::string mimeType;
    std::string text;
};

} // namespace Manifold::Core::MCP
