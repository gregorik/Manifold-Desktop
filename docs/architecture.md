# Architecture

## Overview

Manifold Desktop is a two-process WinUI 3 application:

1. **Native host** -- a C++/WinRT process (`ManifoldDesktop.exe`) that owns the window, manages all backend logic, and hosts a WebView2 control.
2. **WebView2 renderer** -- a Chromium-based process that runs the frontend UI as vanilla HTML/CSS/JavaScript.

The two layers communicate through a bidirectional JSON message bridge.

## Backend (`Manifold.Core/`)

### Provider System

All AI providers implement the `IProvider` interface (`Providers/IProvider.h`):

```
IProvider
  Id(), DisplayName(), ListModels()
  SendChat(request) -> ChatResponse
  StreamChat(request, onChunk) -> ChatResponse
  ValidateKey(apiKey) -> bool
```

Built-in providers:

| Class | Provider |
|---|---|
| `GeminiProvider` | Google Gemini |
| `OpenAIProvider` | OpenAI |
| `AnthropicProvider` | Anthropic Claude |
| `OpenAICompatProvider` | Ollama, LM Studio, or any OpenAI-compatible API |

`ProviderRegistry` stores provider instances and supports listing all providers and their models. Streaming chat runs on a dedicated `std::jthread`; the compare feature spawns one thread per slot (typically two).

### Provider Types

Defined in `Providers/ProviderTypes.h`:

- `ContentPart` -- text, image (base64), or file attachment
- `ChatMessage` -- role + parts, with optional tool call/result
- `ChatRequest` / `ChatResponse` -- the main request/response envelope
- `StreamChunk` -- incremental text or tool call during streaming
- `ToolDefinition`, `ToolCall`, `ToolResult` -- MCP tool integration types

### Terminal

The integrated terminal uses Windows ConPTY:

- `ProcessManager` -- creates a pseudo-console, spawns a shell process (e.g., `cmd.exe` or `pwsh.exe`), manages I/O pipes
- `TerminalEmulator` -- parses ANSI escape sequences and maintains a character grid with styles
- `TerminalTypes.h` -- color, style, and segment types

Multiple terminal sessions can run concurrently, each tracked by ID in `MainWindow::m_terminals`.

### Storage

