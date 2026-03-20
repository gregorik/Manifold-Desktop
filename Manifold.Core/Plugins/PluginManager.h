#pragma once
#include "IPlugin.h"
#include "PluginTypes.h"
#include "../Providers/IProvider.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <windows.h>

namespace Manifold::Core::Plugins {

class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    // Scan for plugin manifests in %LOCALAPPDATA%\Manifold\plugins\*/plugin.json
    void ScanPlugins();

    // Load and initialize enabled plugins
    void LoadEnabled(const std::map<std::string, bool>& enabledMap);

    // Shutdown all loaded plugins
    void ShutdownAll();

    // Get info about all discovered plugins
    std::vector<PluginInfo> ListPlugins() const;

    // Enable/disable a plugin (requires restart to take effect)
    void SetEnabled(const std::string& pluginId, bool enabled);

    // Registrars set by MainWindow
    using VirtualHostRegistrar = std::function<void(const std::wstring& host, const std::wstring& path)>;
    using ProviderRegistrar = std::function<void(std::unique_ptr<Manifold::Core::IProvider>)>;

    void SetVirtualHostRegistrar(VirtualHostRegistrar fn) { m_virtualHostRegistrar = std::move(fn); }
    void SetProviderRegistrar(ProviderRegistrar fn) { m_providerRegistrar = std::move(fn); }

private:
    struct LoadedPlugin {
        PluginInfo info;
        HMODULE hModule = nullptr;
        IPlugin* instance = nullptr;
        DestroyPluginFn destroyFn = nullptr;
    };

    std::vector<PluginInfo> m_discovered;
    std::map<std::string, LoadedPlugin> m_loaded;

    VirtualHostRegistrar m_virtualHostRegistrar;
    ProviderRegistrar m_providerRegistrar;

    std::wstring GetPluginsDir() const;
};

} // namespace Manifold::Core::Plugins
