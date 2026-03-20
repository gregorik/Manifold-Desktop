#pragma once
#include <string>
#include <vector>

namespace Manifold::Core::Plugins {

enum class PluginState {
    Discovered,
    Loaded,
    Initialized,
    Error,
    Disabled
};

struct PluginManifest {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string dllName;        // e.g. "myplugin.dll"
    std::string frontendDir;    // relative path to frontend assets
    std::string virtualHost;    // e.g. "myplugin.manifold.local"
};

struct PluginInfo {
    PluginManifest manifest;
    PluginState state = PluginState::Discovered;
    std::string errorMessage;
};

} // namespace Manifold::Core::Plugins