- **`SettingsManager`** -- reads/writes `settings.json` in `%LOCALAPPDATA%\Manifold\`. Serialization uses `nlohmann::json` with ADL `to_json`/`from_json` overloads.
- **`SessionManager`** -- stores chat sessions as individual JSON files in `%LOCALAPPDATA%\Manifold\sessions\`. Supports listing, search (full-text over titles and content), save, load, delete, export, and import.
- **`PromptManager`** -- stores saved system prompts as JSON files in `%LOCALAPPDATA%\Manifold\prompts\`.
- **`CredentialManager`** -- wraps Windows Credential Manager (DPAPI) to store per-provider API keys securely. Keys never touch disk in plaintext.

### MCP (Model Context Protocol)

`Manifold.Core/MCP/` implements a client for the Model Context Protocol:

- `MCPTransport` -- abstract transport interface
- `StdioTransport` -- launches an MCP server process and communicates via stdin/stdout (JSON-RPC)
- `SSETransport` -- connects to an HTTP SSE endpoint
- `MCPClient` -- manages multiple server connections, aggregates tools from all connected servers, and routes `CallTool` requests to the correct server

When MCP tools are available, they are converted to `ToolDefinition` objects and passed to providers in `ChatRequest::tools`. Providers that support tool use (function calling) can invoke them, and the client handles the tool call round-trip loop (up to `maxToolCallRounds`).

### Plugins

`Manifold.Core/Plugins/` provides a DLL-based plugin system:

- `IPlugin` -- ABI-safe interface that plugins implement (`Id`, `Name`, `Version`, `Initialize`, `Shutdown`)
- `PluginContext` -- passed to plugins during initialization; provides access to host services
- `PluginManager` -- scans `%LOCALAPPDATA%\Manifold\plugins\*/plugin.json` for manifests, loads enabled plugins via `LoadLibrary`, and manages their lifecycle
- Plugins can register WebView2 virtual hosts (for custom UI) and custom providers via registrar callbacks

## Frontend (`frontend/`)

### Component Pattern

Each UI component is an ES6 module that exports a `create()` function. The function builds DOM elements, attaches event listeners, and returns an interface object. There is no virtual DOM, no framework, and no build step.

Components:

| Module | Purpose |
|---|---|
| `components/tab-bar.js` | Tab strip with add/close/select |
| `components/side-panel.js` | Session list, search, navigation |
| `components/input-bar.js` | Message input with provider/model selectors |
| `components/chat-tab.js` | Chat conversation view |
| `components/terminal-tab.js` | Terminal emulator display |
| `components/compare-tab.js` | Side-by-side model comparison |
| `components/settings-panel.js` | Settings overlay |
| `components/home-tab.js` | Landing page with quick actions |
| `components/search-overlay.js` | Full-text session search |
| `components/message-renderer.js` | Markdown rendering (marked.js + highlight.js) |

### Services

| Module | Purpose |
|---|---|
| `services/bridge.js` | WebView2 message bridge (`send`, `on`, `once`) |
| `services/settings-store.js` | Client-side settings cache |
| `services/provider-api.js` | Typed wrappers for provider-related bridge messages |
| `services/session-store.js` | Session list state management |
| `services/plugin-store.js` | Plugin list state management |
| `services/pricing.js` | Token cost calculation per model |
| `services/toast.js` | Toast notification display |
| `services/confirm.js` | Confirmation dialog |

### Vendor Libraries

- `vendor/marked.min.js` -- Markdown to HTML
- `vendor/highlight.min.js` -- Syntax highlighting in code blocks

## Message Bridge

The frontend and backend communicate through WebView2's `postMessage` API. All messages are JSON objects with a `type` field.

**Frontend to backend** (via `bridge.send`):

```json
{ "type": "CHAT_SEND", "providerId": "gemini", "modelId": "gemini-2.0-flash", ... }
```

**Backend to frontend** (via `PostMessageToWeb`):

```json
{ "type": "CHAT_CHUNK", "payload": { "text": "Hello", "done": false } }
```

`MainWindow::OnWebMessage` dispatches incoming messages to handler methods based on the `type` field. Handler methods follow the naming convention `Handle<PascalCaseType>` (e.g., `CHAT_SEND` dispatches to `HandleChatSend`).

### Message categories

| Category | Frontend -> Backend | Backend -> Frontend |
|---|---|---|
| Chat | `CHAT_SEND`, `CHAT_CANCEL` | `CHAT_CHUNK`, `CHAT_DONE`, `CHAT_ERROR`, `CHAT_CANCELLED` |
| Compare | `COMPARE_CHAT`, `COMPARE_CANCEL` | (per-slot chunk/done/error messages) |
| Terminal | `TERMINAL_START`, `TERMINAL_INPUT`, `TERMINAL_STOP`, `TERMINAL_RESIZE` | `TERMINAL_OUTPUT` |
| Settings | `GET_SETTINGS`, `SAVE_SETTINGS`, `SET_API_KEY` | `SETTINGS`, `SETTINGS_SAVED` |
| Sessions | `LIST_SESSIONS`, `LOAD_SESSION`, `SAVE_SESSION`, `DELETE_SESSION`, `SEARCH_SESSIONS` | `SESSIONS`, `SESSION_DATA`, etc. |
| MCP | `LIST_MCP_SERVERS`, `ADD_MCP_SERVER`, `REMOVE_MCP_SERVER`, `LIST_MCP_TOOLS` | `MCP_SERVERS`, `MCP_TOOLS` |
| Prompts | `LIST_PROMPTS`, `SAVE_PROMPT`, `DELETE_PROMPT` | `PROMPTS` |
| Providers | `LIST_PROVIDERS`, `LIST_MODELS`, `VALIDATE_KEY` | `PROVIDERS`, `MODELS`, `KEY_VALID` |
| Plugins | `LIST_PLUGINS`, `SET_PLUGIN_ENABLED` | `PLUGINS` |

## Data Flow

A typical chat interaction:

1. User types a message and presses Enter.
2. `input-bar.js` fires the `onSend` callback with the text, provider ID, and model ID.
3. `app.js` calls `providerApi.sendChat(...)`, which calls `bridge.send('CHAT_SEND', ...)`.
4. WebView2 delivers the JSON string to `MainWindow::OnWebMessage`.
5. The dispatcher calls `HandleChatSend`, which:
   - Retrieves the API key from `CredentialManager`
   - Builds a `ChatRequest` (including MCP tools if available)
   - Spawns a `std::jthread` that calls `provider->StreamChat(request, onChunk)`
6. Each `StreamChunk` callback marshals to the UI thread via `DispatcherQueue` and calls `PostMessageToWeb("CHAT_CHUNK", ...)`.
7. `bridge.js` receives each chunk and dispatches to listeners registered by `chat-tab.js`.
8. `chat-tab.js` appends streamed text to the message renderer, which renders Markdown in real time.
9. When the provider signals `done`, the backend sends `CHAT_DONE` with final token counts.

If the provider returns a tool call, the backend executes it via `MCPClient::CallTool`, appends the result to the message history, and re-sends to the provider -- repeating until the model produces a final text response or the round limit is reached.
