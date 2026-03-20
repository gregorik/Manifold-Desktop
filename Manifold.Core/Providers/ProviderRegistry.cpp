#include "pch.h"
#include "ProviderRegistry.h"

namespace Manifold::Core {

void ProviderRegistry::AddProvider(std::unique_ptr<IProvider> provider) {
    auto id = provider->Id();
    m_providers[id] = std::move(provider);
}

IProvider* ProviderRegistry::GetProvider(const std::string& id) const {
    auto it = m_providers.find(id);
    return (it != m_providers.end()) ? it->second.get() : nullptr;
}

std::vector<IProvider*> ProviderRegistry::ListProviders() const {
    std::vector<IProvider*> result;
    for (auto& [id, p] : m_providers) {
        result.push_back(p.get());
    }
    return result;
}

std::vector<ModelInfo> ProviderRegistry::ListAllModels() const {
    std::vector<ModelInfo> result;
    for (auto& [id, p] : m_providers) {
        auto models = p->ListModels();
        result.insert(result.end(), models.begin(), models.end());
    }
    return result;
}

} // namespace Manifold::Core
