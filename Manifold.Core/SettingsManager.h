#pragma once
#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include "json.hpp"

namespace Manifold::Core
{
    struct ProviderConfig {
        std::string endpointUrl;
        bool enabled = true;
    };

    struct McpServerConfig {
        std::string transport;  // "stdio" or "sse"
        std::string command;
        std::vector<std::string> args;
        std::string url;
        bool enabled = true;
    };

    struct AppSettings
    {
        // Existing (migrated wstring -> string)
        std::string model{ "gemini-2.0-flash" };
        double temperature{ 0.7 };
        std::string theme{ "dark" };
        int fontSize{ 14 };
        std::string systemPrompt;
        bool streamResponses{ true };

        // New — providers
        std::string activeProviderId{ "gemini" };
        std::map<std::string, ProviderConfig> providerConfigs;

        // New — plugins
        std::map<std::string, bool> pluginEnabled;

        // New — MCP
        std::map<std::string, McpServerConfig> mcpServers;
        bool mcpServerEnabled{ false };
        int mcpServerPort{ 9339 };

        // Ollama
        std::string ollamaEndpoint{ "http://localhost:11434" };
    };

    // nlohmann ADL serialization
    void to_json(nlohmann::json& j, const ProviderConfig& c);
    void from_json(const nlohmann::json& j, ProviderConfig& c);
    void to_json(nlohmann::json& j, const McpServerConfig& c);
    void from_json(const nlohmann::json& j, McpServerConfig& c);
    void to_json(nlohmann::json& j, const AppSettings& s);
    void from_json(const nlohmann::json& j, AppSettings& s);

    class SettingsManager
    {
    public:
        SettingsManager();

        AppSettings Load();
        void Save(const AppSettings& settings);

        nlohmann::json ToJson(const AppSettings& settings);
        AppSettings FromJson(const nlohmann::json& j);

    private:
        std::filesystem::path m_settingsPath;
        void EnsureDirectory();
    };
}
