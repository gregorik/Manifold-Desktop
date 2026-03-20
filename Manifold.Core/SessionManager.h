#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "json.hpp"

namespace Manifold::Core
{
    struct SessionInfo
    {
        std::string id;
        std::string title;
        std::string model;
        std::string createdAt;
        std::string updatedAt;
    };

    struct SearchResult
    {
        std::string sessionId;
        std::string title;
        std::string snippet;
    };

    class SessionManager
    {
    public:
        SessionManager();

        void SaveSession(const std::string& id, const nlohmann::json& data);
        nlohmann::json LoadSession(const std::string& id);
        bool DeleteSession(const std::string& id);
        std::vector<SessionInfo> ListSessions();
        std::vector<SearchResult> SearchSessions(const std::string& query);

    private:
        std::filesystem::path m_sessionsDir;
        void EnsureDirectory();
        std::filesystem::path SessionPath(const std::string& id);
    };
}
