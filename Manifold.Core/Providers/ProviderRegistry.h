#pragma once
#include "IProvider.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Manifold::Core {

class ProviderRegistry {
public:
    void AddProvider(std::unique_ptr<IProvider> provider);
    IProvider* GetProvider(const std::string& id) const;
    std::vector<IProvider*> ListProviders() const;
    std::vector<ModelInfo> ListAllModels() const;

private:
    std::map<std::string, std::unique_ptr<IProvider>> m_providers;
};

} // namespace Manifold::Core
