#pragma once
#include "OpenAICompatProvider.h"

namespace Manifold::Core {

class ProxyProvider : public OpenAICompatProvider {
public:
    ProxyProvider(const std::string& proxyUrl, const std::string& deviceId);

    std::string Id() const override { return "proxy"; }
    std::string DisplayName() const override { return "Manifold Free Tier"; }
    bool RequiresApiKey() const override { return false; }

    std::vector<ModelInfo> ListModels() const override;

protected:
    std::map<std::string, std::string> ExtraHeaders() const;

private:
    std::string m_deviceId;
};

} // namespace Manifold::Core
