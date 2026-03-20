#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "json.hpp"

namespace Manifold::Core
{
    class PromptManager
    {
    public:
        PromptManager();

        nlohmann::json ListPrompts();
        void SavePrompt(const std::string& id, const nlohmann::json& data);
        bool DeletePrompt(const std::string& id);

    private:
        std::filesystem::path m_promptsDir;
        void EnsureDirectory();
        std::filesystem::path PromptPath(const std::string& id);
    };
}
