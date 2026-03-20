#pragma once
#include <string>

namespace Manifold::Core
{
    class CredentialManager
    {
    public:
        // Legacy single-key methods (Gemini, backward compatibility)
        static bool SaveApiKey(const std::wstring& apiKey);
        static std::wstring LoadApiKey();
        static bool DeleteApiKey();

        // Per-provider methods
        static bool SaveApiKey(const std::wstring& providerId, const std::wstring& apiKey);
        static std::wstring LoadApiKey(const std::wstring& providerId);
        static bool DeleteApiKey(const std::wstring& providerId);

    private:
        static constexpr wchar_t kCredentialTarget[] = L"Manifold_Gemini_API_Key";
        static std::wstring MakeTarget(const std::wstring& providerId);
    };
}
