#include "pch.h"
#include "PluginManager.h"
#include "../json.hpp"
#include "../StringUtils.h"
#include <filesystem>
#include <fstream>
#include <shlobj.h>

namespace Manifold::Core::Plugins {

PluginManager::PluginManager() {
    ScanPlugins();
}

PluginManager::~PluginManager() {
    ShutdownAll();
}

std::wstring PluginManager::GetPluginsDir() const {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        std::wstring dir = std::wstring(path) + L"\\Manifold\\plugins";
        CoTaskMemFree(path);
        return dir;
    }
    return L"";
}

void PluginManager::ScanPlugins() {
    m_discovered.clear();
    auto dir = GetPluginsDir();
    if (dir.empty()) return;

    namespace fs = std::filesystem;
    if (!fs::exists(dir)) return;

    for (auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_directory()) continue;

        auto manifestPath = entry.path() / "plugin.json";
        if (!fs::exists(manifestPath)) continue;

        try {
            std::ifstream f(manifestPath);
            auto j = nlohmann::json::parse(f);

            PluginManifest manifest;
            manifest.id = j.value("id", entry.path().filename().string());
            manifest.name = j.value("name", manifest.id);
            manifest.version = j.value("version", "0.0.0");
            manifest.author = j.value("author", "");
            manifest.description = j.value("description", "");
            manifest.dllName = j.value("dll", "plugin.dll");
            manifest.frontendDir = j.value("frontend", "");
            manifest.virtualHost = j.value("virtualHost", manifest.id + ".manifold.local");

            PluginInfo info;
            info.manifest = std::move(manifest);
            info.state = PluginState::Discovered;
            m_discovered.push_back(std::move(info));
        } catch (...) {
            // Skip malformed manifests
        }
    }
}

void PluginManager::LoadEnabled(const std::map<std::string, bool>& enabledMap) {
    auto dir = GetPluginsDir();
    if (dir.empty()) return;

    for (auto& info : m_discovered) {
        auto it = enabledMap.find(info.manifest.id);
        if (it == enabledMap.end() || !it->second) {
            info.state = PluginState::Disabled;
            continue;
        }

        auto dllPath = std::filesystem::path(dir) / info.manifest.id / info.manifest.dllName;
        if (!std::filesystem::exists(dllPath)) {
            info.state = PluginState::Error;
            info.errorMessage = "DLL not found: " + dllPath.string();
            continue;
        }

        HMODULE hModule = LoadLibraryW(dllPath.c_str());
        if (!hModule) {
            info.state = PluginState::Error;
            info.errorMessage = "Failed to load DLL";
            continue;
        }

        auto createFn = (CreatePluginFn)GetProcAddress(hModule, "CreatePlugin");
        auto destroyFn = (DestroyPluginFn)GetProcAddress(hModule, "DestroyPlugin");

        if (!createFn || !destroyFn) {
            FreeLibrary(hModule);
            info.state = PluginState::Error;
            info.errorMessage = "Missing CreatePlugin/DestroyPlugin exports";
            continue;
        }

        IPlugin* instance = createFn();
        if (!instance) {
            FreeLibrary(hModule);
            info.state = PluginState::Error;
            info.errorMessage = "CreatePlugin returned null";
            continue;
        }

        info.state = PluginState::Loaded;

        // Register virtual host for plugin frontend
        if (!info.manifest.frontendDir.empty() && m_virtualHostRegistrar) {
            auto frontendPath = std::filesystem::path(dir) / info.manifest.id / info.manifest.frontendDir;
            m_virtualHostRegistrar(
                Utf8ToWide(info.manifest.virtualHost),
                frontendPath.wstring()
            );
        }

        LoadedPlugin loaded;
        loaded.info = info;
        loaded.hModule = hModule;
        loaded.instance = instance;
        loaded.destroyFn = destroyFn;
        m_loaded[info.manifest.id] = std::move(loaded);

        info.state = PluginState::Initialized;
    }
}

void PluginManager::ShutdownAll() {
    for (auto& [id, loaded] : m_loaded) {
        if (loaded.instance) {
            loaded.instance->Shutdown();
            if (loaded.destroyFn) loaded.destroyFn(loaded.instance);
            loaded.instance = nullptr;
        }
        if (loaded.hModule) {
            FreeLibrary(loaded.hModule);
            loaded.hModule = nullptr;
        }
    }
    m_loaded.clear();
}

std::vector<PluginInfo> PluginManager::ListPlugins() const {
    return m_discovered;
}

void PluginManager::SetEnabled(const std::string& pluginId, bool enabled) {
    for (auto& info : m_discovered) {
        if (info.manifest.id == pluginId) {
            info.state = enabled ? PluginState::Discovered : PluginState::Disabled;
        }
    }
}

} // namespace Manifold::Core::Plugins
