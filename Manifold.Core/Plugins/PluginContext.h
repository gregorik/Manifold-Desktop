#pragma once

// ABI-safe plugin context — all strings are const char* (UTF-8 JSON)
// All methods use C-compatible types for DLL boundary safety

namespace Manifold::Core::Plugins {

class PluginContext {
public:
    virtual ~PluginContext() = default;

    // Free a string returned by the context (allocated by the host)
    virtual void FreeString(const char* str) = 0;

    // Register a provider (JSON: {id, displayName, endpointUrl, requiresApiKey})
    virtual void RegisterProvider(const char* providerJson) = 0;

    // Register a tool (JSON: {name, description, parameters})
    virtual void RegisterTool(const char* toolJson) = 0;

    // Register a custom tab type (JSON: {id, displayName, icon})
    virtual void RegisterTabType(const char* tabTypeJson) = 0;

    // Post a message to the frontend (JSON envelope)
    virtual void PostMessage(const char* type, const char* payloadJson) = 0;

    // Register a handler for messages from the frontend
    using MessageCallback = void(*)(const char* type, const char* payloadJson, void* userData);
    virtual void OnMessage(const char* type, MessageCallback callback, void* userData) = 0;

    // Get settings for this plugin (returns JSON string — caller must FreeString)
    virtual const char* GetSettings() = 0;

    // Save settings for this plugin (JSON string)
    virtual void SaveSettings(const char* settingsJson) = 0;

    // Log a message
    virtual void Log(const char* level, const char* message) = 0;
};

} // namespace Manifold::Core::Plugins
