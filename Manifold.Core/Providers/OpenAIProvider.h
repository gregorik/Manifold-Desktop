#pragma once
#include "IProvider.h"

namespace Manifold::Core {

class OpenAIProvider : public IProvider {
public:
    std::string Id() const override { return "openai"; }
    std::string DisplayName() const override { return "OpenAI"; }
    std::vector<ModelInfo> ListModels() const override;
    bool RequiresApiKey() const override { return true; }
    bool SupportsStreaming() const override { return true; }
    bool SupportsTools() const override { return true; }

    ChatResponse SendChat(const ChatRequest& req) override;
    ChatResponse StreamChat(const ChatRequest& req,
        std::function<void(const StreamChunk&)> onChunk) override;
    bool ValidateKey(const std::string& apiKey) override;

protected:
    virtual std::string BaseUrl() const { return "https://api.openai.com"; }
    nlohmann::json BuildRequestBody(const ChatRequest& req, bool stream) const;
    ChatResponse ParseResponse(const nlohmann::json& j) const;
    std::map<std::string, std::string> AuthHeaders(const std::string& apiKey) const;
};

} // namespace Manifold::Core
