#pragma once
#include "PluginContext.h"

namespace Manifold::Core::Plugins {

// ABI-safe plugin interface
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual const char* Id() const = 0;
    virtual const char* Name() const = 0;
    virtual const char* Version() const = 0;
    virtual bool Initialize(PluginContext* context) = 0;
    virtual void Shutdown() = 0;
};

} // namespace Manifold::Core::Plugins

// C exports for DLL loading
extern "C" {
    using CreatePluginFn = Manifold::Core::Plugins::IPlugin* (*)();
    using DestroyPluginFn = void(*)(Manifold::Core::Plugins::IPlugin*);
}

// DLL must export:
// extern "C" __declspec(dllexport) Manifold::Core::Plugins::IPlugin* CreatePlugin();
// extern "C" __declspec(dllexport) void DestroyPlugin(Manifold::Core::Plugins::IPlugin* plugin);
