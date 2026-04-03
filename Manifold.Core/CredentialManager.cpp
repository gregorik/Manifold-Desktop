#include "pch.h"
#include "CredentialManager.h"
#include <wincred.h>

#pragma comment(lib, "Advapi32.lib")

namespace Manifold::Core
{
    bool CredentialManager::SaveApiKey(const std::wstring& apiKey)
    {
        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = const_cast<LPWSTR>(kCredentialTarget);
        cred.CredentialBlobSize = static_cast<DWORD>(apiKey.size() * sizeof(wchar_t));
        cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(apiKey.data()));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
        cred.UserName = const_cast<LPWSTR>(L"Manifold");

        return CredWriteW(&cred, 0) == TRUE;
    }

    std::wstring CredentialManager::LoadApiKey()
    {
        PCREDENTIALW pCred = nullptr;
        if (!CredReadW(kCredentialTarget, CRED_TYPE_GENERIC, 0, &pCred))
            return {};

        std::wstring apiKey;
        if (pCred->CredentialBlob && pCred->CredentialBlobSize > 0)
        {
            apiKey.assign(
                reinterpret_cast<wchar_t*>(pCred->CredentialBlob),
                pCred->CredentialBlobSize / sizeof(wchar_t)
            );
        }
        CredFree(pCred);
        return apiKey;
    }

    bool CredentialManager::DeleteApiKey()
    {
        return CredDeleteW(kCredentialTarget, CRED_TYPE_GENERIC, 0) == TRUE;
    }

    // Per-provider helpers and overloads

    std::wstring CredentialManager::MakeTarget(const std::wstring& providerId)
    {
        return L"Manifold_" + providerId;
    }

    bool CredentialManager::SaveApiKey(const std::wstring& providerId, const std::wstring& apiKey)
    {
        const std::wstring target = MakeTarget(providerId);
        CREDENTIALW cred = {};
        cred.Type = CRED_TYPE_GENERIC;
        cred.TargetName = const_cast<LPWSTR>(target.c_str());
        cred.CredentialBlobSize = static_cast<DWORD>(apiKey.size() * sizeof(wchar_t));
        cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(apiKey.data()));
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
        cred.UserName = const_cast<LPWSTR>(L"Manifold");
        return CredWriteW(&cred, 0) == TRUE;
    }

    std::wstring CredentialManager::LoadApiKey(const std::wstring& providerId)
    {
        const std::wstring target = MakeTarget(providerId);
        PCREDENTIALW pCred = nullptr;
        if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &pCred))
            return {};

        std::wstring apiKey;
        if (pCred->CredentialBlob && pCred->CredentialBlobSize > 0)
        {
            apiKey.assign(
                reinterpret_cast<wchar_t*>(pCred->CredentialBlob),
                pCred->CredentialBlobSize / sizeof(wchar_t)
            );
        }
        CredFree(pCred);
        return apiKey;
    }

    bool CredentialManager::DeleteApiKey(const std::wstring& providerId)
    {
        const std::wstring target = MakeTarget(providerId);
        return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) == TRUE;
    }
}
