#include "pch.h"
#include "GeminiProvider.h"
#include "HttpClient.h"

namespace Manifold::Core {

std::vector<ModelInfo> GeminiProvider::ListModels() const {
    return {
        {"gemini-2.0-flash", "Gemini 2.0 Flash", true, 15, "Free with rate limits"},
        {"gemini-2.0-flash-lite", "Gemini 2.0 Flash Lite", true, 30, "Free, fastest"},
        {"gemini-2.5-flash-preview-05-20", "Gemini 2.5 Flash Preview", true, 10, "Free preview"},
        {"gemini-2.5-pro-preview-05-06", "Gemini 2.5 Pro Preview", true, 5, "Free preview, limited"},
    };
}

nlohmann::json GeminiProvider::BuildRequestBody(const ChatRequest& req) const {
    nlohmann::json contents = nlohmann::json::array();

    for (auto& msg : req.messages) {
        if (msg.role == "system") continue;

        nlohmann::json parts = nlohmann::json::array();

        // Handle tool results
        if (msg.toolResult.has_value()) {
            parts.push_back({
                {"functionResponse", {
                    {"name", msg.toolResult->name},
                    {"response", msg.toolResult->content}
                }}
            });
        }
        // Handle tool calls
        else if (msg.toolCall.has_value()) {
            parts.push_back({
                {"functionCall", {
                    {"name", msg.toolCall->name},
                    {"args", msg.toolCall->arguments}
                }}
            });
        }
        // Handle content parts
        else if (!msg.parts.empty()) {
            for (auto& part : msg.parts) {
                if (part.type == ContentPart::Type::Text && part.text.has_value()) {
                    parts.push_back({{"text", part.text->text}});
                } else if (part.type == ContentPart::Type::Image && part.image.has_value()) {
                    parts.push_back({{"inlineData", {
                        {"mimeType", part.image->mimeType},
                        {"data", part.image->base64Data}
                    }}});
                }
            }
        }
        // Plain text
        else if (!msg.text.empty()) {
            parts.push_back({{"text", msg.text}});
        }

        std::string role = (msg.role == "assistant") ? "model" : "user";
        contents.push_back({{"role", role}, {"parts", parts}});
    }

    nlohmann::json body = {{"contents", contents}};

    // System instruction
    if (!req.systemPrompt.empty()) {
        body["systemInstruction"] = {
            {"parts", {{{"text", req.systemPrompt}}}}
        };
    }

    // Generation config
    body["generationConfig"] = {
        {"temperature", req.temperature}
    };

    // Tools
    if (!req.tools.empty()) {
        nlohmann::json funcs = nlohmann::json::array();
        for (auto& tool : req.tools) {
            funcs.push_back(tool.ToFunctionSchema());
        }
        body["tools"] = {{{"functionDeclarations", funcs}}};
    }

    return body;
}

ChatResponse GeminiProvider::ParseResponse(const nlohmann::json& j) const {
    ChatResponse response;

    if (j.contains("candidates") && !j["candidates"].empty()) {
        auto& candidate = j["candidates"][0];
        if (candidate.contains("content") && candidate["content"].contains("parts")) {
            for (auto& part : candidate["content"]["parts"]) {
                if (part.contains("text")) {
                    response.text += part["text"].get<std::string>();
                } else if (part.contains("functionCall")) {
                    ToolCall tc;
                    tc.name = part["functionCall"].value("name", "");
                    tc.arguments = part["functionCall"].value("args", nlohmann::json::object());
                    tc.id = tc.name + "_" + std::to_string(response.toolCalls.size());
                    response.toolCalls.push_back(std::move(tc));
                }
            }
        }
        response.finishReason = candidate.value("finishReason", "STOP");
    }

    if (j.contains("usageMetadata")) {
        response.promptTokens = j["usageMetadata"].value("promptTokenCount", 0);
        response.completionTokens = j["usageMetadata"].value("candidatesTokenCount", 0);
    }

    if (!response.toolCalls.empty()) {
        response.finishReason = "tool_calls";
    } else {
        response.finishReason = "stop";
    }

    return response;
}

ChatResponse GeminiProvider::SendChat(const ChatRequest& req) {
    auto body = BuildRequestBody(req);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/"
        + req.modelId + ":generateContent?key=" + req.apiKey;

    auto httpResp = HttpClient::Post(url, body.dump());
    if (httpResp.statusCode != 200) {
        return ChatResponse{ "", {}, 0, 0, "error" };
    }

    auto j = nlohmann::json::parse(httpResp.body, nullptr, false);
    if (j.is_discarded()) return ChatResponse{ "", {}, 0, 0, "error" };

    return ParseResponse(j);
}

ChatResponse GeminiProvider::StreamChat(const ChatRequest& req,
    std::function<void(const StreamChunk&)> onChunk) {

    auto body = BuildRequestBody(req);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/"
        + req.modelId + ":streamGenerateContent?key=" + req.apiKey + "&alt=sse";

    ChatResponse finalResponse;
    std::string sseBuffer;

    HttpClient::StreamPost(url, body.dump(),
        {{"Accept", "text/event-stream"}},
        [&](const std::string& chunk) -> bool {
            sseBuffer += chunk;

            // Process SSE lines
            size_t pos = 0;
            while ((pos = sseBuffer.find("\n")) != std::string::npos) {
                auto line = sseBuffer.substr(0, pos);
                sseBuffer = sseBuffer.substr(pos + 1);

                if (line.substr(0, 6) == "data: ") {
                    auto data = line.substr(6);
                    auto j = nlohmann::json::parse(data, nullptr, false);
                    if (j.is_discarded()) continue;

                    auto partial = ParseResponse(j);
                    if (!partial.text.empty()) {
                        finalResponse.text += partial.text;
                        onChunk(StreamChunk{partial.text, false, std::nullopt});
                    }
                    for (auto& tc : partial.toolCalls) {
                        finalResponse.toolCalls.push_back(tc);
                        onChunk(StreamChunk{"", false, tc});
                    }
                    finalResponse.promptTokens = partial.promptTokens > 0
                        ? partial.promptTokens : finalResponse.promptTokens;
                    finalResponse.completionTokens = partial.completionTokens > 0
                        ? partial.completionTokens : finalResponse.completionTokens;
                }
            }
            return true;
        });

    finalResponse.finishReason = finalResponse.toolCalls.empty() ? "stop" : "tool_calls";
    onChunk(StreamChunk{"", true, std::nullopt});
    return finalResponse;
}

bool GeminiProvider::ValidateKey(const std::string& apiKey) {
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models?key=" + apiKey;
    auto resp = HttpClient::Get(url);
    return resp.statusCode == 200;
}

} // namespace Manifold::Core
