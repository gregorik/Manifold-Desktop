#include "pch.h"
#include "PromptManager.h"
#include <fstream>
#include <ShlObj.h>

namespace Manifold::Core
{
    PromptManager::PromptManager()
    {
        PWSTR appData = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData)))
        {
            m_promptsDir = std::filesystem::path(appData) / L"Manifold" / L"prompts";
            CoTaskMemFree(appData);
        }
    }

    void PromptManager::EnsureDirectory()
    {
        std::filesystem::create_directories(m_promptsDir);
    }

    std::filesystem::path PromptManager::PromptPath(const std::string& id)
    {
        return m_promptsDir / (id + ".json");
    }

    nlohmann::json PromptManager::ListPrompts()
    {
        nlohmann::json arr = nlohmann::json::array();
        if (!std::filesystem::exists(m_promptsDir)) return arr;

        for (auto& entry : std::filesystem::directory_iterator(m_promptsDir)) {
            if (entry.path().extension() != ".json") continue;
            std::ifstream f(entry.path());
            if (!f.is_open()) continue;
            try {
                auto j = nlohmann::json::parse(f);
                arr.push_back(j);
            } catch (...) {}
        }
        return arr;
    }

    void PromptManager::SavePrompt(const std::string& id, const nlohmann::json& data)
    {
        EnsureDirectory();
        std::ofstream f(PromptPath(id));
        if (f.is_open()) f << data.dump(2);
    }

    bool PromptManager::DeletePrompt(const std::string& id)
    {
        auto path = PromptPath(id);
        if (std::filesystem::exists(path))
            return std::filesystem::remove(path);
        return false;
    }
}
