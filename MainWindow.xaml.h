#pragma once

#include "MainWindow.g.h"
#include "Manifold.Core/ProcessManager.h"
#include "Manifold.Core/TerminalEmulator.h"
#include "Manifold.Core/SettingsManager.h"
#include "Manifold.Core/CredentialManager.h"
#include "Manifold.Core/SessionManager.h"
#include "Manifold.Core/PromptManager.h"
#include "Manifold.Core/Version.h"
#include "Manifold.Core/json.hpp"
#include "Manifold.Core/StringUtils.h"
#include "Manifold.Core/Providers/ProviderRegistry.h"
#include "Manifold.Core/Providers/GeminiProvider.h"
#include "Manifold.Core/Providers/OpenAIProvider.h"
#include "Manifold.Core/Providers/AnthropicProvider.h"
#include "Manifold.Core/Providers/OpenAICompatProvider.h"
#include "Manifold.Core/Providers/ProxyProvider.h"
#include "Manifold.Core/MCP/MCPClient.h"
#include "Manifold.Core/Plugins/PluginManager.h"
#include <WebView2.h>
#include <wil/com.h>

namespace winrt::ManifoldDesktop::implementation
{
    struct TerminalSession {
        std::unique_ptr<Manifold::Core::ProcessManager> process;
        std::unique_ptr<Manifold::Core::TerminalEmulator> emulator;
    };

    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

    private:
        HWND m_hwnd{ nullptr };

        // WebView2 COM objects (RAII via wil)
        wil::com_ptr<ICoreWebView2Environment> m_webViewEnv;
        wil::com_ptr<ICoreWebView2Controller>  m_webViewCtrl;
        wil::com_ptr<ICoreWebView2>            m_webView;
        wil::com_ptr<ICoreWebView2_3>          m_webView3;

        // UI thread dispatcher
        winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcherQueue{ nullptr };

        // Core managers
        Manifold::Core::SettingsManager m_settingsManager;
        Manifold::Core::SessionManager m_sessionManager;
        Manifold::Core::PromptManager m_promptManager;

        // Provider registry
        Manifold::Core::ProviderRegistry m_providerRegistry;
        std::jthread m_chatThread;
        std::vector<std::jthread> m_compareThreads;

        // MCP client
        Manifold::Core::MCP::MCPClient m_mcpClient;

        // Plugin manager
        Manifold::Core::Plugins::PluginManager m_pluginManager;

        // Terminal instances
        std::map<std::string, TerminalSession> m_terminals;

        // Cached settings
        Manifold::Core::AppSettings m_currentSettings;

        // Window state persistence
        void SaveWindowState();
        void RestoreWindowState();

        // WebView2 initialization
        void InitializeWebView();
        void OnWebViewCreated();
        void ResizeWebView();

        // Plugin virtual host registration
        void RegisterVirtualHost(const std::wstring& hostname, const std::wstring& folderPath);

        // Message bridge
        void OnWebMessage(const std::wstring& message);
        void PostMessageToWeb(const std::string& type, const nlohmann::json& payload);

        // Message handlers
        void HandleGetSettings();
        void HandleSaveSettings(const nlohmann::json& payload);
        void HandleSetApiKey(const nlohmann::json& payload);
        void HandleListSessions();
        void HandleLoadSession(const nlohmann::json& payload);
        void HandleSaveSession(const nlohmann::json& payload);
        void HandleDeleteSession(const nlohmann::json& payload);
        void HandleSearchSessions(const nlohmann::json& payload);
        void HandleOpenFileDialog();
        void HandleExportSession(const nlohmann::json& payload);
        void HandleImportSession();
        void HandleExportMarkdown(const nlohmann::json& payload);
        void HandleCopyToClipboard(const nlohmann::json& payload);
        void HandleOpenExternalUrl(const nlohmann::json& payload);

        // Chat handlers
        void HandleChatSend(const nlohmann::json& payload);
        void HandleChatCancel();
        void HandleListProviders();
        void HandleCheckForUpdates();
        void HandleListModels(const nlohmann::json& payload);
        void HandleValidateKey(const nlohmann::json& payload);

        // Terminal handlers
        void HandleTerminalStart(const nlohmann::json& payload);
        void HandleTerminalInput(const nlohmann::json& payload);
        void HandleTerminalStop(const nlohmann::json& payload);
        void HandleTerminalResize(const nlohmann::json& payload);

        // MCP handlers
        void HandleListMcpServers();
        void HandleAddMcpServer(const nlohmann::json& payload);
        void HandleRemoveMcpServer(const nlohmann::json& payload);
        void HandleListMcpTools();

        // Compare handlers
        void HandleCompareChat(const nlohmann::json& payload);
        void HandleCompareCancel();

        // Prompt handlers
        void HandleListPrompts();
        void HandleSavePrompt(const nlohmann::json& payload);
        void HandleDeletePrompt(const nlohmann::json& payload);

        // Plugin handlers
        void HandleListPlugins();
        void HandleSetPluginEnabled(const nlohmann::json& payload);
    };
}

namespace winrt::ManifoldDesktop::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
