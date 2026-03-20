#pragma once
#include "OpenAIProvider.h"

namespace Manifold::Core {

class OpenAICompatProvider : public OpenAIProvider {
public:
    OpenAICompatProvider(const std::string& id, const std::string& displayName,
                         const std::string& endpointUrl, bool requiresKey = true);

    std::string Id() const override { return m_id; }
    std::string DisplayName() const override { return m_displayName; }
    std::vector<ModelInfo> ListModels() const override;
    bool RequiresApiKey() const override { return m_requiresKey; }

protected:
    std::string BaseUrl() const override { return m_endpointUrl; }

private:
    std::string m_id;
    std::string m_displayName;
    std::string m_endpointUrl;
    bool m_requiresKey;
};

} // namespace Manifold::Core
