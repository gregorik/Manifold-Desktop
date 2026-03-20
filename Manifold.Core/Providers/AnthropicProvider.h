#pragma once
#include "IProvider.h"

namespace Manifold::Core {

class AnthropicProvider : public IProvider {
public:
    std::string Id() const override { return "anthropic"; }
    std::string DisplayName() const override { return "Anthropic"; }
    std::vector<ModelInfo> ListModels() const override;
    bool RequiresApiKey() const override { return true; }
    bool SupportsStreaming() const override { return true; }
    bool SupportsTools() const override { return true; }

    ChatResponse SendChat(const ChatRequest& req) override;
    ChatResponse StreamChat(const ChatRequest& req,
        std::function<void(const StreamChunk&)> onChunk) override;
    bool ValidateKey(const std::string& apiKey) override;

private:
    nlohmann::json BuildRequestBody(const ChatRequest& req, bool stream) const;
    std::map<std::string, std::string> AuthHeaders(const std::string& apiKey) const;
};

} // namespace Manifold::Core
