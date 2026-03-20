#include "pch.h"
#include "AnthropicProvider.h"
#include "HttpClient.h"

namespace Manifold::Core {

std::vector<ModelInfo> AnthropicProvider::ListModels() const {
    return {
        {"claude-opus-4-6", "Claude Opus 4.6", false, 0, "Most capable"},
        {"claude-sonnet-4-6", "Claude Sonnet 4.6", false, 0, "Best balance"},
        {"claude-haiku-4-5-20251001", "Claude Haiku 4.5", false, 0, "Fast and affordable"},
    };
}

std::map<std::string, std::string> AnthropicProvider::AuthHeaders(const std::string& apiKey) const {
    return {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"}
    };
}

nlohmann::json AnthropicProvider::BuildRequestBody(const ChatRequest& req, bool stream) const {
    nlohmann::json messages = nlohmann::json::array();

    for (auto& msg : req.messages) {
        if (msg.role == "system") continue;

        nlohmann::json m;
        m["role"] = msg.role;

        if (msg.toolCall.has_value()) {
            m["role"] = "assistant";
            m["content"] = nlohmann::json::array({
                {
                    {"type", "tool_use"},
                    {"id", msg.toolCall->id},
                    {"name", msg.toolCall->name},
                    {"input", msg.toolCall->arguments}
                }
            });
        } else if (msg.toolResult.has_value()) {
            m["role"] = "user";
            m["content"] = nlohmann::json::array({
                {
                    {"type", "tool_result"},
                    {"tool_use_id", msg.toolResult->callId},
                    {"content", msg.toolResult->content.dump()},
                    {"is_error", msg.toolResult->isError}
                }
            });
        } else if (!msg.parts.empty()) {
            nlohmann::json content = nlohmann::json::array();
            for (auto& part : msg.parts) {
                if (part.type == ContentPart::Type::Text && part.text.has_value()) {
                    content.push_back({{"type", "text"}, {"text", part.text->text}});
                } else if (part.type == ContentPart::Type::Image && part.image.has_value()) {
                    content.push_back({{"type", "image"}, {"source", {
                        {"type", "base64"},
                        {"media_type", part.image->mimeType},
                        {"data", part.image->base64Data}
                    }}});
                }
            }
            m["content"] = content;
        } else {
            m["content"] = msg.text;
        }

        messages.push_back(m);
    }

    nlohmann::json body = {
        {"model", req.modelId},
        {"messages", messages},
        {"max_tokens", 4096},
        {"stream", stream}
    };

    if (!req.systemPrompt.empty()) {
        body["system"] = req.systemPrompt;
    }

    if (req.temperature >= 0) {
        body["temperature"] = req.temperature;
    }

    if (!req.tools.empty()) {
        nlohmann::json tools = nlohmann::json::array();
        for (auto& tool : req.tools) {
            auto schema = tool.ToFunctionSchema();
            tools.push_back({
                {"name", schema["name"]},
                {"description", schema["description"]},
                {"input_schema", schema["parameters"]}
            });
        }
        body["tools"] = tools;
    }

    return body;
}

ChatResponse AnthropicProvider::SendChat(const ChatRequest& req) {
    auto body = BuildRequestBody(req, false);
    auto headers = AuthHeaders(req.apiKey);

    auto httpResp = HttpClient::Post("https://api.anthropic.com/v1/messages", body.dump(), headers);
    if (httpResp.statusCode != 200) {
        return ChatResponse{ "", {}, 0, 0, "error" };
    }

    auto j = nlohmann::json::parse(httpResp.body, nullptr, false);
    if (j.is_discarded()) return ChatResponse{ "", {}, 0, 0, "error" };

    ChatResponse response;
    if (j.contains("content")) {
        for (auto& block : j["content"]) {
            if (block["type"] == "text") {
                response.text += block.value("text", "");
            } else if (block["type"] == "tool_use") {
                ToolCall tc;
                tc.id = block.value("id", "");
                tc.name = block.value("name", "");
                tc.arguments = block.value("input", nlohmann::json::object());
                response.toolCalls.push_back(std::move(tc));
            }
        }
    }

    if (j.contains("usage")) {
        response.promptTokens = j["usage"].value("input_tokens", 0);
        response.completionTokens = j["usage"].value("output_tokens", 0);
    }

    response.finishReason = j.value("stop_reason", "end_turn") == "tool_use" ? "tool_calls" : "stop";
    return response;
}

ChatResponse AnthropicProvider::StreamChat(const ChatRequest& req,
    std::function<void(const StreamChunk&)> onChunk) {

    auto body = BuildRequestBody(req, true);
    auto headers = AuthHeaders(req.apiKey);

    ChatResponse finalResponse;
    std::string sseBuffer;
    std::string currentToolId, currentToolName;
    std::string currentToolArgs;

    HttpClient::StreamPost("https://api.anthropic.com/v1/messages", body.dump(), headers,
        [&](const std::string& chunk) -> bool {
            sseBuffer += chunk;

            size_t pos = 0;
            while ((pos = sseBuffer.find("\n")) != std::string::npos) {
                auto line = sseBuffer.substr(0, pos);
                sseBuffer = sseBuffer.substr(pos + 1);

                if (line.substr(0, 6) != "data: ") continue;
                auto data = line.substr(6);

                auto j = nlohmann::json::parse(data, nullptr, false);
                if (j.is_discarded()) continue;

                auto eventType = j.value("type", "");

                if (eventType == "content_block_start") {
                    auto& block = j["content_block"];
                    if (block["type"] == "tool_use") {
                        currentToolId = block.value("id", "");
                        currentToolName = block.value("name", "");
                        currentToolArgs.clear();
                    }
                } else if (eventType == "content_block_delta") {
                    auto& delta = j["delta"];
                    if (delta["type"] == "text_delta") {
                        auto text = delta.value("text", "");
                        finalResponse.text += text;
                        onChunk(StreamChunk{text, false, std::nullopt});
                    } else if (delta["type"] == "input_json_delta") {
                        currentToolArgs += delta.value("partial_json", "");
                    }
                } else if (eventType == "content_block_stop") {
                    if (!currentToolId.empty()) {
                        ToolCall tc;
                        tc.id = currentToolId;
                        tc.name = currentToolName;
                        tc.arguments = nlohmann::json::parse(currentToolArgs, nullptr, false);
                        if (tc.arguments.is_discarded()) tc.arguments = nlohmann::json::object();
                        finalResponse.toolCalls.push_back(tc);
                        onChunk(StreamChunk{"", false, tc});
                        currentToolId.clear();
                        currentToolName.clear();
                        currentToolArgs.clear();
                    }
                } else if (eventType == "message_delta") {
                    if (j.contains("usage")) {
                        finalResponse.completionTokens = j["usage"].value("output_tokens", 0);
                    }
                } else if (eventType == "message_start") {
                    if (j.contains("message") && j["message"].contains("usage")) {
                        finalResponse.promptTokens = j["message"]["usage"].value("input_tokens", 0);
                    }
                } else if (eventType == "message_stop") {
                    onChunk(StreamChunk{"", true, std::nullopt});
                    return false;
                }
            }
            return true;
        });

    finalResponse.finishReason = finalResponse.toolCalls.empty() ? "stop" : "tool_calls";
    return finalResponse;
}

bool AnthropicProvider::ValidateKey(const std::string& apiKey) {
    auto headers = AuthHeaders(apiKey);
    auto body = nlohmann::json{
        {"model", "claude-haiku-4-5-20251001"},
        {"max_tokens", 1},
        {"messages", {{{"role", "user"}, {"content", "hi"}}}}
    };
    auto resp = HttpClient::Post("https://api.anthropic.com/v1/messages", body.dump(), headers);
    return resp.statusCode == 200;
}

} // namespace Manifold::Core
