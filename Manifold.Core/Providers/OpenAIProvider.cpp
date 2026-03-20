#include "pch.h"
#include "OpenAIProvider.h"
#include "HttpClient.h"

namespace Manifold::Core {

std::vector<ModelInfo> OpenAIProvider::ListModels() const {
    return {
        {"gpt-4o", "GPT-4o", false, 0, "Most capable"},
        {"gpt-4o-mini", "GPT-4o Mini", false, 0, "Fast and affordable"},
        {"gpt-4.1", "GPT-4.1", false, 0, "Latest"},
        {"gpt-4.1-mini", "GPT-4.1 Mini", false, 0, "Latest mini"},
    };
}

std::map<std::string, std::string> OpenAIProvider::AuthHeaders(const std::string& apiKey) const {
    return {{"Authorization", "Bearer " + apiKey}};
}

nlohmann::json OpenAIProvider::BuildRequestBody(const ChatRequest& req, bool stream) const {
    nlohmann::json messages = nlohmann::json::array();

    if (!req.systemPrompt.empty()) {
        messages.push_back({{"role", "system"}, {"content", req.systemPrompt}});
    }

    for (auto& msg : req.messages) {
        nlohmann::json m;
        m["role"] = msg.role;

        if (msg.toolCall.has_value()) {
            m["role"] = "assistant";
            m["content"] = nullptr;
            m["tool_calls"] = nlohmann::json::array({
                {
                    {"id", msg.toolCall->id},
                    {"type", "function"},
                    {"function", {
                        {"name", msg.toolCall->name},
                        {"arguments", msg.toolCall->arguments.dump()}
                    }}
                }
            });
        } else if (msg.toolResult.has_value()) {
            m["role"] = "tool";
            m["tool_call_id"] = msg.toolResult->callId;
            m["content"] = msg.toolResult->content.dump();
        } else if (!msg.parts.empty()) {
            nlohmann::json content = nlohmann::json::array();
            for (auto& part : msg.parts) {
                if (part.type == ContentPart::Type::Text && part.text.has_value()) {
                    content.push_back({{"type", "text"}, {"text", part.text->text}});
                } else if (part.type == ContentPart::Type::Image && part.image.has_value()) {
                    content.push_back({{"type", "image_url"}, {"image_url", {
                        {"url", "data:" + part.image->mimeType + ";base64," + part.image->base64Data}
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
        {"temperature", req.temperature},
        {"stream", stream}
    };

    if (!req.tools.empty()) {
        nlohmann::json tools = nlohmann::json::array();
        for (auto& tool : req.tools) {
            tools.push_back({
                {"type", "function"},
                {"function", tool.ToFunctionSchema()}
            });
        }
        body["tools"] = tools;
    }

    return body;
}

ChatResponse OpenAIProvider::ParseResponse(const nlohmann::json& j) const {
    ChatResponse response;

    if (j.contains("choices") && !j["choices"].empty()) {
        auto& choice = j["choices"][0];
        auto& message = choice["message"];

        response.text = message.value("content", "");

        if (message.contains("tool_calls")) {
            for (auto& tc : message["tool_calls"]) {
                ToolCall call;
                call.id = tc.value("id", "");
                call.name = tc["function"].value("name", "");
                auto argsStr = tc["function"].value("arguments", "");
                call.arguments = nlohmann::json::parse(argsStr, nullptr, false);
                if (call.arguments.is_discarded()) call.arguments = nlohmann::json::object();
                response.toolCalls.push_back(std::move(call));
            }
        }

        auto finish = choice.value("finish_reason", "stop");
        response.finishReason = (finish == "tool_calls") ? "tool_calls" : "stop";
    }

    if (j.contains("usage")) {
        response.promptTokens = j["usage"].value("prompt_tokens", 0);
        response.completionTokens = j["usage"].value("completion_tokens", 0);
    }

    return response;
}

ChatResponse OpenAIProvider::SendChat(const ChatRequest& req) {
    auto body = BuildRequestBody(req, false);
    auto url = BaseUrl() + "/v1/chat/completions";
    auto headers = AuthHeaders(req.apiKey);

    auto httpResp = HttpClient::Post(url, body.dump(), headers);
    if (httpResp.statusCode != 200) {
        return ChatResponse{ "", {}, 0, 0, "error" };
    }

    auto j = nlohmann::json::parse(httpResp.body, nullptr, false);
    if (j.is_discarded()) return ChatResponse{ "", {}, 0, 0, "error" };

    return ParseResponse(j);
}

ChatResponse OpenAIProvider::StreamChat(const ChatRequest& req,
    std::function<void(const StreamChunk&)> onChunk) {

    auto body = BuildRequestBody(req, true);
    auto url = BaseUrl() + "/v1/chat/completions";
    auto headers = AuthHeaders(req.apiKey);

    ChatResponse finalResponse;
    std::string sseBuffer;
    // Accumulate partial tool calls by index
    std::map<int, ToolCall> pendingToolCalls;

    HttpClient::StreamPost(url, body.dump(), headers,
        [&](const std::string& chunk) -> bool {
            sseBuffer += chunk;

            size_t pos = 0;
            while ((pos = sseBuffer.find("\n")) != std::string::npos) {
                auto line = sseBuffer.substr(0, pos);
                sseBuffer = sseBuffer.substr(pos + 1);

                if (line.substr(0, 6) != "data: ") continue;
                auto data = line.substr(6);
                if (data == "[DONE]") {
                    onChunk(StreamChunk{"", true, std::nullopt});
                    return false;
                }

                auto j = nlohmann::json::parse(data, nullptr, false);
                if (j.is_discarded()) continue;

                if (j.contains("choices") && !j["choices"].empty()) {
                    auto& delta = j["choices"][0]["delta"];

                    if (delta.contains("content") && !delta["content"].is_null()) {
                        auto text = delta["content"].get<std::string>();
                        finalResponse.text += text;
                        onChunk(StreamChunk{text, false, std::nullopt});
                    }

                    if (delta.contains("tool_calls")) {
                        for (auto& tc : delta["tool_calls"]) {
                            int idx = tc.value("index", 0);
                            if (tc.contains("id")) {
                                pendingToolCalls[idx].id = tc["id"].get<std::string>();
                            }
                            if (tc.contains("function")) {
                                if (tc["function"].contains("name")) {
                                    pendingToolCalls[idx].name = tc["function"]["name"].get<std::string>();
                                }
                                if (tc["function"].contains("arguments")) {
                                    // Arguments come in fragments
                                    auto& existing = pendingToolCalls[idx].arguments;
                                    if (existing.is_null()) existing = "";
                                    existing = existing.get<std::string>() + tc["function"]["arguments"].get<std::string>();
                                }
                            }
                        }
                    }
                }

                if (j.contains("usage")) {
                    finalResponse.promptTokens = j["usage"].value("prompt_tokens", 0);
                    finalResponse.completionTokens = j["usage"].value("completion_tokens", 0);
                }
            }
            return true;
        });

    // Finalize pending tool calls
    for (auto& [idx, tc] : pendingToolCalls) {
        if (tc.arguments.is_string()) {
            auto parsed = nlohmann::json::parse(tc.arguments.get<std::string>(), nullptr, false);
            tc.arguments = parsed.is_discarded() ? nlohmann::json::object() : parsed;
        }
        finalResponse.toolCalls.push_back(std::move(tc));
        onChunk(StreamChunk{"", false, finalResponse.toolCalls.back()});
    }

    finalResponse.finishReason = finalResponse.toolCalls.empty() ? "stop" : "tool_calls";
    return finalResponse;
}

bool OpenAIProvider::ValidateKey(const std::string& apiKey) {
    auto url = BaseUrl() + "/v1/models";
    auto resp = HttpClient::Get(url, AuthHeaders(apiKey));
    return resp.statusCode == 200;
}

} // namespace Manifold::Core
