# Manifold Desktop

Open-source multi-provider AI chat client for Windows.

![Mani0](https://github.com/user-attachments/assets/cb1539e6-4eab-4a73-9fba-d5fd8cc5a764)


[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform: Windows 10+](https://img.shields.io/badge/Platform-Windows%2010%2B-0078D4.svg)](#prerequisites)
[![Language: C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C.svg)](#architecture)
[![Version: 0.2.0](https://img.shields.io/badge/Version-0.2.0-green.svg)](https://github.com/gregorik/Manifold-Desktop/releases/tag/v0.2.0)

Manifold Desktop is a native Windows desktop client that lets you chat with multiple AI providers through a single unified interface. Instead of juggling separate tabs/apps for ChatGPT, Claude, Gemini, and local models, you use one app with one conversation history, one settings panel, and the ability to switch providers mid-conversation or compare them side-by-side.

> **Status:** Experimental. Feedback and contributions welcome.

*[This repository is a FOSS solution to a persistent problem in current LLM interface programming. 🟢 Currently available for B2B consulting and remote contract/Co-Dev integration (CET Timezone). [Contact form.](https://gregorigin.com/contact.html)]*

## Features

![Mani1](https://github.com/user-attachments/assets/0c7a7564-80c2-457a-8db2-c6f9522e1331)

- **Multi-provider support** -- Gemini, OpenAI, Anthropic, Ollama, and any OpenAI-compatible endpoint
- **Side-by-side model comparison** -- stream responses from two models in parallel
- **Integrated terminal** -- full ConPTY pseudo-console with ANSI rendering
- **MCP client** -- Model Context Protocol support with stdio and SSE transports
- **Prompt library** -- save and reuse system prompts across conversations
- **Session management** -- save, load, search, rename, export, and import chat sessions
- **Markdown export** -- export conversations as `.md` files
- **Cost tracking** -- per-conversation token usage and cost estimates
- **Plugin system** -- extend functionality via DLL plugins with virtual host registration
- **Secure API key storage** -- Windows Credential Manager (DPAPI) encryption
- **Onboarding** -- guided setup overlay when no API keys are configured
- **Window state persistence** -- remembers position, size, and maximized state across sessions
- **Keyboard shortcuts**
  - `Ctrl+N` -- new chat
  - `Ctrl+T` -- new terminal
  - `Ctrl+W` -- close tab
  - `Ctrl+F` -- search sessions
  - `Ctrl+,` -- settings
  - `Ctrl+K` -- focus sidebar search
  - `Ctrl+1..9` -- switch tabs

## What's New in v0.2.0

- **Ollama integration** -- chat with local models running on Ollama
- **Side-by-side comparison** -- compare responses from two providers in parallel
- **Prompt library** -- save, organize, and reuse system prompts
- **Session search** -- full-text search across all saved conversations
- **Markdown export** -- export any conversation to a `.md` file
- **Cost tracking** -- per-conversation token counts and cost estimates
- **Session export/import** -- portable JSON-based session backup and restore
- **Auto-update check** -- notifies you when a new version is available
- **Onboarding flow** -- first-run overlay guides API key configuration
- **Window state persistence** -- remembers window position, size, and maximized state
- **Inline session rename** -- double-click a session name to rename it
- **Token estimates** -- live token count below the input textarea
- **Loading indicators** -- visual feedback during AI response streaming
- **Toast notifications** -- non-intrusive status messages
- **Confirmation dialogs** -- safe tab close and session delete with confirmation prompts
- **Accessibility** -- ARIA attributes, focus-visible indicators, and improved keyboard navigation
- **Error recovery** -- inline error display with one-click retry

See the [full changelog](#changelog) below.

## Quick Start

### Prerequisites

- Windows 10 version 1903 or later (x64)
- [Visual Studio 2022](https://visualstudio.microsoft.com/) or later with:
  - **Desktop development with C++** workload
  - **Universal Windows Platform development** workload
  - Windows 10 SDK (10.0.19041.0 or later)
- [Windows App SDK 1.8](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/)

### Clone and build

```bash
git clone https://github.com/gregorik/Manifold-Desktop.git
cd Manifold-Desktop
```

Restore NuGet packages and build:

```bash
msbuild ManifoldDesktop.vcxproj /p:Configuration=Release /p:Platform=x64 /restore
```

Run the output from `x64\Release\ManifoldDesktop\`.

## Build from Source

### Debug build

```bash
msbuild ManifoldDesktop.vcxproj /p:Configuration=Debug /p:Platform=x64 /restore
```

### Release build

```bash
msbuild ManifoldDesktop.vcxproj /p:Configuration=Release /p:Platform=x64 /restore
```

The `/restore` flag triggers NuGet package restore automatically. If you prefer to restore separately:

```bash
nuget restore ManifoldDesktop.sln
msbuild ManifoldDesktop.vcxproj /p:Configuration=Release /p:Platform=x64
```

You can also open `ManifoldDesktop.sln` in Visual Studio, which handles NuGet restore on build.

## Architecture

Manifold Desktop is a two-layer application:

- **C++/WinRT backend** -- manages providers, terminal sessions, MCP connections, file I/O, and credential storage
- **WebView2 frontend** -- vanilla JavaScript (ES6 modules) with no framework or build step

The two layers communicate via a JSON message bridge (`postMessage` / `OnWebMessage`). Each provider implements the `IProvider` interface and is registered at startup through `ProviderRegistry`.

See [docs/architecture.md](docs/architecture.md) for a detailed breakdown.

## Extending

### Adding a Provider

1. Create a class that implements `Manifold::Core::IProvider` (see `Manifold.Core/Providers/IProvider.h`).
2. Implement all virtual methods: `Id()`, `DisplayName()`, `ListModels()`, `SendChat()`, `StreamChat()`, and `ValidateKey()`.
3. Register an instance in `MainWindow`'s constructor alongside the existing providers:

```cpp
m_providerRegistry.AddProvider(std::make_unique<YourProvider>());
```

4. Rebuild. The new provider will appear in the frontend's provider dropdown automatically.

### Adding an MCP Server

1. Open Settings (`Ctrl+,`) and navigate to the MCP section.
2. Add a server with either:
   - **Stdio transport** -- provide the command and arguments (e.g., `npx -y @modelcontextprotocol/server-filesystem /path`)
   - **SSE transport** -- provide the server URL (e.g., `http://localhost:8080/sse`)
3. Enable the server. Manifold will connect and discover available tools, which are then offered to AI providers that support tool use.

## Changelog

### v0.2.0 (2026-04-03)

**New features:**
- Ollama provider integration for local model support
- Side-by-side model comparison mode
- Prompt library with save/load/organize
- Full-text session search
- Markdown conversation export
- Per-conversation cost tracking and token estimates
- Session export/import (JSON format)
- Auto-update check on launch
- First-run onboarding overlay
- Window position/size/maximized state persistence
- Inline session rename (double-click)
- Live token estimate below input textarea
- Loading indicators during streaming
- Toast notification system
- Confirmation dialogs for destructive actions
- Accessibility improvements (ARIA, focus-visible, keyboard nav)
- Inline error display with retry

**Bug fixes:**
- Terminal theme variable consistency
- Tab transition animations
- Disabled button state styling

### v0.1.0 (2026-01-27)

- Initial release
- Multi-provider support (Gemini, OpenAI, Anthropic, OpenAI-compatible)
- Integrated ConPTY terminal
- MCP client (stdio + SSE)
- Plugin system
- Secure credential storage
- Session save/load

## Contributing

See [docs/contributing.md](docs/contributing.md) for setup instructions, code style, and PR guidelines.

## License

[MIT](LICENSE). Code by Andras Gregori @ GregOrigin.
