#pragma once
#include "IProvider.h"

namespace Manifold::Core {

class GeminiProvider : public IProvider {
public:
    std::string Id() const override { return "gemini"; }
    std::string DisplayName() const override { return "Google Gemini"; }
    std::vector<ModelInfo> ListModels() const override;
    bool RequiresApiKey() const override { return true; }
    bool SupportsStreaming() const override { return true; }
    bool SupportsTools() const override { return true; }

    ChatResponse SendChat(const ChatRequest& req) override;
    ChatResponse StreamChat(const ChatRequest& req,
        std::function<void(const StreamChunk&)> onChunk) override;
    bool ValidateKey(const std::string& apiKey) override;

private:
    nlohmann::json BuildRequestBody(const ChatRequest& req) const;
    ChatResponse ParseResponse(const nlohmann::json& j) const;
};

} // namespace Manifold::Core
