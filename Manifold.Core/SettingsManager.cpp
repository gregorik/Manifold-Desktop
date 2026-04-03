#include "pch.h"
#include "SettingsManager.h"
#include <fstream>
#include <ShlObj.h>
#include <combaseapi.h>

namespace Manifold::Core
{
    // --- ADL serialization ---

    void to_json(nlohmann::json& j, const ProviderConfig& c) {
        j = nlohmann::json{{"endpointUrl", c.endpointUrl}, {"enabled", c.enabled}};
    }
    void from_json(const nlohmann::json& j, ProviderConfig& c) {
        c.endpointUrl = j.value("endpointUrl", "");
        c.enabled = j.value("enabled", true);
    }

    void to_json(nlohmann::json& j, const McpServerConfig& c) {
        j = nlohmann::json{
            {"transport", c.transport}, {"command", c.command},
            {"args", c.args}, {"url", c.url}, {"enabled", c.enabled}
        };
    }
    void from_json(const nlohmann::json& j, McpServerConfig& c) {
        c.transport = j.value("transport", "stdio");
        c.command = j.value("command", "");
        if (j.contains("args") && j["args"].is_array())
            c.args = j["args"].get<std::vector<std::string>>();
        c.url = j.value("url", "");
        c.enabled = j.value("enabled", true);
    }

    void to_json(nlohmann::json& j, const AppSettings& s) {
        j = nlohmann::json{
            {"model", s.model},
            {"temperature", s.temperature},
            {"theme", s.theme},
            {"fontSize", s.fontSize},
            {"systemPrompt", s.systemPrompt},
            {"streamResponses", s.streamResponses},
            {"activeProviderId", s.activeProviderId},
            {"providerConfigs", s.providerConfigs},
            {"pluginEnabled", s.pluginEnabled},
            {"mcpServers", s.mcpServers},
            {"mcpServerEnabled", s.mcpServerEnabled},
            {"mcpServerPort", s.mcpServerPort},
            {"proxyUrl", s.proxyUrl},
            {"deviceId", s.deviceId},
            {"ollamaEndpoint", s.ollamaEndpoint}
        };
    }

    void from_json(const nlohmann::json& j, AppSettings& s) {
        s.model = j.value("model", "gemini-2.0-flash");
        s.temperature = j.value("temperature", 0.7);
        s.theme = j.value("theme", "dark");
        s.fontSize = j.value("fontSize", 14);
        s.systemPrompt = j.value("systemPrompt", "");
        s.streamResponses = j.value("streamResponses", true);
        s.activeProviderId = j.value("activeProviderId", "gemini");
        if (j.contains("providerConfigs"))
            s.providerConfigs = j["providerConfigs"].get<std::map<std::string, ProviderConfig>>();
        if (j.contains("pluginEnabled"))
            s.pluginEnabled = j["pluginEnabled"].get<std::map<std::string, bool>>();
        if (j.contains("mcpServers"))
            s.mcpServers = j["mcpServers"].get<std::map<std::string, McpServerConfig>>();
        s.mcpServerEnabled = j.value("mcpServerEnabled", false);
        s.mcpServerPort = j.value("mcpServerPort", 9339);
        s.proxyUrl = j.value("proxyUrl", "https://manifold-proxy.example.com");
        s.deviceId = j.value("deviceId", "");
        s.ollamaEndpoint = j.value("ollamaEndpoint", "http://localhost:11434");
    }

    // --- SettingsManager ---

    SettingsManager::SettingsManager()
    {
        PWSTR localAppDataPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataPath)))
        {
            m_settingsPath = std::filesystem::path(localAppDataPath) / L"Manifold" / L"settings.json";
            CoTaskMemFree(localAppDataPath);
        }
    }

    void SettingsManager::EnsureDirectory()
    {
        auto dir = m_settingsPath.parent_path();
        std::filesystem::create_directories(dir);
    }

    std::string SettingsManager::GenerateUUID()
    {
        GUID guid;
        CoCreateGuid(&guid);
        char buf[64];
        snprintf(buf, sizeof(buf),
            "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return buf;
    }

    AppSettings SettingsManager::Load()
    {
        AppSettings settings;
        if (m_settingsPath.empty() || !std::filesystem::exists(m_settingsPath))
        {
            // Generate device ID on first launch
            settings.deviceId = GenerateUUID();
            Save(settings);
            return settings;
        }

        std::ifstream file(m_settingsPath);
        if (!file.is_open()) return settings;

        try {
            auto j = nlohmann::json::parse(file);
            settings = j.get<AppSettings>();
        } catch (...) {
            // Corrupt file — use defaults
        }

        // Ensure device ID exists
        if (settings.deviceId.empty()) {
            settings.deviceId = GenerateUUID();
            Save(settings);
        }

        return settings;
    }

    void SettingsManager::Save(const AppSettings& settings)
    {
        if (m_settingsPath.empty()) return;
        EnsureDirectory();

        std::ofstream file(m_settingsPath);
        if (!file.is_open()) return;
        file << nlohmann::json(settings).dump(2);
    }

    nlohmann::json SettingsManager::ToJson(const AppSettings& settings)
    {
        return nlohmann::json(settings);
    }

    AppSettings SettingsManager::FromJson(const nlohmann::json& j)
    {
        return j.get<AppSettings>();
    }
}
