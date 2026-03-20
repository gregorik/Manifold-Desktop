#include "pch.h"

#include <WebView2.h>
#include <microsoft.ui.xaml.window.h>
#include "Manifold.Core/Base64.h"
#include "Manifold.Core/StringUtils.h"
#include <wil/com.h>
#include <fstream>
#include <ctime>
#include <ShlObj.h>

#pragma comment(lib, "Shell32.lib")

#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace Manifold::Core;

// ---------------------------------------------------------------------------
//  Lightweight COM callback implementations for WebView2 async APIs
// ---------------------------------------------------------------------------
namespace
{
    class EnvironmentCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
    {
        std::function<HRESULT(HRESULT, ICoreWebView2Environment*)> m_fn;
        std::atomic<ULONG> m_ref{ 1 };
    public:
        explicit EnvironmentCompletedHandler(std::function<HRESULT(HRESULT, ICoreWebView2Environment*)> fn)
            : m_fn(std::move(fn)) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
            if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
                *ppv = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
                AddRef(); return S_OK;
            }
            *ppv = nullptr; return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override { auto c = --m_ref; if (c == 0) delete this; return c; }
        HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Environment* env) override { return m_fn(hr, env); }
    };

    class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
    {
        std::function<HRESULT(HRESULT, ICoreWebView2Controller*)> m_fn;
        std::atomic<ULONG> m_ref{ 1 };
    public:
        explicit ControllerCompletedHandler(std::function<HRESULT(HRESULT, ICoreWebView2Controller*)> fn)
            : m_fn(std::move(fn)) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
            if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
                *ppv = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
                AddRef(); return S_OK;
            }
            *ppv = nullptr; return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override { auto c = --m_ref; if (c == 0) delete this; return c; }
        HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Controller* ctrl) override { return m_fn(hr, ctrl); }
    };

    class WebMessageReceivedHandler : public ICoreWebView2WebMessageReceivedEventHandler
    {
        std::function<HRESULT(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)> m_fn;
        std::atomic<ULONG> m_ref{ 1 };
    public:
        explicit WebMessageReceivedHandler(
            std::function<HRESULT(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)> fn)
            : m_fn(std::move(fn)) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
            if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
                *ppv = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
                AddRef(); return S_OK;
            }
            *ppv = nullptr; return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override { auto c = --m_ref; if (c == 0) delete this; return c; }
        HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
            return m_fn(sender, args);
        }
    };

    class NavigationCompletedHandler : public ICoreWebView2NavigationCompletedEventHandler
    {
        std::function<HRESULT(ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)> m_fn;
        std::atomic<ULONG> m_ref{ 1 };
    public:
        explicit NavigationCompletedHandler(
            std::function<HRESULT(ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*)> fn)
            : m_fn(std::move(fn)) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
            if (riid == IID_IUnknown || riid == IID_ICoreWebView2NavigationCompletedEventHandler) {
                *ppv = static_cast<ICoreWebView2NavigationCompletedEventHandler*>(this);
                AddRef(); return S_OK;
            }
            *ppv = nullptr; return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
        ULONG STDMETHODCALLTYPE Release() override { auto c = --m_ref; if (c == 0) delete this; return c; }
        HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) override {
            return m_fn(sender, args);
        }
    };
}

