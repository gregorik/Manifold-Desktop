#include "pch.h"
#include "ProxyProvider.h"
#include "HttpClient.h"

namespace Manifold::Core {

ProxyProvider::ProxyProvider(const std::string& proxyUrl, const std::string& deviceId)
    : OpenAICompatProvider("proxy", "Manifold Free Tier", proxyUrl, false)
    , m_deviceId(deviceId)
{
}

std::vector<ModelInfo> ProxyProvider::ListModels() const {
    try {
        auto resp = HttpClient::Get(BaseUrl() + "/v1/models", ExtraHeaders());
        if (resp.statusCode == 200) {
            auto j = nlohmann::json::parse(resp.body, nullptr, false);
            if (!j.is_discarded() && j.contains("data")) {
                std::vector<ModelInfo> models;
                for (auto& m : j["data"]) {
                    ModelInfo info;
                    info.id = m.value("id", "");
                    info.displayName = m.value("displayName", info.id);
                    info.isFree = true;
                    models.push_back(info);
                }
                return models;
            }
        }
    } catch (...) {}

    // Fallback
    return {
        {"gemini-2.0-flash", "Gemini 2.0 Flash (Free)", true, 10, "Via proxy"},
        {"gemini-2.0-flash-lite", "Gemini 2.0 Flash Lite (Free)", true, 30, "Via proxy"},
        {"gemini-1.5-flash", "Gemini 1.5 Flash (Free)", true, 15, "Via proxy"}
    };
}

std::map<std::string, std::string> ProxyProvider::ExtraHeaders() const {
    return {{"X-Manifold-Device-ID", m_deviceId}};
}

} // namespace Manifold::Core
