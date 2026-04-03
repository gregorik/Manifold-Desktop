#include "pch.h"
#include "SessionManager.h"
#include <fstream>
#include <algorithm>
#include <ShlObj.h>

namespace Manifold::Core
{
    SessionManager::SessionManager()
    {
        PWSTR localAppDataPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataPath)))
        {
            m_sessionsDir = std::filesystem::path(localAppDataPath) / L"Manifold" / L"sessions";
            CoTaskMemFree(localAppDataPath);
        }
    }

    void SessionManager::EnsureDirectory()
    {
        std::filesystem::create_directories(m_sessionsDir);
    }

    std::filesystem::path SessionManager::SessionPath(const std::string& id)
    {
        return m_sessionsDir / (id + ".json");
    }

    void SessionManager::SaveSession(const std::string& id, const nlohmann::json& data)
    {
        if (m_sessionsDir.empty()) return;
        EnsureDirectory();
        std::ofstream file(SessionPath(id));
        if (file.is_open())
            file << data.dump(2);
    }

    nlohmann::json SessionManager::LoadSession(const std::string& id)
    {
        auto path = SessionPath(id);
        if (!std::filesystem::exists(path)) return nullptr;

        std::ifstream file(path);
        if (!file.is_open()) return nullptr;

        try {
            return nlohmann::json::parse(file);
        } catch (...) {
            return nullptr;
        }
    }

    bool SessionManager::DeleteSession(const std::string& id)
    {
        auto path = SessionPath(id);
        if (std::filesystem::exists(path))
            return std::filesystem::remove(path);
        return false;
    }

    std::vector<SessionInfo> SessionManager::ListSessions()
    {
        std::vector<SessionInfo> sessions;
        if (m_sessionsDir.empty() || !std::filesystem::exists(m_sessionsDir))
            return sessions;

        for (auto& entry : std::filesystem::directory_iterator(m_sessionsDir))
        {
            if (entry.path().extension() != ".json") continue;

            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            try {
                auto j = nlohmann::json::parse(file);
                SessionInfo info;
                info.id = entry.path().stem().string();
                info.title = j.value("title", "Untitled");
                info.model = j.value("model", "");
                info.createdAt = j.value("createdAt", "");
                info.updatedAt = j.value("updatedAt", "");
                sessions.push_back(std::move(info));
            } catch (...) {
                continue;
            }
        }

        // Sort by updatedAt descending
        std::sort(sessions.begin(), sessions.end(),
            [](const SessionInfo& a, const SessionInfo& b) {
                return a.updatedAt > b.updatedAt;
            });

        return sessions;
    }

    std::vector<SearchResult> SessionManager::SearchSessions(const std::string& query)
    {
        std::vector<SearchResult> results;
        if (query.empty()) return results;

        auto lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        auto sessions = ListSessions();
        for (auto& info : sessions)
        {
            auto data = LoadSession(info.id);
            if (data.is_null()) continue;

            auto dump = data.dump();
            auto lowerDump = dump;
            std::transform(lowerDump.begin(), lowerDump.end(), lowerDump.begin(), ::tolower);

            auto pos = lowerDump.find(lowerQuery);
            if (pos != std::string::npos)
            {
                SearchResult result;
                result.sessionId = info.id;
                result.title = info.title;

                // Extract snippet around match
                size_t start = (pos > 40) ? pos - 40 : 0;
                size_t len = std::min<size_t>(100, dump.size() - start);
                result.snippet = dump.substr(start, len);

                results.push_back(std::move(result));
            }
        }
        return results;
    }
}