// ---------------------------------------------------------------------------
//  MainWindow implementation
// ---------------------------------------------------------------------------
namespace winrt::ManifoldDesktop::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        m_dispatcherQueue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        auto windowNative = this->try_as<::IWindowNative>();
        check_hresult(windowNative->get_WindowHandle(&m_hwnd));

        RestoreWindowState();

        m_currentSettings = m_settingsManager.Load();

        // Register built-in providers
        m_providerRegistry.AddProvider(std::make_unique<GeminiProvider>());
        m_providerRegistry.AddProvider(std::make_unique<OpenAIProvider>());
        m_providerRegistry.AddProvider(std::make_unique<AnthropicProvider>());

        // Add user-configured OpenAI-compat providers from settings
        for (auto& [id, cfg] : m_currentSettings.providerConfigs) {
            if (cfg.enabled && !cfg.endpointUrl.empty()) {
                auto isLocal = cfg.endpointUrl.find("localhost") != std::string::npos
                            || cfg.endpointUrl.find("127.0.0.1") != std::string::npos;
                m_providerRegistry.AddProvider(
                    std::make_unique<OpenAICompatProvider>(id, id, cfg.endpointUrl, !isLocal));
            }
        }

        // Auto-register Ollama if endpoint is configured
        if (!m_currentSettings.ollamaEndpoint.empty()) {
            m_providerRegistry.AddProvider(
                std::make_unique<OpenAICompatProvider>(
                    "ollama", "Ollama (Local)",
                    m_currentSettings.ollamaEndpoint + "/v1", false));
        }

        // Initialize plugin system
        m_pluginManager.SetVirtualHostRegistrar([this](const std::wstring& host, const std::wstring& path) {
            RegisterVirtualHost(host, path);
        });
        m_pluginManager.LoadEnabled(m_currentSettings.pluginEnabled);

        // Connect to configured MCP servers
        for (auto& [id, mcpCfg] : m_currentSettings.mcpServers) {
            if (!mcpCfg.enabled) continue;
            MCP::McpServerConfig cfg;
            cfg.id = id;
            cfg.name = id;
            cfg.transportType = mcpCfg.transport;
            cfg.command = mcpCfg.command;
            cfg.args = mcpCfg.args;
            cfg.url = mcpCfg.url;
            m_mcpClient.ConnectServer(cfg);
        }

        RootGrid().SizeChanged([this](auto&&, auto&&) { ResizeWebView(); });

        // Cleanup on close
        this->Closed([this](auto&&, auto&&) {
            SaveWindowState();
            auto terminals = std::move(m_terminals);
            m_terminals.clear();
            for (auto& [id, session] : terminals) {
                session.process->Stop();
            }
            m_pluginManager.ShutdownAll();
            m_mcpClient.DisconnectAll();
            m_webView3 = nullptr;
            m_webView = nullptr;
            m_webViewCtrl = nullptr;
            m_webViewEnv = nullptr;
        });

        InitializeWebView();
    }

    void MainWindow::InitializeWebView()
    {
        PWSTR localAppDataPath = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataPath);
        if (FAILED(hr)) return;

        std::wstring appDataDir = std::wstring(localAppDataPath) + L"\\Manifold";
        CoTaskMemFree(localAppDataPath);
        std::wstring userDataFolder = appDataDir + L"\\webview2";

        CreateDirectoryW(appDataDir.c_str(), nullptr);
        CreateDirectoryW(userDataFolder.c_str(), nullptr);

        auto envHandler = new EnvironmentCompletedHandler(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) return result;
                m_webViewEnv = env;

                auto ctrlHandler = new ControllerCompletedHandler(
                    [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                        if (FAILED(result) || !controller) return result;
                        m_webViewCtrl = controller;

                        ICoreWebView2* webView = nullptr;
                        controller->get_CoreWebView2(&webView);
                        m_webView.attach(webView);

                        OnWebViewCreated();
                        return S_OK;
                    }
                );
                auto hr2 = env->CreateCoreWebView2Controller(m_hwnd, ctrlHandler);
                ctrlHandler->Release();
                return hr2;
            }
        );

        CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataFolder.c_str(), nullptr, envHandler);
        envHandler->Release();
    }

    void MainWindow::OnWebViewCreated()
    {
        // Dark background
        wil::com_ptr<ICoreWebView2Controller2> controller2;
        if (SUCCEEDED(m_webViewCtrl->QueryInterface(IID_PPV_ARGS(controller2.put()))))
        {
            COREWEBVIEW2_COLOR bgColor = { 255, 24, 24, 27 };
            controller2->put_DefaultBackgroundColor(bgColor);
        }

        // Settings
        wil::com_ptr<ICoreWebView2Settings> settings;
        m_webView->get_Settings(settings.put());
        if (settings)
        {
            settings->put_IsScriptEnabled(TRUE);
            settings->put_AreDefaultScriptDialogsEnabled(TRUE);
            settings->put_IsWebMessageEnabled(TRUE);
#ifdef _DEBUG
            settings->put_AreDevToolsEnabled(TRUE);
#else
            settings->put_AreDevToolsEnabled(FALSE);
#endif
            // Disable default browser accelerator keys so Ctrl+N/T/W reach JS
            wil::com_ptr<ICoreWebView2Settings3> settings3;
            if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(settings3.put())))) {
                settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
            }
        }

        // Virtual host mapping — keep m_webView3 for plugin host registration later
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring exeDir(exePath);
        exeDir = exeDir.substr(0, exeDir.find_last_of(L'\\'));
        std::wstring frontendPath = exeDir + L"\\frontend";

        if (SUCCEEDED(m_webView->QueryInterface(IID_PPV_ARGS(m_webView3.put()))))
        {
            m_webView3->SetVirtualHostNameToFolderMapping(
                L"manifold.local", frontendPath.c_str(),
                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
            );
        }

        // Web message handler
        EventRegistrationToken token;
        auto msgHandler = new WebMessageReceivedHandler(
            [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR messageRaw = nullptr;
                args->TryGetWebMessageAsString(&messageRaw);
                if (messageRaw) {
                    std::wstring message(messageRaw);
                    CoTaskMemFree(messageRaw);
                    OnWebMessage(message);
                }
                return S_OK;
            }
        );
        m_webView->add_WebMessageReceived(msgHandler, &token);
        msgHandler->Release();

        // Navigation completed — send HOST_READY
        auto navHandler = new NavigationCompletedHandler(
            [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                nlohmann::json providerKeys = nlohmann::json::object();
                for (auto* p : m_providerRegistry.ListProviders()) {
                    auto wideId = Utf8ToWide(p->Id());
                    providerKeys[p->Id()] = !CredentialManager::LoadApiKey(wideId).empty();
                }
                auto settingsJson = m_settingsManager.ToJson(m_currentSettings);
                nlohmann::json payload = {
                    {"providerKeys", providerKeys},
                    {"settings", settingsJson},
                    {"version", Manifold::Core::VERSION_STRING},
                };
                PostMessageToWeb("HOST_READY", payload);
                return S_OK;
            }
        );
        m_webView->add_NavigationCompleted(navHandler, &token);
        navHandler->Release();

        m_webView->Navigate(L"https://manifold.local/index.html");
        ResizeWebView();
    }

    void MainWindow::ResizeWebView()
    {
        if (m_webViewCtrl) {
            RECT bounds;
            ::GetClientRect(m_hwnd, &bounds);
            m_webViewCtrl->put_Bounds(bounds);
        }
    }

    void MainWindow::RegisterVirtualHost(const std::wstring& hostname, const std::wstring& folderPath)
    {
        if (m_webView3) {
            m_webView3->SetVirtualHostNameToFolderMapping(
                hostname.c_str(), folderPath.c_str(),
                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
        }
    }

    void MainWindow::SaveWindowState()
    {
        wchar_t* appData = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData))) return;
        std::wstring dir = std::wstring(appData) + L"\\Manifold";
        CoTaskMemFree(appData);
        CreateDirectoryW(dir.c_str(), nullptr);
        std::wstring path = dir + L"\\window-state.json";

        RECT rc;
        if (!m_hwnd || !::GetWindowRect(m_hwnd, &rc)) return;

        WINDOWPLACEMENT wp = { sizeof(wp) };
        ::GetWindowPlacement(m_hwnd, &wp);
        bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED);

        nlohmann::json state = {
            {"x", rc.left}, {"y", rc.top},
            {"width", rc.right - rc.left}, {"height", rc.bottom - rc.top},
            {"maximized", maximized}
        };

        std::ofstream f(path);
        if (f.is_open()) f << state.dump(2);
    }

    void MainWindow::RestoreWindowState()
    {
        wchar_t* appData = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData))) return;
        std::wstring path = std::wstring(appData) + L"\\Manifold\\window-state.json";
        CoTaskMemFree(appData);

        std::ifstream f(path);
        if (!f.is_open()) return;

        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto state = nlohmann::json::parse(content, nullptr, false);
        if (state.is_discarded()) return;

        int x = state.value("x", 100);
        int y = state.value("y", 100);
        int w = state.value("width", 1200);
        int h = state.value("height", 800);
        bool maximized = state.value("maximized", false);

        if (!m_hwnd) return;

        RECT workArea;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        if (x < workArea.left || x > workArea.right - 100) x = workArea.left + 50;
        if (y < workArea.top || y > workArea.bottom - 100) y = workArea.top + 50;
        if (w < 400) w = 1200;
        if (h < 300) h = 800;

        ::MoveWindow(m_hwnd, x, y, w, h, TRUE);
        if (maximized) ::ShowWindow(m_hwnd, SW_MAXIMIZE);
    }

    // --- Message Bridge ---

    void MainWindow::OnWebMessage(const std::wstring& message)
    {
        auto utf8 = WideToUtf8(message);
        auto msg = nlohmann::json::parse(utf8, nullptr, false);
        if (msg.is_discarded()) return;

        auto type = msg.value("type", "");
        auto payload = msg.contains("payload") ? msg["payload"] : nlohmann::json::object();

        // For messages that spread fields at top level (bridge.send compatibility)
        if (payload.is_object() && payload.empty()) {
            payload = msg;
            payload.erase("type");
        }

        if (type == "GET_SETTINGS")          HandleGetSettings();
        else if (type == "SAVE_SETTINGS")    HandleSaveSettings(payload);
        else if (type == "SET_API_KEY")      HandleSetApiKey(payload);
        else if (type == "LIST_SESSIONS")    HandleListSessions();
        else if (type == "LOAD_SESSION")     HandleLoadSession(payload);
        else if (type == "SAVE_SESSION")     HandleSaveSession(payload);
        else if (type == "DELETE_SESSION")   HandleDeleteSession(payload);
        else if (type == "SEARCH_SESSIONS")  HandleSearchSessions(payload);
        else if (type == "OPEN_FILE_DIALOG") HandleOpenFileDialog();
        else if (type == "EXPORT_SESSION")  HandleExportSession(payload);
        else if (type == "IMPORT_SESSION")  HandleImportSession();
        else if (type == "EXPORT_MARKDOWN") HandleExportMarkdown(payload);
        else if (type == "COPY_TO_CLIPBOARD") HandleCopyToClipboard(payload);
        else if (type == "OPEN_EXTERNAL_URL") HandleOpenExternalUrl(payload);
        else if (type == "CHAT_SEND")        HandleChatSend(payload);
        else if (type == "CHAT_CANCEL")      HandleChatCancel();
        else if (type == "LIST_PROVIDERS")   HandleListProviders();
        else if (type == "LIST_MODELS")      HandleListModels(payload);
        else if (type == "VALIDATE_KEY")     HandleValidateKey(payload);
        else if (type == "TERMINAL_START")   HandleTerminalStart(payload);
        else if (type == "TERMINAL_INPUT")   HandleTerminalInput(payload);
        else if (type == "TERMINAL_STOP")    HandleTerminalStop(payload);
        else if (type == "TERMINAL_RESIZE")  HandleTerminalResize(payload);
        else if (type == "LIST_MCP_SERVERS") HandleListMcpServers();
        else if (type == "ADD_MCP_SERVER")   HandleAddMcpServer(payload);
        else if (type == "REMOVE_MCP_SERVER") HandleRemoveMcpServer(payload);
        else if (type == "LIST_MCP_TOOLS")   HandleListMcpTools();
        else if (type == "COMPARE_CHAT")      HandleCompareChat(payload);
        else if (type == "COMPARE_CANCEL")   HandleCompareCancel();
        else if (type == "LIST_PROMPTS")      HandleListPrompts();
        else if (type == "SAVE_PROMPT")       HandleSavePrompt(payload);
        else if (type == "DELETE_PROMPT")     HandleDeletePrompt(payload);
        else if (type == "LIST_PLUGINS")     HandleListPlugins();
        else if (type == "TOGGLE_PLUGIN")    HandleSetPluginEnabled(payload);
    }

    void MainWindow::PostMessageToWeb(const std::string& type, const nlohmann::json& payload)
    {
        if (!m_webView) return;
        nlohmann::json msg = {{"type", type}, {"payload", payload}};
        auto wideMsg = Utf8ToWide(msg.dump());
        m_webView->PostWebMessageAsString(wideMsg.c_str());
    }

    // --- Handlers ---

    void MainWindow::HandleGetSettings()
    {
        nlohmann::json providerKeys = nlohmann::json::object();
        for (auto* p : m_providerRegistry.ListProviders()) {
            auto wideId = Utf8ToWide(p->Id());
            providerKeys[p->Id()] = !CredentialManager::LoadApiKey(wideId).empty();
        }
        nlohmann::json payload = {
            {"providerKeys", providerKeys},
            {"settings", m_settingsManager.ToJson(m_currentSettings)}
        };
        PostMessageToWeb("SETTINGS_LOADED", payload);
    }

    void MainWindow::HandleSaveSettings(const nlohmann::json& payload)
    {
        m_currentSettings = m_settingsManager.FromJson(payload);

        // Clamp values to valid ranges
        if (m_currentSettings.temperature < 0.0) m_currentSettings.temperature = 0.0;
        if (m_currentSettings.temperature > 2.0) m_currentSettings.temperature = 2.0;
        if (m_currentSettings.fontSize < 10) m_currentSettings.fontSize = 10;
        if (m_currentSettings.fontSize > 24) m_currentSettings.fontSize = 24;
        if (m_currentSettings.systemPrompt.size() > 10000)
            m_currentSettings.systemPrompt = m_currentSettings.systemPrompt.substr(0, 10000);

        m_settingsManager.Save(m_currentSettings);
        PostMessageToWeb("SETTINGS_UPDATED", m_settingsManager.ToJson(m_currentSettings));
    }

    void MainWindow::HandleSetApiKey(const nlohmann::json& payload)
    {
        auto providerId = payload.value("providerId", "");
        auto key = payload.value("apiKey", "");
        auto wideProviderId = Utf8ToWide(providerId);
        if (key.empty())
            CredentialManager::DeleteApiKey(wideProviderId);
        else
            CredentialManager::SaveApiKey(wideProviderId, Utf8ToWide(key));
        PostMessageToWeb("API_KEY_SAVED", {{"providerId", providerId}});
    }

    void MainWindow::HandleListSessions()
    {
        auto sessions = m_sessionManager.ListSessions();
        nlohmann::json arr = nlohmann::json::array();
        for (auto& s : sessions) {
            arr.push_back({
                {"id", s.id}, {"title", s.title}, {"model", s.model},
                {"createdAt", s.createdAt}, {"updatedAt", s.updatedAt}
            });
        }
        PostMessageToWeb("SESSIONS_LOADED", arr);
    }

    void MainWindow::HandleLoadSession(const nlohmann::json& payload)
    {
        auto id = payload.value("id", "");
        auto data = m_sessionManager.LoadSession(id);
        PostMessageToWeb("SESSION_DATA", data.is_null() ? nlohmann::json(nullptr) : data);
    }

    void MainWindow::HandleSaveSession(const nlohmann::json& payload)
    {
        auto id = payload.value("id", "");
        auto data = payload.value("data", nlohmann::json::object());
        m_sessionManager.SaveSession(id, data);
        PostMessageToWeb("SESSION_SAVED", true);
    }

    void MainWindow::HandleDeleteSession(const nlohmann::json& payload)
    {
        auto id = payload.value("id", "");
        m_sessionManager.DeleteSession(id);
        PostMessageToWeb("SESSION_DELETED", true);
    }

    void MainWindow::HandleSearchSessions(const nlohmann::json& payload)
    {
        auto query = payload.value("query", "");
        auto results = m_sessionManager.SearchSessions(query);

        nlohmann::json arr = nlohmann::json::array();
        for (auto& r : results) {
            arr.push_back({
                {"sessionId", r.sessionId},
                {"title", r.title},
                {"snippet", r.snippet}
            });
        }
        PostMessageToWeb("SEARCH_RESULTS", arr);
    }

    void MainWindow::HandleOpenFileDialog()
    {
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
            IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (FAILED(hr)) return;

        pFileOpen->SetOptions(FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);
        hr = pFileOpen->Show(m_hwnd);

        if (SUCCEEDED(hr))
        {
            IShellItemArray* pItems = nullptr;
            pFileOpen->GetResults(&pItems);
            if (pItems)
            {
                DWORD count = 0;
                pItems->GetCount(&count);

                for (DWORD i = 0; i < count; ++i)
                {
                    IShellItem* pItem = nullptr;
                    pItems->GetItemAt(i, &pItem);
                    if (pItem)
                    {
                        PWSTR filePath = nullptr;
                        pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
                        if (filePath)
                        {
                            std::ifstream file(filePath, std::ios::binary);
                            if (file.is_open())
                            {
                                std::vector<uint8_t> data(
                                    (std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());

                                auto b64 = Manifold::Core::Base64Encode(data);

                                std::wstring fullPath(filePath);
                                auto slash = fullPath.find_last_of(L'\\');
                                auto filename = (slash != std::wstring::npos)
                                    ? fullPath.substr(slash + 1) : fullPath;

                                nlohmann::json payload = {
                                    {"name", WideToUtf8(filename)},
                                    {"data", b64}
                                };
                                PostMessageToWeb("FILE_ATTACHED", payload);
                            }
                            CoTaskMemFree(filePath);
                        }
                        pItem->Release();
                    }
                }
                pItems->Release();
            }
        }
        pFileOpen->Release();
    }

    void MainWindow::HandleExportSession(const nlohmann::json& payload)
    {
        auto id = payload.value("id", "");
        auto data = m_sessionManager.LoadSession(id);
        if (data.is_null()) return;

        IFileSaveDialog* pFileSave = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileSave)))) return;

        COMDLG_FILTERSPEC filter = { L"JSON Files", L"*.json" };
        pFileSave->SetFileTypes(1, &filter);
        pFileSave->SetDefaultExtension(L"json");
        pFileSave->SetFileName(L"session-export.json");

        if (SUCCEEDED(pFileSave->Show(m_hwnd))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileSave->GetResult(&pItem))) {
                LPWSTR filePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath))) {
                    std::ofstream f(filePath);
                    if (f.is_open()) f << data.dump(2);
                    CoTaskMemFree(filePath);
                }
                pItem->Release();
            }
        }
        pFileSave->Release();
    }

    void MainWindow::HandleImportSession()
    {
        IFileOpenDialog* pFileOpen = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileOpen)))) return;

        COMDLG_FILTERSPEC filter = { L"JSON Files", L"*.json" };
        pFileOpen->SetFileTypes(1, &filter);

        if (SUCCEEDED(pFileOpen->Show(m_hwnd))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                LPWSTR filePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath))) {
                    std::ifstream f(filePath);
                    CoTaskMemFree(filePath);
                    if (f.is_open()) {
                        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                        auto data = nlohmann::json::parse(content, nullptr, false);
                        if (!data.is_discarded()) {
                            auto id = data.value("id", "imported-" + std::to_string(std::time(nullptr)));
                            m_sessionManager.SaveSession(id, data);
                            PostMessageToWeb("SESSION_IMPORTED", {{"id", id}});
                        }
                    }
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }

    void MainWindow::HandleExportMarkdown(const nlohmann::json& payload)
    {
        auto id = payload.value("id", "");
        auto data = m_sessionManager.LoadSession(id);
        if (data.is_null()) return;

        // Build markdown
        std::string md = "# " + data.value("title", "Chat") + "\n\n";
        md += "**Provider:** " + data.value("provider", "") +
              " | **Model:** " + data.value("model", "") +
              " | **Date:** " + data.value("updatedAt", "") + "\n\n---\n\n";

        auto messages = data.value("messages", nlohmann::json::array());
        for (auto& msg : messages) {
            auto role = msg.value("role", "user");
            auto content = msg.value("content", msg.value("text", ""));
            if (role == "user") md += "### User\n\n" + content + "\n\n";
            else if (role == "assistant") md += "### Assistant\n\n" + content + "\n\n";
            else md += "### System\n\n" + content + "\n\n";
            md += "---\n\n";
        }

        IFileSaveDialog* pFileSave = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileSave)))) return;

        COMDLG_FILTERSPEC filter = { L"Markdown Files", L"*.md" };
        pFileSave->SetFileTypes(1, &filter);
        pFileSave->SetDefaultExtension(L"md");
        pFileSave->SetFileName(L"chat-export.md");

        if (SUCCEEDED(pFileSave->Show(m_hwnd))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileSave->GetResult(&pItem))) {
                LPWSTR filePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath))) {
                    std::ofstream f(filePath);
                    if (f.is_open()) f << md;
                    CoTaskMemFree(filePath);
                }
                pItem->Release();
            }
        }
        pFileSave->Release();
    }

    void MainWindow::HandleCopyToClipboard(const nlohmann::json& payload)
    {
        auto text = payload.value("text", "");
        if (text.empty()) return;
        auto wtext = Utf8ToWide(text);

        if (OpenClipboard(m_hwnd)) {
            EmptyClipboard();
            size_t size = (wtext.size() + 1) * sizeof(wchar_t);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
            if (hMem) {
                auto pMem = static_cast<wchar_t*>(GlobalLock(hMem));
                if (pMem) {
                    wcscpy_s(pMem, wtext.size() + 1, wtext.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_UNICODETEXT, hMem);
                }
            }
            CloseClipboard();
        }
    }

    void MainWindow::HandleOpenExternalUrl(const nlohmann::json& payload)
    {
        auto url = payload.value("url", "");
        if (!url.empty()) {
            auto wurl = Utf8ToWide(url);
            ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    // --- Chat Handlers ---

    void MainWindow::HandleChatSend(const nlohmann::json& payload)
    {
        auto providerId = payload.value("provider", m_currentSettings.activeProviderId);
        auto modelId = payload.value("model", m_currentSettings.model);
        auto messagesJson = payload.value("messages", nlohmann::json::array());
        auto systemPrompt = payload.value("systemPrompt", m_currentSettings.systemPrompt);

        auto* provider = m_providerRegistry.GetProvider(providerId);
        if (!provider) {
            PostMessageToWeb("CHAT_ERROR", {{"error", "Provider not found: " + providerId}});
            return;
        }

        // Build ChatRequest
        ChatRequest req;
        req.providerId = providerId;
        req.modelId = modelId;
        req.systemPrompt = systemPrompt;
        req.temperature = m_currentSettings.temperature;
        req.apiKey = WideToUtf8(CredentialManager::LoadApiKey(Utf8ToWide(providerId)));

        for (auto& mj : messagesJson) {
            ChatMessage msg;
            msg.role = mj.value("role", "user");
            msg.text = mj.value("content", "");
            req.messages.push_back(std::move(msg));
        }

        // Inject MCP tools
        auto mcpTools = m_mcpClient.GetAllTools();
        for (auto& td : mcpTools) {
            req.tools.push_back(std::move(td));
        }

        // Cancel any existing chat thread
        if (m_chatThread.joinable()) {
            m_chatThread.request_stop();
            m_chatThread = std::jthread{};
        }

        // Stream on background thread
        m_chatThread = std::jthread([this, req = std::move(req), provider](std::stop_token stopToken) {
            try {
                auto response = provider->StreamChat(req, [this, &stopToken](const StreamChunk& chunk) {
                    if (stopToken.stop_requested()) return;

                    nlohmann::json chunkData;
                    if (!chunk.text.empty()) {
                        chunkData = {{"text", chunk.text}, {"done", chunk.done}};
                    } else if (chunk.toolCall.has_value()) {
                        chunkData = {
                            {"toolCall", {
                                {"id", chunk.toolCall->id},
                                {"name", chunk.toolCall->name},
                                {"arguments", chunk.toolCall->arguments}
                            }},
                            {"done", chunk.done}
                        };
                    } else {
                        chunkData = {{"text", ""}, {"done", chunk.done}};
                    }

                    m_dispatcherQueue.TryEnqueue([this, chunkData = std::move(chunkData)]() {
                        PostMessageToWeb("CHAT_CHUNK", chunkData);
                    });
                });

                if (!stopToken.stop_requested()) {
                    nlohmann::json doneData = {
                        {"promptTokens", response.promptTokens},
                        {"completionTokens", response.completionTokens},
                        {"finishReason", response.finishReason}
                    };
                    m_dispatcherQueue.TryEnqueue([this, doneData = std::move(doneData)]() {
                        PostMessageToWeb("CHAT_DONE", doneData);
                    });
                }
            } catch (const std::exception& ex) {
                std::string err = ex.what();
                m_dispatcherQueue.TryEnqueue([this, err = std::move(err)]() {
                    PostMessageToWeb("CHAT_ERROR", {{"error", err}});
                });
            }
        });
    }

    void MainWindow::HandleChatCancel()
    {
        if (m_chatThread.joinable()) {
            m_chatThread.request_stop();
            m_chatThread = std::jthread{};
        }
        PostMessageToWeb("CHAT_CANCELLED", true);
    }

    void MainWindow::HandleListProviders()
    {
        nlohmann::json providers = nlohmann::json::array();
        for (auto* p : m_providerRegistry.ListProviders()) {
            nlohmann::json models = nlohmann::json::array();
            for (auto& m : p->ListModels()) {
                models.push_back({
                    {"id", m.id}, {"displayName", m.displayName},
                    {"isFree", m.isFree}, {"freeRpmLimit", m.freeRpmLimit},
                    {"note", m.note}
                });
            }
            providers.push_back({
                {"id", p->Id()},
                {"displayName", p->DisplayName()},
                {"requiresApiKey", p->RequiresApiKey()},
                {"supportsStreaming", p->SupportsStreaming()},
                {"supportsTools", p->SupportsTools()},
                {"models", models}
            });
        }
        PostMessageToWeb("PROVIDERS_LIST", providers);
    }

    void MainWindow::HandleListModels(const nlohmann::json& payload)
    {
        auto providerId = payload.value("providerId", "");
        auto* provider = m_providerRegistry.GetProvider(providerId);
        if (!provider) {
            PostMessageToWeb("MODELS_LIST", nlohmann::json::array());
            return;
        }
        nlohmann::json models = nlohmann::json::array();
        for (auto& m : provider->ListModels()) {
            models.push_back({
                {"id", m.id}, {"displayName", m.displayName},
                {"isFree", m.isFree}, {"note", m.note}
            });
        }
        PostMessageToWeb("MODELS_LIST", models);
    }

    void MainWindow::HandleValidateKey(const nlohmann::json& payload)
    {
        auto providerId = payload.value("providerId", "");
        auto apiKey = payload.value("apiKey", "");
        auto* provider = m_providerRegistry.GetProvider(providerId);
        if (!provider) {
            PostMessageToWeb("KEY_VALIDATION", {{"providerId", providerId}, {"valid", false}, {"error", "Provider not found"}});
            return;
        }
        bool valid = provider->ValidateKey(apiKey);
        PostMessageToWeb("KEY_VALIDATION", {{"providerId", providerId}, {"valid", valid}, {"error", valid ? "" : "Invalid API key"}});
    }

    // --- Terminal Handlers ---

    void MainWindow::HandleTerminalStart(const nlohmann::json& payload)
    {
        auto tabId = payload.value("tabId", "");
        auto command = payload.value("command", "cmd.exe");
        if (tabId.empty()) return;

        TerminalSession session;
        session.process = std::make_unique<ProcessManager>();
        session.emulator = std::make_unique<TerminalEmulator>(120, 30);

        auto tabIdCopy = tabId;
        auto* emulatorRaw = session.emulator.get();
        session.process->SetOutputCallback([this, tabIdCopy, emulatorRaw](const std::wstring& data) {
            emulatorRaw->Feed(data);
            auto dirty = emulatorRaw->RenderDirty();
            emulatorRaw->AcknowledgeRender();

            nlohmann::json rows = nlohmann::json::array();
            for (auto& row : dirty.dirtyRows) {
                nlohmann::json segs = nlohmann::json::array();
                for (auto& seg : row.segments) {
                    auto& fg = seg.style.foreground;
                    auto& bg = seg.style.background;
                    char fgBuf[8], bgBuf[8];
                    snprintf(fgBuf, sizeof(fgBuf), "#%02X%02X%02X", fg.r, fg.g, fg.b);
                    snprintf(bgBuf, sizeof(bgBuf), "#%02X%02X%02X", bg.r, bg.g, bg.b);

                    segs.push_back({
                        {"text", WideToUtf8(seg.text)},
                        {"fg", fg.isDefault ? nlohmann::json(nullptr) : nlohmann::json(fgBuf)},
                        {"bg", bg.isDefault ? nlohmann::json(nullptr) : nlohmann::json(bgBuf)},
                        {"bold", seg.style.isBold},
                        {"italic", seg.style.isItalic},
                        {"underline", seg.style.isUnderline}
                    });
                }
                rows.push_back({{"line", row.rowIndex}, {"segments", segs}});
            }

            auto rowData = nlohmann::json{{"tabId", tabIdCopy}, {"rows", rows}};
            m_dispatcherQueue.TryEnqueue([this, rowData = std::move(rowData)]() {
                PostMessageToWeb("TERMINAL_OUTPUT", rowData);
            });
        });

        session.process->Start(Utf8ToWide(command));
        m_terminals[tabId] = std::move(session);
    }

    void MainWindow::HandleTerminalInput(const nlohmann::json& payload)
    {
        auto tabId = payload.value("tabId", "");
        auto data = payload.value("data", "");
        auto it = m_terminals.find(tabId);
        if (it != m_terminals.end()) {
            it->second.process->WriteRawBytes(data);
        }
    }

    void MainWindow::HandleTerminalStop(const nlohmann::json& payload)
    {
        auto tabId = payload.value("tabId", "");
        auto it = m_terminals.find(tabId);
        if (it == m_terminals.end()) return;
        auto session = std::move(it->second);
        m_terminals.erase(it);
        session.process->Stop();
        PostMessageToWeb("TERMINAL_STOPPED", {{"tabId", tabId}});
    }

    void MainWindow::HandleTerminalResize(const nlohmann::json& payload)
    {
        auto tabId = payload.value("tabId", "");
        int cols = payload.value("cols", 120);
        int rows = payload.value("rows", 30);
        auto it = m_terminals.find(tabId);
        if (it != m_terminals.end()) {
            it->second.emulator->Resize(cols, rows);
            it->second.process->Resize(static_cast<SHORT>(cols), static_cast<SHORT>(rows));
        }
    }

    // --- MCP Handlers ---

    void MainWindow::HandleListMcpServers()
    {
        nlohmann::json servers = nlohmann::json::array();
        for (auto& [id, server] : m_mcpClient.GetServers()) {
            servers.push_back({
                {"id", server.config.id},
                {"name", server.config.name},
                {"transportType", server.config.transportType},
                {"command", server.config.command},
                {"url", server.config.url},
                {"connected", server.transport->IsConnected()},
                {"toolCount", (int)server.tools.size()}
            });
        }
        PostMessageToWeb("MCP_SERVERS_LIST", servers);
    }

    void MainWindow::HandleAddMcpServer(const nlohmann::json& payload)
    {
        MCP::McpServerConfig cfg;
        cfg.id = payload.value("id", payload.value("name", ""));
        cfg.name = payload.value("name", "");
        cfg.transportType = payload.value("transportType", "stdio");
        cfg.command = payload.value("command", "");
        cfg.url = payload.value("url", "");
        if (payload.contains("args") && payload["args"].is_array()) {
            for (auto& a : payload["args"]) cfg.args.push_back(a.get<std::string>());
        }

        bool ok = m_mcpClient.ConnectServer(cfg);
        PostMessageToWeb("MCP_SERVER_ADDED", {{"id", cfg.id}, {"success", ok}});

        // Refresh list
        HandleListMcpServers();
    }

    void MainWindow::HandleRemoveMcpServer(const nlohmann::json& payload)
    {
        auto serverId = payload.value("serverId", "");
        m_mcpClient.DisconnectServer(serverId);
        PostMessageToWeb("MCP_SERVER_REMOVED", {{"serverId", serverId}});
        HandleListMcpServers();
    }

    void MainWindow::HandleListMcpTools()
    {
        auto tools = m_mcpClient.GetAllTools();
        nlohmann::json arr = nlohmann::json::array();
        for (auto& t : tools) {
            arr.push_back({
                {"name", t.name},
                {"description", t.description}
            });
        }
        PostMessageToWeb("MCP_TOOLS_LIST", arr);
    }

    // --- Plugin Handlers ---

    // --- Compare handlers ---

    void MainWindow::HandleCompareChat(const nlohmann::json& payload)
    {
        // Cancel any existing comparison
        HandleCompareCancel();

        auto slots = payload.value("slots", nlohmann::json::array());
        auto messagesJson = payload.value("messages", nlohmann::json::array());
        auto systemPrompt = payload.value("systemPrompt", m_currentSettings.systemPrompt);

        for (int i = 0; i < static_cast<int>(slots.size()); i++) {
            auto providerId = slots[i].value("providerId", "");
            auto modelId = slots[i].value("modelId", "");
            auto* provider = m_providerRegistry.GetProvider(providerId);
            if (!provider) continue;

            ChatRequest req;
            req.providerId = providerId;
            req.modelId = modelId;
            req.systemPrompt = systemPrompt;
            req.temperature = m_currentSettings.temperature;
            req.apiKey = WideToUtf8(CredentialManager::LoadApiKey(Utf8ToWide(providerId)));

            for (auto& mj : messagesJson) {
                ChatMessage msg;
                msg.role = mj.value("role", "user");
                msg.text = mj.value("content", "");
                req.messages.push_back(std::move(msg));
            }

            int slotIndex = i;
            m_compareThreads.emplace_back([this, req = std::move(req), provider, slotIndex](std::stop_token stopToken) {
                try {
                    auto response = provider->StreamChat(req, [this, &stopToken, slotIndex](const StreamChunk& chunk) {
                        if (stopToken.stop_requested()) return;
                        nlohmann::json chunkData = {
                            {"slotIndex", slotIndex},
                            {"text", chunk.text},
                            {"done", chunk.done}
                        };
                        m_dispatcherQueue.TryEnqueue([this, chunkData = std::move(chunkData)]() {
                            PostMessageToWeb("COMPARE_CHUNK", chunkData);
                        });
                    });

                    if (!stopToken.stop_requested()) {
                        nlohmann::json doneData = {
                            {"slotIndex", slotIndex},
                            {"promptTokens", response.promptTokens},
                            {"completionTokens", response.completionTokens}
                        };
                        m_dispatcherQueue.TryEnqueue([this, doneData = std::move(doneData)]() {
                            PostMessageToWeb("COMPARE_DONE", doneData);
                        });
                    }
                } catch (const std::exception& ex) {
                    std::string err = ex.what();
                    m_dispatcherQueue.TryEnqueue([this, slotIndex, err = std::move(err)]() {
                        PostMessageToWeb("COMPARE_ERROR", {{"slotIndex", slotIndex}, {"error", err}});
                    });
                }
            });
        }
    }

    void MainWindow::HandleCompareCancel()
    {
        for (auto& t : m_compareThreads) {
            if (t.joinable()) t.request_stop();
        }
        m_compareThreads.clear();
        PostMessageToWeb("COMPARE_CANCELLED", true);
    }

    // --- Prompt handlers ---

    void MainWindow::HandleListPrompts()
    {
        PostMessageToWeb("PROMPTS_LIST", m_promptManager.ListPrompts());
    }

    void MainWindow::HandleSavePrompt(const nlohmann::json& payload)
    {
        auto id = payload.value("id", "");
        if (id.empty()) return;
        m_promptManager.SavePrompt(id, payload);
        PostMessageToWeb("PROMPT_SAVED", true);
    }

    void MainWindow::HandleDeletePrompt(const nlohmann::json& payload)
    {
        auto id = payload.value("id", "");
        m_promptManager.DeletePrompt(id);
        PostMessageToWeb("PROMPT_DELETED", true);
    }

    void MainWindow::HandleListPlugins()
    {
        auto plugins = m_pluginManager.ListPlugins();
        nlohmann::json arr = nlohmann::json::array();
        for (auto& p : plugins) {
            arr.push_back({
                {"id", p.manifest.id},
                {"name", p.manifest.name},
                {"version", p.manifest.version},
                {"author", p.manifest.author},
                {"description", p.manifest.description},
                {"enabled", p.state != Manifold::Core::Plugins::PluginState::Disabled},
                {"state", static_cast<int>(p.state)},
                {"error", p.errorMessage}
            });
        }
        PostMessageToWeb("PLUGINS_LIST", arr);
    }

    void MainWindow::HandleSetPluginEnabled(const nlohmann::json& payload)
    {
        auto pluginId = payload.value("pluginId", "");
        bool enabled = payload.value("enabled", false);
        m_pluginManager.SetEnabled(pluginId, enabled);

        // Update settings
        m_currentSettings.pluginEnabled[pluginId] = enabled;
        m_settingsManager.Save(m_currentSettings);

        HandleListPlugins();
    }
}
