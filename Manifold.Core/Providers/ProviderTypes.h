#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include "../json.hpp"

namespace Manifold::Core {

// --- Content Parts ---

struct TextPart {
    std::string text;
};

struct ImagePart {
    std::string mimeType;
    std::string base64Data;
};

struct FilePart {
    std::string filename;
    std::string mimeType;
    std::string base64Data;
};

struct ContentPart {
    enum class Type { Text, Image, File };
    Type type;
    std::optional<TextPart> text;
    std::optional<ImagePart> image;
    std::optional<FilePart> file;

    static ContentPart MakeText(const std::string& t) {
        return { Type::Text, TextPart{t}, std::nullopt, std::nullopt };
    }
    static ContentPart MakeImage(const std::string& mime, const std::string& b64) {
        return { Type::Image, std::nullopt, ImagePart{mime, b64}, std::nullopt };
    }
    static ContentPart MakeFile(const std::string& name, const std::string& mime, const std::string& b64) {
        return { Type::File, std::nullopt, std::nullopt, FilePart{name, mime, b64} };
    }
};

// --- Tool Types ---

struct ToolParameter {
    std::string name;
    std::string type;
    std::string description;
    bool required = false;
    nlohmann::json schema;
};

struct ToolDefinition {
    std::string name;
    std::string description;
    std::vector<ToolParameter> parameters;
    std::string serverId;

    nlohmann::json ToFunctionSchema() const {
        nlohmann::json properties = nlohmann::json::object();
        std::vector<std::string> requiredParams;
        for (auto& p : parameters) {
            if (!p.schema.is_null()) {
                properties[p.name] = p.schema;
            } else {
                properties[p.name] = {{"type", p.type}, {"description", p.description}};
            }
            if (p.required) requiredParams.push_back(p.name);
        }
        nlohmann::json schema = {
            {"type", "object"},
            {"properties", properties}
        };
        if (!requiredParams.empty()) schema["required"] = requiredParams;
        return {
            {"name", name},
            {"description", description},
            {"parameters", schema}
        };
    }
};

struct ToolCall {
    std::string id;
    std::string name;
    nlohmann::json arguments;
};

struct ToolResult {
    std::string callId;
    std::string name;
    nlohmann::json content;
    bool isError = false;
};

// --- Model & Provider Info ---

struct ModelInfo {
    std::string id;
    std::string displayName;
    bool isFree = false;
    int freeRpmLimit = 0;
    std::string note;
};

// --- Chat Messages ---

struct ChatMessage {
    std::string role;
    std::string text;
    std::vector<ContentPart> parts;
    std::optional<ToolCall> toolCall;
    std::optional<ToolResult> toolResult;
};

// --- Request / Response ---

struct ChatRequest {
    std::string providerId;
    std::string modelId;
    std::vector<ChatMessage> messages;
    std::string systemPrompt;
    double temperature = 0.7;
    std::vector<ToolDefinition> tools;
    std::string apiKey;
    int maxToolCallRounds = 10;
};

struct StreamChunk {
    std::string text;
    bool done = false;
    std::optional<ToolCall> toolCall;
};

struct ChatResponse {
    std::string text;
    std::vector<ToolCall> toolCalls;
    int promptTokens = 0;
    int completionTokens = 0;
    std::string finishReason;
};

} // namespace Manifold::Core
