#pragma once
#include "ProviderTypes.h"
#include <string>
#include <vector>
#include <functional>

namespace Manifold::Core {

class IProvider {
public:
    virtual ~IProvider() = default;
    virtual std::string Id() const = 0;
    virtual std::string DisplayName() const = 0;
    virtual std::vector<ModelInfo> ListModels() const = 0;
    virtual bool RequiresApiKey() const = 0;
    virtual bool SupportsStreaming() const = 0;
    virtual bool SupportsTools() const = 0;

    virtual ChatResponse SendChat(const ChatRequest& req) = 0;
    virtual ChatResponse StreamChat(const ChatRequest& req,
        std::function<void(const StreamChunk&)> onChunk) = 0;
    virtual bool ValidateKey(const std::string& apiKey) = 0;
};

} // namespace Manifold::Core
