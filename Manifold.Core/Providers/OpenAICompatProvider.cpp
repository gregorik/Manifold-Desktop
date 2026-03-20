#include "pch.h"
#include "OpenAICompatProvider.h"
#include "HttpClient.h"

namespace Manifold::Core {

OpenAICompatProvider::OpenAICompatProvider(
    const std::string& id, const std::string& displayName,
    const std::string& endpointUrl, bool requiresKey)
    : m_id(id), m_displayName(displayName),
      m_endpointUrl(endpointUrl), m_requiresKey(requiresKey) {}

std::vector<ModelInfo> OpenAICompatProvider::ListModels() const {
    // Try to fetch models from the endpoint
    auto headers = m_requiresKey ? std::map<std::string, std::string>{} : std::map<std::string, std::string>{};
    auto resp = HttpClient::Get(m_endpointUrl + "/v1/models", headers);

    std::vector<ModelInfo> models;
    if (resp.statusCode == 200) {
        auto j = nlohmann::json::parse(resp.body, nullptr, false);
        if (!j.is_discarded() && j.contains("data")) {
            for (auto& m : j["data"]) {
                models.push_back({
                    m.value("id", "unknown"),
                    m.value("id", "unknown"),
                    !m_requiresKey,  // Local models are free
                    0, ""
                });
            }
        }
    }

    if (models.empty()) {
        // Return a placeholder if we can't reach the endpoint
        models.push_back({m_id + "-default", m_displayName + " (default)", !m_requiresKey, 0, ""});
    }

    return models;
}

} // namespace Manifold::Core
